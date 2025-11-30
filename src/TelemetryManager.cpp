/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO OTIMIZADA PARA BALÃO METEOROLÓGICO (HAB) + UTC
 * @version 7.4.0 - HAB Edition + UTC Standard
 * @date 2025-11-30
 * 
 * CHANGELOG v7.4.0:
 * - [NEW] RTC 100% UTC (CubeSat padrão internacional)
 * - [FIX] Reset de flag forwarded ao atualizar nó
 * - [FIX] Reset periódico de flags para retransmissão contínua
 * - [NEW] Timestamp UTC em toda telemetria
 */
#include "TelemetryManager.h"
#include "config.h"

bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;
DisplayManager* g_displayManagerPtr = nullptr;

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
    _useNewDisplay(true),
    _lastSensorReset(0)
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
        Wire.setTimeOut(1000);
        
        delay(1000);
        i2cInitialized = true;
        
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado (100 kHz, timeout 1000ms)");
        DEBUG_PRINTLN("[TelemetryManager] Aguardando estabilização dos sensores...");
        delay(500);
        
        DEBUG_PRINTLN("[TelemetryManager] Dispositivos no barramento:");
        
        uint8_t devicesFound = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            uint8_t error = Wire.endTransmission();
            if (error == 0) {
                DEBUG_PRINTF("[TelemetryManager]   - 0x%02X\n", addr);
                devicesFound++;
            }
            delay(10);
        }
        
        if (devicesFound == 0) {
            DEBUG_PRINTLN("[TelemetryManager] AVISO: Nenhum dispositivo detectado!");
            DEBUG_PRINTLN("[TelemetryManager] Verifique conexões I2C (SDA, SCL, VCC, GND)");
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
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT HAB - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION " UTC");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");

    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initI2CBus();
    
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);

    bool success = true;
    uint8_t subsystemsOk = 0;

    DEBUG_PRINTLN("[TelemetryManager] Inicializando DisplayManager...");
    bool displayOk = false;
    
    if (activeModeConfig->displayEnabled) {
        if (_displayMgr.begin()) {
            _useNewDisplay = true;
            displayOk = true;
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager OK");
            
            g_displayManagerPtr = &_displayMgr;
            _displayMgr.showBoot();
            delay(2000);
        } else {
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager FAILED");
            _useNewDisplay = false;
            g_displayManagerPtr = nullptr;
        }
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando RTC (UTC Mode)...");
    if (_rtc.begin(&Wire)) {
        subsystemsOk++;
        DEBUG_PRINTF("[TelemetryManager] RTC OK: %s (unix: %lu)\n", 
                     _rtc.getUTCDateTime().c_str(), _rtc.getUnixTime());
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("RTC", true);
        }
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando botão...");
    _button.begin();

    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] System Health OK");
    } else {
        success = false;
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager OK");
    } else {
        success = false;
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        if (_useNewDisplay && displayOk) {
            delay(500);
            _displayMgr.clear();
            delay(100);
            
            _displayMgr.showSensorInit("MPU9250", _sensors.isMPU9250Online());
            _displayMgr.showSensorInit("BMP280", _sensors.isBMP280Online());
            _displayMgr.showSensorInit("SI7021", _sensors.isSI7021Online());
            _displayMgr.showSensorInit("CCS811", _sensors.isCCS811Online());
        }
    } else {
        success = false;
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage...");
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Storage OK");
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication...");
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication OK");
    } else {
        success = false;
    }

    if (_rtc.isInitialized() && WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("[TelemetryManager] Iniciando sincronização NTP...");
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        
        if (_rtc.syncWithNTP()) {
            DEBUG_PRINTLN("[TelemetryManager] RTC sincronizado com NTP!");
            DEBUG_PRINTF("[TelemetryManager] Hora local (BRT): %s\n", 
                        _rtc.getDateTime().c_str());
            DEBUG_PRINTF("[TelemetryManager] Unix timestamp: %lu\n", 
                        _rtc.getUnixTime());
            
            if (_useNewDisplay && displayOk) {
                _displayMgr.showSensorInit("NTP Sync", true);
                delay(1000);
            }
        } else {
            DEBUG_PRINTLN("[TelemetryManager] NTP sync falhou - usando compile time");
            
            if (_useNewDisplay && displayOk) {
                _displayMgr.showSensorInit("NTP Sync", false);
                delay(1000);
            }
        }
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] WiFi offline - NTP sync não disponível");
    }

    if (_useNewDisplay && displayOk) {
        _displayMgr.showReady();
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
    
    return success;
}


