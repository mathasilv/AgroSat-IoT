/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO OTIMIZADA PARA BALÃO METEOROLÓGICO (HAB)
 * @version 7.0.0 - HAB Edition
 * @date 2025-11-17
 * 
 * CHANGELOG v7.0.0 HAB:
 * - [REMOVED] Lógica de passagens orbitais (não aplicável para balão)
 * - [REMOVED] Detecção de órbita LEO (movimento lento de balão)
 * - [NEW] Modo de coleta contínua simplificado
 * - [OPT] Forward imediato ao receber dados (sem esperar "próxima passagem")
 * - [OPT] SF7 otimizado para line-of-sight balão (31km altitude)
 */
#include "TelemetryManager.h"
#include "config.h"

bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _displayMgr(), 
    _mode(MODE_INIT),
    _lastTelemetrySend(0),
    _lastStorageSave(0),
    _missionActive(false),
    _missionStartTime(0),
    _lastDisplayUpdate(0),
    _lastHeapCheck(0),
    _minHeapSeen(UINT32_MAX),
    _useNewDisplay(true)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    memset(&_groundNodeBuffer, 0, sizeof(GroundNodeBuffer));
    
    _groundNodeBuffer.activeNodes = 0;
    _groundNodeBuffer.totalPacketsCollected = 0;
    
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        _groundNodeBuffer.lastUpdate[i] = 0;
        _groundNodeBuffer.nodes[i].forwarded = false;
    }
    
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
}

bool TelemetryManager::_initI2CBus() {
    static bool i2cInitialized = false;
    
    if (!i2cInitialized) {
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("[TelemetryManager] Inicializando barramento I2C...");
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000);
        Wire.setTimeOut(200);
        
        delay(300);
        i2cInitialized = true;
        
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado (100 kHz, timeout 200ms)");
        DEBUG_PRINTLN("[TelemetryManager] Dispositivos no barramento:");
        
        uint8_t devicesFound = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                DEBUG_PRINTF("[TelemetryManager]   - 0x%02X\n", addr);
                devicesFound++;
            }
        }
        
        if (devicesFound == 0) {
            DEBUG_PRINTLN("[TelemetryManager] Nenhum dispositivo I2C encontrado!");
        } else {
            DEBUG_PRINTF("[TelemetryManager] Total: %d dispositivo(s) I2C\n", devicesFound);
        }
        
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("");
    }
    
    return true;
}

void TelemetryManager::applyModeConfig(OperationMode mode) {
    switch(mode) {
        case MODE_PREFLIGHT: 
            activeModeConfig = &PREFLIGHT_CONFIG; 
            break;
        case MODE_FLIGHT: 
            activeModeConfig = &FLIGHT_CONFIG; 
            break;
        case MODE_SAFE:
            activeModeConfig = &SAFE_CONFIG;
            break;
        default: 
            activeModeConfig = &PREFLIGHT_CONFIG; 
            break;
    }
    
    currentSerialLogsEnabled = activeModeConfig->serialLogsEnabled;
    
    if (_useNewDisplay) {
        if (activeModeConfig->displayEnabled) {
            _displayMgr.turnOn();
        } else {
            _displayMgr.turnOff();
        }
    }
    
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    DEBUG_PRINTF("[TelemetryManager] Modo aplicado: %d | Display: %s | Logs: %s | LoRa: %s\n",
                 mode,
                 activeModeConfig->displayEnabled ? "ON" : "OFF",
                 activeModeConfig->serialLogsEnabled ? "ON" : "OFF",
                 activeModeConfig->loraEnabled ? "ON" : "OFF");
}

