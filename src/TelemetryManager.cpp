/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO COM STORE-AND-FORWARD LEO COMPLETO
 * @version 6.0.0
 * @date 2025-11-15
 * 
 * CHANGELOG v6.0.0:
 * - [NEW] Store-and-Forward completo para órbita LEO
 * - [NEW] Gerenciamento inteligente de múltiplos nós terrestres
 * - [NEW] Detecção automática de passagens orbitais
 * - [NEW] Consolidação e forward otimizados
 */
#include "TelemetryManager.h"
#include "config.h"

bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _display(OLED_ADDRESS),
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
    _groundNodeBuffer.passNumber = 0;
    _groundNodeBuffer.inOrbitalPass = false;
    
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
    } else {
        if (!activeModeConfig->displayEnabled) {
            _display.displayOff();
        } else {
            _display.displayOn();
        }
    }
    
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    DEBUG_PRINTF("[TelemetryManager] Modo aplicado: %d | Display: %s | Logs: %s | LoRa: %s | HTTP: %s\n",
                 mode,
                 activeModeConfig->displayEnabled ? "ON" : "OFF",
                 activeModeConfig->serialLogsEnabled ? "ON" : "OFF",
                 activeModeConfig->loraEnabled ? "ON" : "OFF",
                 activeModeConfig->httpEnabled ? "ON" : "OFF");
}