void TelemetryManager::loop() {
    uint32_t currentTime = millis();
    
    // Atualizar subsistemas
    _health.update(); 
    delay(5);
    _power.update(); 
    delay(5);
    _sensors.update(); 
    delay(10);
    _comm.update(); 
    delay(5);
    
    _rtc.update();
    delay(5);
    
    _handleButtonEvents();
    delay(5);
    
    // Controle de LED
    _updateLEDIndicator(currentTime);
    
    // ========== RECEPÇÃO LORA ==========
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
            
            if (_storage.isAvailable()) {
                _storage.saveMissionData(receivedData);
            }
            
            if (_useNewDisplay && _displayMgr.isOn()) {
                char nodeInfo[32];
                snprintf(nodeInfo, sizeof(nodeInfo), "N%u:%.0f%% %ddBm", 
                        receivedData.nodeId, receivedData.soilMoisture, rssi);
                _displayMgr.displayMessage("LORA RX", nodeInfo);
            }
        }
    }
    
    // ========== LIMPEZA DE BUFFER ==========
    static unsigned long lastCleanup = 0;
    if (currentTime - lastCleanup > 600000) {
        lastCleanup = currentTime;
        _cleanupStaleNodes(NODE_TTL_MS);
    }
    
    // ========== RESET FLAGS RETRANSMISSÃO ==========
    static unsigned long lastFlagReset = 0;
    const unsigned long FLAG_RESET_INTERVAL = 60000;  // 60s
    
    if (currentTime - lastFlagReset >= FLAG_RESET_INTERVAL) {
        lastFlagReset = currentTime;
        
        std::lock_guard<std::mutex> lock(_bufferMutex);
        uint8_t resetCount = 0;
        
        for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
            if (_groundNodeBuffer.nodes[i].forwarded) {
                _groundNodeBuffer.nodes[i].forwarded = false;
                resetCount++;
            }
        }
        
        if (resetCount > 0) {
            DEBUG_PRINTF("[TelemetryManager] %d nó(s) prontos para retransmissão periódica\n", resetCount);
        }
    }
    
    // ========== COLETAR TELEMETRIA ==========
    _collectTelemetryData();
    _checkOperationalConditions();
    
    // ========== TRANSMISSÃO ==========
    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    // ========== SALVAR SD ==========
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // ========== ATUALIZAR DISPLAY ==========
    if (activeModeConfig->displayEnabled && _useNewDisplay) {
        _displayMgr.updateTelemetry(_telemetryData);
    }
    
    // ========== MONITORAMENTO DE HEAP ==========
    _monitorHeapUsage(currentTime);
    
    delay(20);
}

void TelemetryManager::_updateGroundNode(const MissionData& data) {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    
    int existingIndex = -1;
    
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        if (_groundNodeBuffer.nodes[i].nodeId == data.nodeId) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex >= 0) {
        MissionData& existingNode = _groundNodeBuffer.nodes[existingIndex];
        
        if (data.sequenceNumber > existingNode.sequenceNumber) {
            DEBUG_PRINTF("[TelemetryManager] Node %u ATUALIZADO (seq %u → %u)\n", 
                         data.nodeId, existingNode.sequenceNumber, data.sequenceNumber);
            
            if (data.sequenceNumber > existingNode.sequenceNumber + 1) {
                uint16_t lost = data.sequenceNumber - existingNode.sequenceNumber - 1;
                DEBUG_PRINTF("[TelemetryManager] %u pacote(s) perdido(s)\n", lost);
            }
            
            existingNode = data;
            existingNode.lastLoraRx = millis();
            existingNode.forwarded = false;  // ✅ RESET FLAG
            existingNode.priority = _comm.calculatePriority(data);
            
            _groundNodeBuffer.lastUpdate[existingIndex] = millis();
            _groundNodeBuffer.totalPacketsCollected++;
            
            DEBUG_PRINTF("[TelemetryManager] Node %u pronto para retransmitir\n", data.nodeId);
            
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
    
    // ✅ UTC TIMESTAMP
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Início UTC: %s (unix: %lu)\n", 
                     _rtc.getUTCDateTime().c_str(), _rtc.getUnixTime());
        DEBUG_PRINTF("[TelemetryManager] Local: %s\n", _rtc.getLocalDateTime().c_str());
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
    
    // ✅ UTC TIMESTAMP
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Fim UTC: %s (unix: %lu)\n", 
                     _rtc.getUTCDateTime().c_str(), _rtc.getUnixTime());
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
    
    _mode = MODE_PREFLIGHT;
    _missionActive = false;
    
    if (_useNewDisplay) {
        char durationStr[32];
        snprintf(durationStr, sizeof(durationStr), "%lum%lus", 
                 missionDuration / 60000, (missionDuration % 60000) / 1000);
        _displayMgr.displayMessage("MISSAO COMPLETA", durationStr);
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Missão HAB encerrada - Sistema em PREFLIGHT");
}