bool TelemetryManager::begin() {
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT HAB - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION " HAB");
    DEBUG_PRINTLN("  Balão Meteorológico - Coleta Contínua");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");

    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initI2CBus();

    // ========== DISPLAY MANAGER - INICIALIZAR PRIMEIRO ==========
    bool displayOk = false;
    
    if (activeModeConfig->displayEnabled) {
        DEBUG_PRINTLN("[TelemetryManager] Inicializando DisplayManager...");

        esp_task_wdt_reset();
        
        if (_displayMgr.begin()) {
            _useNewDisplay = true;
            displayOk = true;
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager OK");
            
            esp_task_wdt_reset();
            
            _displayMgr.showBoot();
            delay(2000);
        } else {
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager FAILED (não-crítico)");
            _useNewDisplay = false;
        }
        
        delay(500);
    }

    bool success = true;
    uint8_t subsystemsOk = 0;

    // RTC MANAGER
    DEBUG_PRINTLN("[TelemetryManager] Inicializando RTC Manager...");
    esp_task_wdt_reset();
    
    if (_rtc.begin(&Wire)) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] RTC Manager OK");
        DEBUG_PRINTF("[TelemetryManager] RTC: %s\n", _rtc.getDateTime().c_str());
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("RTC", true);
            delay(500);
        }
    } else {
        DEBUG_PRINTLN("[TelemetryManager] RTC Manager FAILED (não-crítico)");
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("RTC", false);
            delay(500);
        }
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando gerenciador de botão...");
    _button.begin();

    // SYSTEM HEALTH
    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    esp_task_wdt_reset();
    
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] System Health OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Health", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] System Health FAILED");
    }

    // POWER MANAGER
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    esp_task_wdt_reset();
    
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Power", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager FAILED");
    }

    // ========== SENSOR MANAGER COM FEEDBACK VISUAL ==========
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    esp_task_wdt_reset();
    
    // ✅ MOSTRAR MENSAGEM DE CALIBRAÇÃO NO DISPLAY (ANTES DO BEGIN)
    if (_useNewDisplay && displayOk) {
        _displayMgr.displayMessage("CALIBRACAO", "Rotacione IMU...");
        delay(1000);
    }
    
    // ✅ CHAMAR begin() SEM PARÂMETROS (como está originalmente)
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        // ✅ MOSTRAR RESULTADO DA CALIBRAÇÃO NO DISPLAY (APÓS O BEGIN)
        if (_useNewDisplay && displayOk) {
            // A calibração já aconteceu dentro do begin()
            _displayMgr.displayMessage("CALIBRADO!", "MPU9250 OK");
            delay(1500);
            
            // Mostrar status de cada sensor
            _displayMgr.showSensorInit("MPU9250", _sensors.isMPU9250Online());
            delay(500);
            _displayMgr.showSensorInit("BMP280", _sensors.isBMP280Online());
            delay(500);
            _displayMgr.showSensorInit("SI7021", _sensors.isSI7021Online());
            delay(500);
            _displayMgr.showSensorInit("CCS811", _sensors.isCCS811Online());
            delay(1000);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager FAILED");
    }

    // STORAGE MANAGER
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
    esp_task_wdt_reset();
    
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Storage Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Storage", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Storage Manager FAILED");
    }
    
    // COMMUNICATION MANAGER
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    DEBUG_PRINTLN("[TelemetryManager]   - LoRa RX/TX Contínuo (HAB Mode)");
    DEBUG_PRINTLN("[TelemetryManager]   - Antena Direcional Terrestre");
    esp_task_wdt_reset();
    
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("LoRa", _comm.isLoRaOnline());
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager FAILED");
    }

    if (_useNewDisplay && displayOk) {
        _displayMgr.showReady();
        delay(2000);
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("Sistema inicializado: %s\n", success ? "OK" : "COM ERROS");
    DEBUG_PRINTF("Subsistemas online: %d/6\n", subsystemsOk);
    
    uint32_t postInitHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("Heap disponível: %lu bytes\n", postInitHeap);
    DEBUG_PRINTF("Heap usado: %lu bytes\n", initialHeap - postInitHeap);
    
    if (postInitHeap < _minHeapSeen) {
        _minHeapSeen = postInitHeap;
    }
    
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    esp_task_wdt_reset();
    
    return success;
}