bool TelemetryManager::begin() {
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT CubeSat - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("  LEO Store-and-Forward");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");

    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initI2CBus();

    bool displayOk = false;
    
    if (activeModeConfig->displayEnabled) {
        DEBUG_PRINTLN("[TelemetryManager] Inicializando DisplayManager...");
        
        if (_displayMgr.begin()) {
            _useNewDisplay = true;
            displayOk = true;
            _displayMgr.showBoot();
            delay(2000);
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager OK");
        } else {
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager FAILED, tentando sistema legado...");
            _useNewDisplay = false;
            
            delay(100);
            if (_display.init()) {
                _display.flipScreenVertically();
                _display.setFont(ArialMT_Plain_10);
                _display.clear();
                _display.drawString(0, 0, "AgroSat-IoT");
                _display.drawString(0, 15, "Initializing...");
                _display.display();
                displayOk = true;
                DEBUG_PRINTLN("[TelemetryManager] Display legado OK");
            } else {
                DEBUG_PRINTLN("[TelemetryManager] Display FAILED (não-crítico)");
            }
        }
        delay(500);
    }

    bool success = true;
    uint8_t subsystemsOk = 0;

    DEBUG_PRINTLN("[TelemetryManager] Inicializando RTC Manager...");
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

    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
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

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
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

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        if (_useNewDisplay && displayOk) {
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

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
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
    
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    DEBUG_PRINTLN("[TelemetryManager]   - WiFi + HTTP (obsat.org.br)");
    DEBUG_PRINTLN("[TelemetryManager]   - LoRa RX/TX (Store-and-Forward)");
    
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("WiFi", _comm.isConnected());
            delay(500);
            _displayMgr.showSensorInit("LoRa", _comm.isLoRaOnline());
            delay(500);
        }
        
        if (_rtc.isInitialized() && _comm.isConnected()) {
            DEBUG_PRINTLN("[TelemetryManager] WiFi disponível, sincronizando RTC com NTP...");
            
            if (_rtc.syncWithNTP()) {
                DEBUG_PRINTLN("[TelemetryManager] RTC sincronizado com NTP");
            } else {
                DEBUG_PRINTLN("[TelemetryManager] Falha na sincronização NTP (não-crítico)");
            }
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager FAILED");
    }

    if (_useNewDisplay && displayOk) {
        _displayMgr.showReady();
    } else if (displayOk) {
        _display.clear();
        _display.drawString(0, 0, success ? "Sistema OK!" : "ERRO Sistema!");
        _display.drawString(0, 15, "Modo: PRE-FLIGHT");
        String subsysInfo = String(subsystemsOk) + "/6 subsistemas";
        _display.drawString(0, 30, subsysInfo);
        
        if (_rtc.isInitialized()) {
            String dt = _rtc.getDateTime();
            String timeOnly = dt.substring(11, 19);
            _display.drawString(0, 45, "RTC: " + timeOnly);
        } else {
            _display.drawString(0, 45, "RTC: OFF");
        }
        
        _display.display();
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("Sistema inicializado: %s\n", success ? "OK" : "COM ERROS");
    DEBUG_PRINTF("Subsistemas online: %d/6\n", subsystemsOk);
    DEBUG_PRINTF("Heap disponível: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    return success;
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();
    
    _health.update(); 
    delay(5);
    _power.update(); 
    delay(5);
    _sensors.update(); 
    delay(10);
    _comm.update(); 
    delay(5);
    
    String loraPacket;
    int rssi;
    float snr;
    
    if (_comm.receiveLoRaPacket(loraPacket, rssi, snr)) {
        DEBUG_PRINTF("[TelemetryManager] ━━━━━ PACOTE LORA RECEBIDO ━━━━━\n");
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
            
            receivedData.priority = 0;
            receivedData.forwarded = false;
            
            _updateGroundNode(receivedData);
            
            DEBUG_PRINTLN("[TelemetryManager] Payload processado com sucesso!");
            DEBUG_PRINTF("[TelemetryManager]   Node ID: %u\n", receivedData.nodeId);
            DEBUG_PRINTF("[TelemetryManager]   Seq: %u\n", receivedData.sequenceNumber);
            DEBUG_PRINTF("[TelemetryManager]   Umidade Solo: %.1f%%\n", receivedData.soilMoisture);
            DEBUG_PRINTF("[TelemetryManager]   Temperatura: %.1f°C\n", receivedData.ambientTemp);
            DEBUG_PRINTF("[TelemetryManager]   Umidade Ar: %.1f%%\n", receivedData.humidity);
            DEBUG_PRINTF("[TelemetryManager]   Irrigação: %d\n", receivedData.irrigationStatus);
            DEBUG_PRINTF("[TelemetryManager]   Pacotes RX: %u, Perdidos: %u\n", 
                        receivedData.packetsReceived, receivedData.packetsLost);
            
            if (_storage.isAvailable()) {
                if (_storage.saveMissionData(receivedData)) {
                    DEBUG_PRINTLN("[TelemetryManager] Dados salvos no SD");
                }
            }
            
            if (_useNewDisplay && _displayMgr.isOn()) {
                char nodeInfo[32];
                snprintf(nodeInfo, sizeof(nodeInfo), "Node %u: %.0f%%", 
                        receivedData.nodeId, receivedData.soilMoisture);
                _displayMgr.displayMessage("LORA RX", nodeInfo);
            }
            
        } else {
            DEBUG_PRINTLN("[TelemetryManager] Falha ao processar payload");
        }
    }
    
    _manageOrbitalPass();
    
    static unsigned long lastCleanup = 0;
    if (currentTime - lastCleanup > 60000) {
        lastCleanup = currentTime;
        _cleanupStaleNodes();
    }
    
    _collectTelemetryData();
    _checkOperationalConditions();
    
    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    if (activeModeConfig->displayEnabled) {
        if (_useNewDisplay) {
            _displayMgr.updateTelemetry(_telemetryData);
        } else {
            if (currentTime - _lastDisplayUpdate >= 2000) {
                _lastDisplayUpdate = currentTime;
                updateDisplay();
            }
        }
    }
    
    delay(20);
}

void TelemetryManager::_updateGroundNode(const MissionData& data) {
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
            DEBUG_PRINTF("[TelemetryManager] Node %u ATUALIZADO (seq %u -> %u)\n", 
                         data.nodeId, existingNode.sequenceNumber, data.sequenceNumber);
            
            if (data.sequenceNumber > existingNode.sequenceNumber + 1) {
                uint16_t lost = data.sequenceNumber - existingNode.sequenceNumber - 1;
                DEBUG_PRINTF("[TelemetryManager]   %u pacote(s) perdido(s)\n", lost);
            }
            
            existingNode = data;
            existingNode.lastLoraRx = millis();
            existingNode.forwarded = false;
            
            _groundNodeBuffer.lastUpdate[existingIndex] = millis();
            _groundNodeBuffer.totalPacketsCollected++;
            
        } else if (data.sequenceNumber == existingNode.sequenceNumber) {
            DEBUG_PRINTF("[TelemetryManager] Node %u pacote duplicado (seq %u), ignorando\n",
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
            
            _groundNodeBuffer.lastUpdate[newIndex] = millis();
            _groundNodeBuffer.activeNodes++;
            _groundNodeBuffer.totalPacketsCollected++;
            
            DEBUG_PRINTF("[TelemetryManager] Node %u NOVO (slot %d), total: %d/%d\n", 
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
    
    DEBUG_PRINTF("[TelemetryManager] Buffer cheio! Substituindo Node %u por Node %u\n",
                 _groundNodeBuffer.nodes[replaceIndex].nodeId, newData.nodeId);
    
    _groundNodeBuffer.nodes[replaceIndex] = newData;
    _groundNodeBuffer.nodes[replaceIndex].lastLoraRx = millis();
    _groundNodeBuffer.nodes[replaceIndex].forwarded = false;
    _groundNodeBuffer.lastUpdate[replaceIndex] = millis();
}

void TelemetryManager::_cleanupStaleNodes(unsigned long maxAge) {
    unsigned long now = millis();
    uint8_t removedCount = 0;
    
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        unsigned long age = now - _groundNodeBuffer.lastUpdate[i];
        
        if (age > maxAge) {
            DEBUG_PRINTF("[TelemetryManager] Removendo Node %u (inativo há %lu s)\n",
                         _groundNodeBuffer.nodes[i].nodeId, age / 1000);
            
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
        DEBUG_PRINTF("[TelemetryManager] Limpeza: %d nó(s) removido(s)\n", removedCount);
    }
}

void TelemetryManager::_manageOrbitalPass() {
    unsigned long now = millis();
    
    if (_groundNodeBuffer.activeNodes == 0) {
        return;
    }
    
    unsigned long timeSinceLastPacket = ULONG_MAX;
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        unsigned long age = now - _groundNodeBuffer.lastUpdate[i];
        if (age < timeSinceLastPacket) {
            timeSinceLastPacket = age;
        }
    }
    
    if (!_groundNodeBuffer.inOrbitalPass && timeSinceLastPacket < 600000) {
        _groundNodeBuffer.inOrbitalPass = true;
        _groundNodeBuffer.sessionStartTime = _rtc.isInitialized() ? _rtc.getUnixTime() : (millis() / 1000);
        _groundNodeBuffer.passNumber++;
        
        DEBUG_PRINTF("[TelemetryManager] ========== INICIO PASSAGEM #%u ==========\n",
                     _groundNodeBuffer.passNumber);
    }
    
    if (_groundNodeBuffer.inOrbitalPass && timeSinceLastPacket > ORBITAL_PASS_TIMEOUT_MS) {
        _groundNodeBuffer.inOrbitalPass = false;
        _groundNodeBuffer.sessionEndTime = _rtc.isInitialized() ? _rtc.getUnixTime() : (millis() / 1000);
        
        unsigned long passDuration = _groundNodeBuffer.sessionEndTime - _groundNodeBuffer.sessionStartTime;
        
        DEBUG_PRINTF("[TelemetryManager] ========== FIM PASSAGEM #%u ==========\n",
                     _groundNodeBuffer.passNumber);
        DEBUG_PRINTF("[TelemetryManager] Duração: %lu s\n", passDuration);
        DEBUG_PRINTF("[TelemetryManager] Nós coletados: %u\n", _groundNodeBuffer.activeNodes);
        DEBUG_PRINTF("[TelemetryManager] Pacotes totais: %u\n", _groundNodeBuffer.totalPacketsCollected);
        
        _prepareForward();
    }
}

void TelemetryManager::_prepareForward() {
    for (uint8_t i = 0; i < _groundNodeBuffer.activeNodes; i++) {
        _groundNodeBuffer.nodes[i].forwarded = false;
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Dados prontos para forward na próxima transmissão");
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT || _mode == MODE_POSTFLIGHT) {
        DEBUG_PRINTLN("[TelemetryManager] Missão já em andamento!");
        return;
    }
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Início: %s\n", _rtc.getDateTime().c_str());
    }
    
    if (_useNewDisplay && _displayMgr.isOn()) {
        _displayMgr.displayMessage("MISSAO", "INICIADA");
        delay(2000);
    }
    
    _mode = MODE_FLIGHT;
    _missionActive = true;
    _missionStartTime = millis();
    
    applyModeConfig(MODE_FLIGHT);
    
    DEBUG_PRINTLN("[TelemetryManager] Modo FLIGHT ativado");
    DEBUG_PRINTLN("[TelemetryManager] Store-and-Forward ativo");
    DEBUG_PRINTLN("[TelemetryManager] Missão iniciada com sucesso!");
}

void TelemetryManager::stopMission() {
    if (!_missionActive) {
        DEBUG_PRINTLN("[TelemetryManager] Nenhuma missão ativa!");
        return;
    }
    
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] ENCERRANDO MISSÃO");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    uint32_t missionDuration = millis() - _missionStartTime;
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Fim: %s\n", _rtc.getDateTime().c_str());
    }
    
    DEBUG_PRINTF("[TelemetryManager] Duração: %lu segundos\n", missionDuration / 1000);
    DEBUG_PRINTF("[TelemetryManager] Passagens orbitais: %u\n", _groundNodeBuffer.passNumber);
    DEBUG_PRINTF("[TelemetryManager] Total pacotes coletados: %u\n", _groundNodeBuffer.totalPacketsCollected);
    
    _mode = MODE_POSTFLIGHT;
    _missionActive = false;
    
    if (_useNewDisplay) {
        char durationStr[32];
        snprintf(durationStr, sizeof(durationStr), "%lus", missionDuration / 1000);
        _displayMgr.displayMessage("MISSAO COMPLETA", durationStr);
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Missão encerrada com sucesso!");
}