OperationMode TelemetryManager::getMode() {
    return _mode;
}

void TelemetryManager::_collectTelemetryData() {
    // ✅ UTC TIMESTAMP
    if (_rtc.isInitialized()) {
        _telemetryData.timestamp = _rtc.getUnixTime();  // UTC puro!
    } else {
        _telemetryData.timestamp = millis() / 1000;
    }
    
    _telemetryData.missionTime = _health.getMissionTime();
    _telemetryData.batteryVoltage = _power.getVoltage();
    _telemetryData.batteryPercentage = _power.getPercentage();
    
    _telemetryData.temperature = _sensors.getTemperature();
    _telemetryData.temperatureBMP = _sensors.getTemperatureBMP280();
    _telemetryData.temperatureSI = NAN;
    
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
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
    
    // Gerar resumo simples dos nós
    if (_groundNodeBuffer.activeNodes > 0) {
        snprintf(_telemetryData.payload, PAYLOAD_MAX_SIZE, 
                 "Nodes:%d", _groundNodeBuffer.activeNodes);
    } else {
        _telemetryData.payload[0] = '\0';
    }
}

void TelemetryManager::_sendTelemetry() {
    if (activeModeConfig->serialLogsEnabled) {
        // ✅ UTC + LOCAL display
        DEBUG_PRINTF("[TelemetryManager] TX [UTC: %s | Local: %s]\n", 
                     _rtc.getUTCDateTime().c_str(),
                     _rtc.getLocalDateTime().c_str());
        
        bool hasBMP = !isnan(_telemetryData.temperatureBMP);
        bool hasSI = !isnan(_telemetryData.temperatureSI);
        
        if (hasBMP && hasSI) {
            float delta = abs(_telemetryData.temperatureBMP - _telemetryData.temperatureSI);
            DEBUG_PRINTF("Temp BMP280: %.2f°C | SI7021: %.2f°C | Δ: %.2f°C\n",
                         _telemetryData.temperatureBMP,
                         _telemetryData.temperatureSI,
                         delta);
        } else if (hasBMP && !hasSI) {
            DEBUG_PRINTF("Temp BMP280: %.2f°C (SI7021 indisponível)\n", 
                         _telemetryData.temperatureBMP);
        } else if (!hasBMP && hasSI) {
            DEBUG_PRINTF("Temp SI7021: %.2f°C (BMP280 indisponível)\n", 
                         _telemetryData.temperatureSI);
        } else {
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
        DEBUG_PRINTF("[TelemetryManager] Salvando [UTC: %s]...\n", _rtc.getUTCDateTime().c_str());
    } else {
        DEBUG_PRINTLN("[TelemetryManager] Salvando dados...");
    }
    
    if (_storage.saveTelemetry(_telemetryData)) { 
        DEBUG_PRINTLN("[TelemetryManager] Telemetria salva no SD"); 
    }
    
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
    
    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck >= 60000) {
        lastSensorCheck = millis();
        
        if (!_sensors.isMPU9250Online()) { 
            _health.reportError(STATUS_SENSOR_ERROR, "IMU offline"); 
            
            if (millis() - _lastSensorReset >= 300000) {
                DEBUG_PRINTLN("[TelemetryManager] Tentando recuperação de sensores...");
                _sensors.resetAll();
                _lastSensorReset = millis();
            }
        }
        
        if (!_sensors.isBMP280Online()) { 
            _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline"); 
        }
    }
}