// ========== LOOP SIMPLIFICADO PARA BALÃO ==========
void TelemetryManager::loop() {
    uint32_t currentTime = millis();
    
    // Atualizar subsistemas básicos
    _health.update(); 
    delay(5);
    _power.update(); 
    delay(5);
    _sensors.update(); 
    delay(10);
    _comm.update(); 
    delay(5);
    _handleButtonEvents();
    delay(5);

    
    // ========== RECEPÇÃO E PROCESSAMENTO DE PACOTES LORA ==========
    String loraPacket;
    int rssi;
    float snr;
    
    if (_comm.receiveLoRaPacket(loraPacket, rssi, snr)) {
        DEBUG_PRINTLN("[TelemetryManager] ━━━━━ PACOTE LORA RECEBIDO ━━━━━");
        DEBUG_PRINTF("[TelemetryManager] Raw: %s\n", loraPacket.c_str());
        DEBUG_PRINTF("[TelemetryManager] RSSI: %d dBm, SNR: %.1f dB\n", rssi, snr);
        
        MissionData receivedData;
        memset(&receivedData, 0, sizeof(MissionData));
        
        if (_comm.processLoRaPacket(loraPacket, receivedData)) {
            receivedData.rssi = rssi;
            receivedData.snr = snr;
            receivedData.lastLoraRx = millis();
            
            if (_rtc.isInitialized()) {
                receivedData.collectionTime = _rtc.getUnixTime();
            } else {
                receivedData.collectionTime = millis() / 1000;
            }
            
            receivedData.forwarded = false;
            
            _updateGroundNode(receivedData);
            
            DEBUG_PRINTLN("[TelemetryManager] Payload processado!");
            DEBUG_PRINTF("[TelemetryManager]   Node ID: %u | Seq: %u\n", 
                         receivedData.nodeId, receivedData.sequenceNumber);
            DEBUG_PRINTF("[TelemetryManager]   Umidade Solo: %.1f%% | Temp: %.1f°C\n", 
                         receivedData.soilMoisture, receivedData.ambientTemp);
            DEBUG_PRINTF("[TelemetryManager]   Umidade Ar: %.1f%% | Irrigação: %d\n", 
                         receivedData.humidity, receivedData.irrigationStatus);
            DEBUG_PRINTF("[TelemetryManager]   RSSI: %d dBm | SNR: %.1f dB\n", rssi, snr);
            DEBUG_PRINTF("[TelemetryManager]   Prioridade: %d | RX Total: %u | Perdidos: %u\n",
                         receivedData.priority, receivedData.packetsReceived, receivedData.packetsLost);
            
            // Salvar no SD imediatamente
            if (_storage.isAvailable()) {
                if (_storage.saveMissionData(receivedData)) {
                    DEBUG_PRINTLN("[TelemetryManager] Dados salvos no SD");
                }
            }
            
            // Atualizar display
            if (_useNewDisplay && _displayMgr.isOn()) {
                char nodeInfo[32];
                snprintf(nodeInfo, sizeof(nodeInfo), "N%u:%.0f%% %ddBm", 
                        receivedData.nodeId, receivedData.soilMoisture, rssi);
                _displayMgr.displayMessage("LORA RX", nodeInfo);
            }
            
        } else {
            DEBUG_PRINTLN("[TelemetryManager] Falha ao processar payload (checksum inválido)");
        }
    }
    
    static unsigned long lastCleanup = 0;
    if (currentTime - lastCleanup > 600000) { // A cada 10 minutos
        lastCleanup = currentTime;
        _cleanupStaleNodes(NODE_TTL_MS); // 30 minutos de TTL
        
        DEBUG_PRINTLN("[TelemetryManager] ━━━━━ STATUS DO BUFFER ━━━━━");
        DEBUG_PRINTF("[TelemetryManager] Nós ativos: %d/%d\n", 
                     _groundNodeBuffer.activeNodes, MAX_GROUND_NODES);
        DEBUG_PRINTF("[TelemetryManager] Pacotes coletados: %u\n", 
                     _groundNodeBuffer.totalPacketsCollected);
    }
    
    // Coletar telemetria do balão
    _collectTelemetryData();
    
    // Verificar condições operacionais
    _checkOperationalConditions();
    
    // ========== TRANSMISSÃO DE TELEMETRIA ==========
    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        
        DEBUG_PRINTLN("[TelemetryManager] ╔════════════════════════════════════╗");
        DEBUG_PRINTLN("[TelemetryManager] ║  TRANSMISSÃO DE TELEMETRIA         ║");
        DEBUG_PRINTLN("[TelemetryManager] ╚════════════════════════════════════╝");
        
        if (_groundNodeBuffer.activeNodes > 0) {
            std::lock_guard<std::mutex> lock(_bufferMutex);
            DEBUG_PRINTF("[TelemetryManager] Buffer: %d nós ativos\n", _groundNodeBuffer.activeNodes);
            
            for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
                DEBUG_PRINTF("[TelemetryManager]   [%d] Node %u: seq=%u, fwd=%s, pri=%d, rssi=%d\n",
                            i,
                            _groundNodeBuffer.nodes[i].nodeId,
                            _groundNodeBuffer.nodes[i].sequenceNumber,
                            _groundNodeBuffer.nodes[i].forwarded ? "Y" : "N",
                            _groundNodeBuffer.nodes[i].priority,
                            _groundNodeBuffer.nodes[i].rssi);
            }
        }
        
        _sendTelemetry();
        
        DEBUG_PRINTLN("[TelemetryManager] ════════════════════════════════════");
    }
    
    // Salvar no SD periodicamente
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // Atualizar display
    if (activeModeConfig->displayEnabled && _useNewDisplay) {
        _displayMgr.updateTelemetry(_telemetryData);
    }
    
    // ========== MONITORAMENTO DE HEAP ==========
    static unsigned long lastHeapCheck = 0;
    if (currentTime - lastHeapCheck >= 30000) {
        lastHeapCheck = currentTime;
        uint32_t currentHeap = ESP.getFreeHeap();
        
        if (currentHeap < _minHeapSeen) {
            _minHeapSeen = currentHeap;
        }
        
        DEBUG_PRINTF("[TelemetryManager] Heap: %lu KB (mín: %lu KB)\n",
                     currentHeap / 1024, _minHeapSeen / 1024);
        
        if (currentHeap < 15000) {
            DEBUG_PRINTLN("[TelemetryManager] ALERTA: Heap baixo!");
            _health.reportError(STATUS_WATCHDOG, "Low memory warning");
        }
        
        if (currentHeap < 8000) {
            DEBUG_PRINTLN("[TelemetryManager] CRÍTICO: Entrando em SAFE MODE!");
            applyModeConfig(MODE_SAFE);
        }
    }
    
    delay(20);
}