OperationMode TelemetryManager::getMode() {
    return _mode;
}

void TelemetryManager::updateDisplay() {
    if (!activeModeConfig->displayEnabled) {
        _display.displayOff();
        return;
    }
    
    if (_mode == MODE_ERROR) {
        _displayError("Sistema com erro");
        return;
    }
    
    if (_mode == MODE_PREFLIGHT) {
        _displayStatus();
    } else {
        _displayTelemetry();
    }
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
    _telemetryData.temperature = _sensors.getTemperature();
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
    _telemetryData.temperatureSI = NAN;
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
    
    String payloadStr = _comm.generateConsolidatedPayload(_groundNodeBuffer);
    strncpy(_telemetryData.payload, payloadStr.c_str(), PAYLOAD_MAX_SIZE - 1);
    _telemetryData.payload[PAYLOAD_MAX_SIZE - 1] = '\0';
}

void TelemetryManager::_sendTelemetry() {
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF("[TelemetryManager] Enviando telemetria [%s]...\n", 
                     _rtc.getDateTime().c_str());
        
        DEBUG_PRINTF("  Temp BMP280: %.2f°C", _telemetryData.temperature);
        
        if (!isnan(_telemetryData.temperatureSI)) {
            DEBUG_PRINTF(" | Temp SI7021: %.2f°C", _telemetryData.temperatureSI);
            float delta = _telemetryData.temperature - _telemetryData.temperatureSI;
            DEBUG_PRINTF(" | Delta: %.2f°C\n", delta);
        } else {
            DEBUG_PRINTLN();
        }
        
        DEBUG_PRINTF("  Pressão: %.2f hPa, Alt: %.1f m\n", 
                     _telemetryData.pressure, 
                     _telemetryData.altitude);
        
        if (_groundNodeBuffer.activeNodes > 0) {
            DEBUG_PRINTF("  Nós terrestres: %d ativos\n", _groundNodeBuffer.activeNodes);
        }
    }
    
    bool sendSuccess = _comm.sendTelemetry(_telemetryData, _groundNodeBuffer);
    
    if (sendSuccess) {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada com sucesso!");
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
        DEBUG_PRINTF("[TelemetryManager] Salvando dados no SD [%s]...\n", _rtc.getDateTime().c_str());
    } else {
        DEBUG_PRINTLN("[TelemetryManager] Salvando dados no SD...");
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

void TelemetryManager::_displayStatus() {
    _display.clear();
    _display.setFont(ArialMT_Plain_10);
    
    String line1 = "PRE-FLIGHT MODE";
    _display.drawString(0, 0, line1);

    String batteryStr = "Bat: " + String(_power.getPercentage(), 0) + "% (" + String(_power.getVoltage(), 2) + "V)";
    _display.drawString(0, 12, batteryStr);

    String tempHumStr;
    if (_sensors.isSI7021Online() && !isnan(_sensors.getHumidity())) {
        tempHumStr = "Temp: " + String(_sensors.getTemperature(), 1) + "C Hum: " + String(_sensors.getHumidity(), 0) + "%";
    } else {
        tempHumStr = "Temp: " + String(_sensors.getTemperature(), 1) + "C Press: " + String(_sensors.getPressure(), 0) + "hPa";
    }
    _display.drawString(0, 24, tempHumStr);

    String timeLine;
    if (_rtc.isInitialized()) {
        String dt = _rtc.getDateTime();
        timeLine = "RTC: " + dt.substring(11, 19);
    } else {
        timeLine = "Nodes: " + String(_groundNodeBuffer.activeNodes);
    }
    _display.drawString(0, 36, timeLine);

    String statusStr = "";
    statusStr += _comm.isConnected() ? "[WiFi]" : "[NoWiFi]";
    statusStr += _comm.isLoRaOnline() ? "[LoRa]" : "[NoLoRa]";
    statusStr += _storage.isAvailable() ? "[SD]" : "[NoSD]";
    _display.drawString(0, 48, statusStr);

    _display.display();
}

void TelemetryManager::_displayTelemetry() {
    _display.clear();
    _display.setFont(ArialMT_Plain_10);

    unsigned long missionTimeSec = _telemetryData.missionTime / 1000;
    unsigned long minutes = missionTimeSec / 60;
    unsigned long seconds = missionTimeSec % 60;
    String timeStr = String(minutes) + ":" + (seconds < 10 ? "0" + String(seconds) : String(seconds));
    _display.drawString(0, 0, "Mission: " + timeStr);

    String batStr = "Bat: " + String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 12, batStr);

    String nodesStr = "Nodes: " + String(_groundNodeBuffer.activeNodes) + "/" + String(MAX_GROUND_NODES);
    _display.drawString(0, 24, nodesStr);

    String passStr = "Pass: #" + String(_groundNodeBuffer.passNumber);
    _display.drawString(0, 36, passStr);

    String pktStr = "Pkts: " + String(_groundNodeBuffer.totalPacketsCollected);
    _display.drawString(0, 48, pktStr);

    _display.display();
}

void TelemetryManager::_displayError(const String& error) {
    _display.clear();
    _display.setFont(ArialMT_Plain_10);
    _display.drawString(0, 0, "ERRO:");
    _display.drawString(0, 15, error);
    String heapInfo = "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB";
    _display.drawString(0, 30, heapInfo);
    _display.display();
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
    _comm.sendLoRa("TEST_AGROSAT");
}

void TelemetryManager::sendCustomLoRa(const String& message) {
    _comm.sendLoRa(message);
}

void TelemetryManager::printLoRaStats() {
    uint16_t sent, failed;
    _comm.getLoRaStatistics(sent, failed);
    DEBUG_PRINTF("[TelemetryManager] LoRa Stats: %d enviados, %d falhas\n", sent, failed);
}