void TelemetryManager::testLoRaTransmission() {
    DEBUG_PRINTLN("[TelemetryManager] Testando transmissão LoRa...");
    _comm.sendLoRa("TEST_AGROSAT_HAB_UTC");
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
        if (_mode == MODE_PREFLIGHT || _mode == MODE_POSTFLIGHT) {
            DEBUG_PRINTLN("");
            DEBUG_PRINTLN("╔════════════════════════════════════════╗");
            DEBUG_PRINTLN("║   BOTÃO PRESSIONADO: START MISSION     ║");
            DEBUG_PRINTLN("║   PREFLIGHT → FLIGHT                   ║");
            DEBUG_PRINTLN("╚════════════════════════════════════════╝");
            DEBUG_PRINTLN("");
            
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
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("║   BOTÃO SEGURADO 3s: SAFE MODE         ║");
        DEBUG_PRINTLN("║   Ativando modo de emergência...       ║");
        DEBUG_PRINTLN("╚════════════════════════════════════════╝");
        DEBUG_PRINTLN("");
        
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

void TelemetryManager::_updateLEDIndicator(unsigned long currentTime) {
    static unsigned long lastBlink = 0;
    
    if (currentTime - lastBlink < 1000) {
        return;
    }
    
    lastBlink = currentTime;
    
    switch (_mode) {
        case MODE_PREFLIGHT: 
            digitalWrite(LED_BUILTIN, HIGH); 
            break;
            
        case MODE_FLIGHT: 
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
            break;
            
        case MODE_SAFE:
            if ((currentTime / 1000) % 5 < 3) {
                digitalWrite(LED_BUILTIN, HIGH);
            } else {
                digitalWrite(LED_BUILTIN, LOW);
            }
            break;
            
        case MODE_POSTFLIGHT:
            if ((currentTime / 1000) % 2 == 0) {
                digitalWrite(LED_BUILTIN, HIGH);
            } else {
                digitalWrite(LED_BUILTIN, LOW);
            }
            break;
            
        case MODE_ERROR:
            if ((currentTime / 100) % 2 == 0) {
                digitalWrite(LED_BUILTIN, HIGH);
            } else {
                digitalWrite(LED_BUILTIN, LOW);
            }
            break;
            
        default: 
            digitalWrite(LED_BUILTIN, LOW); 
            break;
    }
}

void TelemetryManager::_monitorHeapUsage(unsigned long currentTime) {
    static unsigned long lastHeapCheck = 0;
    
    if (currentTime - lastHeapCheck < 30000) {
        return;
    }
    
    lastHeapCheck = currentTime;
    uint32_t currentHeap = ESP.getFreeHeap();
    
    if (currentHeap < _minHeapSeen) {
        _minHeapSeen = currentHeap;
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========== STATUS DO SISTEMA ==========");
    DEBUG_PRINTF("Uptime: %lu s\n", currentTime / 1000);
    DEBUG_PRINTF("Modo: %d\n", _mode);
    
    // ✅ UTC STATUS
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("RTC UTC: %s\n", _rtc.getUTCDateTime().c_str());
        DEBUG_PRINTF("Unix: %lu\n", _rtc.getUnixTime());
    }
    
    DEBUG_PRINTF("Heap atual: %lu KB\n", currentHeap / 1024);
    DEBUG_PRINTF("Heap mínimo: %lu KB\n", _minHeapSeen / 1024);
    
    if (currentHeap < 15000) {
        DEBUG_PRINTLN("ALERTA: Heap baixo!");
        _health.reportError(STATUS_WATCHDOG, "Low memory");
    }
    
    if (currentHeap < 8000) {
        DEBUG_PRINTLN("CRÍTICO: Entrando em SAFE MODE!");
        applyModeConfig(MODE_SAFE);
    }
    
    if (currentHeap < 5000) {
        DEBUG_PRINTF("CRÍTICO: Heap muito baixo: %lu bytes\n", currentHeap); 
        DEBUG_PRINTLN("Sistema será reiniciado em 3 segundos..."); 
        delay(3000); 
        ESP.restart(); 
    }
    
    DEBUG_PRINTLN("=======================================");
    DEBUG_PRINTLN("");
}