void TelemetryManager::_updateGroundNode(const MissionData& data) {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    
    int existingIndex = -1;
    
    // Procurar se nó já existe no buffer
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        if (_groundNodeBuffer.nodes[i].nodeId == data.nodeId) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex >= 0) {
        // Nó já existe - atualizar
        MissionData& existingNode = _groundNodeBuffer.nodes[existingIndex];
        
        if (data.sequenceNumber > existingNode.sequenceNumber) {
            DEBUG_PRINTF("[TelemetryManager] Node %u ATUALIZADO (seq %u → %u)\n", 
                         data.nodeId, existingNode.sequenceNumber, data.sequenceNumber);
            
            // Detectar pacotes perdidos
            if (data.sequenceNumber > existingNode.sequenceNumber + 1) {
                uint16_t lost = data.sequenceNumber - existingNode.sequenceNumber - 1;
                DEBUG_PRINTF("[TelemetryManager] %u pacote(s) perdido(s)\n", lost);
            }
            
            existingNode = data;
            existingNode.lastLoraRx = millis();
            existingNode.forwarded = false; 
            existingNode.priority = _comm.calculatePriority(data);
            
            _groundNodeBuffer.lastUpdate[existingIndex] = millis();
            _groundNodeBuffer.totalPacketsCollected++;
            
        } else if (data.sequenceNumber == existingNode.sequenceNumber) {
            DEBUG_PRINTF("[TelemetryManager] Node %u duplicado (seq %u), ignorando\n",
                         data.nodeId, data.sequenceNumber);
            return;
        } else {
            DEBUG_PRINTF("[TelemetryManager] Node %u pacote antigo (seq %u < %u), ignorando\n",
                         data.nodeId, data.sequenceNumber, existingNode.sequenceNumber);
            return;
        }
        
    } else {
        // Nó novo
        if (_groundNodeBuffer.activeNodes < MAX_GROUND_NODES) {
            uint8_t newIndex = _groundNodeBuffer.activeNodes;
            _groundNodeBuffer.nodes[newIndex] = data;
            _groundNodeBuffer.nodes[newIndex].lastLoraRx = millis();
            _groundNodeBuffer.nodes[newIndex].forwarded = false;
            _groundNodeBuffer.nodes[newIndex].priority = _comm.calculatePriority(data);
            
            _groundNodeBuffer.lastUpdate[newIndex] = millis();
            _groundNodeBuffer.activeNodes++;
            _groundNodeBuffer.totalPacketsCollected++;
            
            DEBUG_PRINTF("[TelemetryManager] Node %u NOVO (slot %d) | Total: %d/%d\n", 
                         data.nodeId, newIndex, _groundNodeBuffer.activeNodes, MAX_GROUND_NODES);
        } else {
            // Buffer cheio - substituir nó de menor prioridade
            _replaceLowestPriorityNode(data);
        }
    }
}

void TelemetryManager::_replaceLowestPriorityNode(const MissionData& newData) {
    uint8_t replaceIndex = 0;
    uint8_t lowestPriority = 255;
    unsigned long oldestTime = ULONG_MAX;
    
    for (uint8_t i = 0; i < MAX_GROUND_NODES; i++) {
        if (_groundNodeBuffer.nodes[i].priority < lowestPriority ||
            (_groundNodeBuffer.nodes[i].priority == lowestPriority && 
             _groundNodeBuffer.lastUpdate[i] < oldestTime)) {
            
            lowestPriority = _groundNodeBuffer.nodes[i].priority;
            oldestTime = _groundNodeBuffer.lastUpdate[i];
            replaceIndex = i;
        }
    }
    
    DEBUG_PRINTF("[TelemetryManager] Buffer cheio! Substituindo Node %u (pri=%d) por Node %u (pri=%d)\n",
                 _groundNodeBuffer.nodes[replaceIndex].nodeId, 
                 _groundNodeBuffer.nodes[replaceIndex].priority,
                 newData.nodeId,
                 newData.priority);
    
    _groundNodeBuffer.nodes[replaceIndex] = newData;
    _groundNodeBuffer.nodes[replaceIndex].lastLoraRx = millis();
    _groundNodeBuffer.nodes[replaceIndex].forwarded = false;
    _groundNodeBuffer.lastUpdate[replaceIndex] = millis();
}

void TelemetryManager::_cleanupStaleNodes(unsigned long maxAge) {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    
    unsigned long now = millis();
    uint8_t removedCount = 0;
    
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        unsigned long age = now - _groundNodeBuffer.lastUpdate[i];
        
        if (age > maxAge) {
            DEBUG_PRINTF("[TelemetryManager] Removendo Node %u (inativo há %lu min)\n",
                         _groundNodeBuffer.nodes[i].nodeId, age / 60000);
            
            // Compactar buffer
            for (uint8_t j = i; j < _groundNodeBuffer.activeNodes - 1; j++) {
                _groundNodeBuffer.nodes[j] = _groundNodeBuffer.nodes[j + 1];
                _groundNodeBuffer.lastUpdate[j] = _groundNodeBuffer.lastUpdate[j + 1];
            }
            
            _groundNodeBuffer.activeNodes--;
            removedCount++;
            i--;
        }
    }
    
    if (removedCount > 0) {
        DEBUG_PRINTF("[TelemetryManager] Limpeza concluída: %d nó(s) removido(s)\n", removedCount);
    }
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT || _mode == MODE_POSTFLIGHT) {
        DEBUG_PRINTLN("[TelemetryManager] Missão já em andamento!");
        return;
    }
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO HAB");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Início: %s\n", _rtc.getDateTime().c_str());
    }
    
    if (_useNewDisplay && _displayMgr.isOn()) {
        _displayMgr.displayMessage("MISSAO HAB", "INICIADA");
        delay(2000);
    }
    
    _mode = MODE_FLIGHT;
    _missionActive = true;
    _missionStartTime = millis();
    
    applyModeConfig(MODE_FLIGHT);
    
    DEBUG_PRINTLN("[TelemetryManager] Modo FLIGHT ativado");
    DEBUG_PRINTLN("[TelemetryManager] Coleta contínua de dados terrestres");
    DEBUG_PRINTLN("[TelemetryManager] Missão iniciada!");
}

void TelemetryManager::stopMission() {
    if (!_missionActive) {
        DEBUG_PRINTLN("[TelemetryManager] Nenhuma missão ativa!");
        return;
    }
    
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] ENCERRANDO MISSÃO HAB");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    uint32_t missionDuration = millis() - _missionStartTime;
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Fim: %s\n", _rtc.getDateTime().c_str());
    }
    
    DEBUG_PRINTF("[TelemetryManager] Duração total: %lu min %lu seg\n", 
                 missionDuration / 60000, (missionDuration % 60000) / 1000);
    DEBUG_PRINTF("[TelemetryManager] Nós diferentes coletados: %u\n", _groundNodeBuffer.activeNodes);
    DEBUG_PRINTF("[TelemetryManager] Total de pacotes recebidos: %u\n", _groundNodeBuffer.totalPacketsCollected);
    
    if (_groundNodeBuffer.activeNodes > 0) {
        int avgRSSI = 0;
        float avgSNR = 0.0;
        int bestRSSI = -200;
        int worstRSSI = 0;
        uint16_t totalLost = 0;
        uint16_t totalReceived = 0;
        
        for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
            avgRSSI += _groundNodeBuffer.nodes[i].rssi;
            avgSNR += _groundNodeBuffer.nodes[i].snr;
            
            if (_groundNodeBuffer.nodes[i].rssi > bestRSSI) {
                bestRSSI = _groundNodeBuffer.nodes[i].rssi;
            }
            if (_groundNodeBuffer.nodes[i].rssi < worstRSSI) {
                worstRSSI = _groundNodeBuffer.nodes[i].rssi;
            }
            
            totalLost += _groundNodeBuffer.nodes[i].packetsLost;
            totalReceived += _groundNodeBuffer.nodes[i].packetsReceived;
        }
        
        avgRSSI /= _groundNodeBuffer.activeNodes;
        avgSNR /= _groundNodeBuffer.activeNodes;
        
        float packetLossRate = (totalReceived > 0) ? 
                               (totalLost * 100.0) / (totalReceived + totalLost) : 0.0;
        
        DEBUG_PRINTLN("[TelemetryManager] ━━━━━ ESTATÍSTICAS DO LINK ━━━━━");
        DEBUG_PRINTF("[TelemetryManager] RSSI médio: %d dBm (melhor: %d, pior: %d)\n",
                     avgRSSI, bestRSSI, worstRSSI);
        DEBUG_PRINTF("[TelemetryManager] SNR médio: %.1f dB\n", avgSNR);
        DEBUG_PRINTF("[TelemetryManager] Taxa de perda: %.2f%% (%u/%u pacotes)\n",
                     packetLossRate, totalLost, totalReceived + totalLost);
        DEBUG_PRINTLN("[TelemetryManager] ════════════════════════════════");
    }
    
    _mode = MODE_POSTFLIGHT;
    _missionActive = false;
    
    if (_useNewDisplay) {
        char durationStr[32];
        snprintf(durationStr, sizeof(durationStr), "%lum%lus", 
                 missionDuration / 60000, (missionDuration % 60000) / 1000);
        _displayMgr.displayMessage("MISSAO COMPLETA", durationStr);
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Missão HAB encerrada com sucesso!");
}


OperationMode TelemetryManager::getMode() {
    return _mode;
}

void TelemetryManager::_collectTelemetryData() {
    if (_rtc.isInitialized()) {
        _telemetryData.timestamp = _rtc.getUnixTime();
    } else {
        _telemetryData.timestamp = millis();
    }
    
    _telemetryData.missionTime = _health.getMissionTime();
    _telemetryData.batteryVoltage = _power.getVoltage();
    _telemetryData.batteryPercentage = _power.getPercentage();
    
    _telemetryData.temperature = _sensors.getTemperature();           // Temperatura com fallback automático
    _telemetryData.temperatureBMP = _sensors.getTemperatureBMP280();  // ← ADICIONADO: BMP280 RAW
    _telemetryData.temperatureSI = NAN;                               // Será preenchido abaixo se SI7021 estiver online
    
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
    // Inicializar sensores opcionais como NAN
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    // SI7021 (Temperatura + Umidade)
    if (_sensors.isSI7021Online()) {
        float tempSI = _sensors.getTemperatureSI7021();
        if (!isnan(tempSI) && tempSI >= TEMP_MIN_VALID && tempSI <= TEMP_MAX_VALID) {
            _telemetryData.temperatureSI = tempSI;
        }
        
        float hum = _sensors.getHumidity();
        if (!isnan(hum) && hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
            _telemetryData.humidity = hum;
        }
    }
    
    // CCS811 (CO2 + TVOC)
    if (_sensors.isCCS811Online()) {
        float co2 = _sensors.getCO2();
        float tvoc = _sensors.getTVOC();
        
        if (!isnan(co2) && co2 >= CO2_MIN_VALID && co2 <= CO2_MAX_VALID) {
            _telemetryData.co2 = co2;
        }
        
        if (!isnan(tvoc) && tvoc >= TVOC_MIN_VALID && tvoc <= TVOC_MAX_VALID) {
            _telemetryData.tvoc = tvoc;
        }
    }
    
    // MPU9250 (Magnetômetro)
    if (_sensors.isMPU9250Online()) {
        float magX = _sensors.getMagX();
        float magY = _sensors.getMagY();
        float magZ = _sensors.getMagZ();
        
        if (!isnan(magX) && !isnan(magY) && !isnan(magZ)) {
            if (magX >= MAG_MIN_VALID && magX <= MAG_MAX_VALID &&
                magY >= MAG_MIN_VALID && magY <= MAG_MAX_VALID &&
                magZ >= MAG_MIN_VALID && magZ <= MAG_MAX_VALID) {
                _telemetryData.magX = magX;
                _telemetryData.magY = magY;
                _telemetryData.magZ = magZ;
            }
        }
    }
    
    _telemetryData.systemStatus = _health.getSystemStatus();
    _telemetryData.errorCount = _health.getErrorCount();
    
    String payloadStr = _comm.generateConsolidatedPayload(_groundNodeBuffer);
    strncpy(_telemetryData.payload, payloadStr.c_str(), PAYLOAD_MAX_SIZE - 1);
    _telemetryData.payload[PAYLOAD_MAX_SIZE - 1] = '\0';
}


void TelemetryManager::_sendTelemetry() {
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF("[TelemetryManager] Enviando telemetria [%s]...\n", 
                     _rtc.getDateTime().c_str());
        
        // ✅ CORRIGIDO: Usar valores separados
        bool hasBMP = !isnan(_telemetryData.temperatureBMP);
        bool hasSI = !isnan(_telemetryData.temperatureSI);
        
        if (hasBMP && hasSI) {
            // Ambos sensores funcionando - mostrar delta
            float delta = abs(_telemetryData.temperatureBMP - _telemetryData.temperatureSI);
            DEBUG_PRINTF("Temp BMP280: %.2f°C | SI7021: %.2f°C | Δ: %.2f°C\n",
                         _telemetryData.temperatureBMP,
                         _telemetryData.temperatureSI,
                         delta);
        } else if (hasBMP && !hasSI) {
            // Só BMP280 funcionando
            DEBUG_PRINTF("Temp BMP280: %.2f°C (SI7021 indisponível)\n", 
                         _telemetryData.temperatureBMP);
        } else if (!hasBMP && hasSI) {
            // Só SI7021 funcionando
            DEBUG_PRINTF("Temp SI7021: %.2f°C (BMP280 indisponível)\n", 
                         _telemetryData.temperatureSI);
        } else {
            // Nenhum sensor funcionando
            DEBUG_PRINTLN("Temperatura: INDISPONÍVEL");
        }
        
        DEBUG_PRINTF("  Pressão: %.2f hPa | Altitude: %.1f m\n", 
                     _telemetryData.pressure, 
                     _telemetryData.altitude);
        
        DEBUG_PRINTF("  Bateria: %.1f%% (%.2fV)\n",
                     _telemetryData.batteryPercentage,
                     _telemetryData.batteryVoltage);
        
        if (_groundNodeBuffer.activeNodes > 0) {
            DEBUG_PRINTF("  Nós terrestres ativos: %d\n", _groundNodeBuffer.activeNodes);
        }
    }
    
    bool sendSuccess = _comm.sendTelemetry(_telemetryData, _groundNodeBuffer);
    
    if (sendSuccess) {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada!");
        }
    } else {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTLN("[TelemetryManager] ERRO ao enviar telemetria");
        }
    }
}

void TelemetryManager::_saveToStorage() {
    if (!_storage.isAvailable()) return;
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Salvando dados [%s]...\n", _rtc.getDateTime().c_str());
    } else {
        DEBUG_PRINTLN("[TelemetryManager] Salvando dados...");
    }
    
    if (_storage.saveTelemetry(_telemetryData)) { 
        DEBUG_PRINTLN("[TelemetryManager] Telemetria salva no SD"); 
    }
    
    // Salvar dados de todos os nós terrestres
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        _storage.saveMissionData(_groundNodeBuffer.nodes[i]);
    }
}

void TelemetryManager::_checkOperationalConditions() {
    if (_power.isCritical()) { 
        _health.reportError(STATUS_BATTERY_CRIT, "Critical battery level"); 
        _power.enablePowerSave(); 
    } else if (_power.isLow()) { 
        _health.reportError(STATUS_BATTERY_LOW, "Low battery level"); 
    }
    
    if (!_sensors.isMPU6050Online() && !_sensors.isMPU9250Online()) { 
        _health.reportError(STATUS_SENSOR_ERROR, "IMU offline"); 
        _sensors.resetAll(); 
    }
    
    if (!_sensors.isBMP280Online()) { 
        _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline"); 
        _sensors.resetAll(); 
    }
    
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < 10000) { 
        DEBUG_PRINTF("[TelemetryManager] CRÍTICO: Heap baixo: %lu bytes\n", currentHeap); 
        _health.reportError(STATUS_WATCHDOG, "Critical low memory"); 
    }
}

void TelemetryManager::_logHeapUsage(const String& component) {
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < _minHeapSeen) { _minHeapSeen = currentHeap; }
    DEBUG_PRINTF("[TelemetryManager] %s - Heap: %lu bytes\n", component.c_str(), currentHeap);
}

void TelemetryManager::_monitorHeap() {
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < _minHeapSeen) { _minHeapSeen = currentHeap; }
    DEBUG_PRINTF("[TelemetryManager] Heap: %lu KB, Min: %lu KB\n",
        currentHeap / 1024, _minHeapSeen / 1024);
}

void TelemetryManager::testLoRaTransmission() {
    DEBUG_PRINTLN("[TelemetryManager] Testando transmissão LoRa...");
    _comm.sendLoRa("TEST_AGROSAT_HAB");
}

void TelemetryManager::sendCustomLoRa(const String& message) {
    _comm.sendLoRa(message);
}

void TelemetryManager::printLoRaStats() {
    uint16_t sent, failed;
    _comm.getLoRaStatistics(sent, failed);
    DEBUG_PRINTF("[TelemetryManager] LoRa Stats: %d enviados, %d falhas\n", sent, failed);
}

void TelemetryManager::_handleButtonEvents() {
    ButtonEvent event = _button.update();
    
    if (event == ButtonEvent::SHORT_PRESS) {
        // ========== TOGGLE PREFLIGHT ↔ FLIGHT ==========
        if (_mode == MODE_PREFLIGHT) {
            DEBUG_PRINTLN("");
            DEBUG_PRINTLN("╔════════════════════════════════════════╗");
            DEBUG_PRINTLN("║   BOTÃO PRESSIONADO: START MISSION     ║");
            DEBUG_PRINTLN("║   PREFLIGHT → FLIGHT                   ║");
            DEBUG_PRINTLN("╚════════════════════════════════════════╝");
            DEBUG_PRINTLN("");
            
            // Feedback visual LED
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(100);
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
            }
            
            startMission();
        } 
        else if (_mode == MODE_FLIGHT) {
            DEBUG_PRINTLN("");
            DEBUG_PRINTLN("╔════════════════════════════════════════╗");
            DEBUG_PRINTLN("║   BOTÃO PRESSIONADO: STOP MISSION      ║");
            DEBUG_PRINTLN("║   FLIGHT → PREFLIGHT                   ║");
            DEBUG_PRINTLN("╚════════════════════════════════════════╝");
            DEBUG_PRINTLN("");
            
            // Feedback visual LED
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(100);
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
            }
            
            stopMission();
        }
        else {
            DEBUG_PRINTF("[TelemetryManager] Modo atual (%d) não permite toggle via botão\n", _mode);
        }
    }
    else if (event == ButtonEvent::LONG_PRESS) {
        // ========== LONG PRESS → SAFE MODE ==========
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("║   BOTÃO SEGURADO 3s: SAFE MODE         ║");
        DEBUG_PRINTLN("║   Ativando modo de emergência...       ║");
        DEBUG_PRINTLN("╚════════════════════════════════════════╝");
        DEBUG_PRINTLN("");
        
        // Feedback visual LED (piscar rápido 5x)
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(50);
            digitalWrite(LED_BUILTIN, LOW);
            delay(50);
        }
        
        applyModeConfig(MODE_SAFE);
        _mode = MODE_SAFE;
        _missionActive = false;
    }
}