/**
 * @file TelemetryManager.cpp
 * @brief Gerenciador Central (FIX: Lógica Unificada de Missão)
 * @version 10.5.0
 */

#include "TelemetryManager.h"
#include "config.h"

// Variáveis Globais de Configuração
bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _sensors(), 
    _gps(),
    _power(), 
    _systemHealth(), 
    _rtc(), 
    _button(),
    _storage(), 
    _comm(), 
    _groundNodes(),
    _mission(_rtc, _groundNodes),
    _telemetryCollector(_sensors, _gps, _power, _systemHealth, _rtc, _groundNodes),
    _commandHandler(_sensors),
    _mode(MODE_INIT), 
    // _missionActive removido
    _lastTelemetrySend(0), 
    _lastStorageSave(0),
    _missionStartTime(0), 
    _lastSensorReset(0),
    _lastBeaconTime(0)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN; 
    _telemetryData.magY = NAN; 
    _telemetryData.magZ = NAN;
    
    _telemetryData.latitude = 0.0;
    _telemetryData.longitude = 0.0;
    _telemetryData.gpsAltitude = 0.0;
    _telemetryData.satellites = 0;
    _telemetryData.gpsFix = false;
}

bool TelemetryManager::begin() {
    uint32_t initialHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initModeDefaults();

    bool success = true;
    uint8_t subsystemsOk = 0;

    _initSubsystems(subsystemsOk, success);
    _syncNTPIfAvailable();

    if (_mission.begin()) { 
        DEBUG_PRINTLN("[TelemetryManager] Restaurando modo FLIGHT...");
        _mode = MODE_FLIGHT;
        // _missionActive = true; // REMOVIDO: Redundante
        applyModeConfig(MODE_FLIGHT);
    }

    _logInitSummary(success, subsystemsOk, initialHeap);
    return success;
}

void TelemetryManager::_initModeDefaults() {
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);
}

void TelemetryManager::_initSubsystems(uint8_t& subsystemsOk, bool& success) {
    DEBUG_PRINTLN("[TelemetryManager] Init RTC (UTC)");
    if (_rtc.begin(&Wire)) subsystemsOk++;
    else success = false;

    DEBUG_PRINTLN("[TelemetryManager] Init botao");
    _button.begin();

    DEBUG_PRINTLN("[TelemetryManager] Init SystemHealth");
    if (_systemHealth.begin()) subsystemsOk++;
    else success = false;

    DEBUG_PRINTLN("[TelemetryManager] Init PowerManager");
    if (_power.begin()) subsystemsOk++;
    else success = false;

    DEBUG_PRINTLN("[TelemetryManager] Init SensorManager");
    if (_sensors.begin()) subsystemsOk++;
    else success = false;
    
    DEBUG_PRINTLN("[TelemetryManager] Init GPSManager");
    if (_gps.begin()) subsystemsOk++;

    DEBUG_PRINTLN("[TelemetryManager] Init Storage");
    if (_storage.begin()) {
        _storage.setRTCManager(&_rtc); 
        _storage.setSystemHealth(&_systemHealth); // <--- LINHA ADICIONADA
        subsystemsOk++;
    } else success = false;

    DEBUG_PRINTLN("[TelemetryManager] Init Communication");
    if (_comm.begin()) subsystemsOk++;
    else success = false;
}

void TelemetryManager::_syncNTPIfAvailable() {
    if (_rtc.isInitialized()) {
        DEBUG_PRINTLN("[TelemetryManager] Sincronizando NTP...");
        _rtc.syncWithNTP();
    }
}

void TelemetryManager::_logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap) {
    uint32_t used = initialHeap - ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Init: %s, subsistemas=%d/8, heap usado=%lu bytes\n",
                 success ? "OK" : "ERRO", subsystemsOk, used);
}

void TelemetryManager::feedWatchdog() {
    _systemHealth.feedWatchdog();
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();

    _systemHealth.update();
    SystemHealth::HeapStatus heapStatus = _systemHealth.getHeapStatus();

    switch (heapStatus) {
        case SystemHealth::HeapStatus::HEAP_CRITICAL:
            if (_mode != MODE_SAFE) {
                DEBUG_PRINTLN("[TelemetryManager] MEMÓRIA CRÍTICA! Entrando em SAFE MODE.");
                applyModeConfig(MODE_SAFE);
                _mode = MODE_SAFE;
            }
            break;
        case SystemHealth::HeapStatus::HEAP_FATAL:
            if (activeModeConfig->serialLogsEnabled) {
                DEBUG_PRINTLN("[TelemetryManager] MEMÓRIA FATAL. Reiniciando...");
            }
            delay(1000);
            ESP.restart();
            break;
        default: break;
    }

    _power.update();
    _power.adjustCpuFrequency();
    _sensors.update(); 
    _gps.update();
    _comm.update();
    _rtc.update();

    _handleButtonEvents();
    _updateLEDIndicator(currentTime);

    _handleIncomingRadio();
    _maintainGroundNetwork();

    _telemetryCollector.collect(_telemetryData);
    
    _systemHealth.setCurrentMode((uint8_t)_mode);
    _systemHealth.setBatteryVoltage(_power.getVoltage());
    _systemHealth.setSDCardStatus(_storage.isAvailable());
    
    _checkOperationalConditions();

    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }

    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    if (_mode == MODE_SAFE) {
        uint32_t beaconInterval = activeModeConfig->beaconInterval;
        if (beaconInterval > 0 && (currentTime - _lastBeaconTime >= beaconInterval)) {
            _sendSafeBeacon();
            _lastBeaconTime = currentTime;
        }
    }
}

// ... (Método _checkOperationalConditions mantido igual ao anterior) ...
void TelemetryManager::_checkOperationalConditions() {
    bool batCritical = _power.isCritical();
    bool batLow = (_power.getVoltage() <= BATTERY_LOW); 
    
    if (batCritical) _power.enablePowerSave();
    
    _systemHealth.setSystemError(STATUS_BATTERY_CRIT, batCritical);
    _systemHealth.setSystemError(STATUS_BATTERY_LOW, batLow);
    
    bool sensorFail = !_sensors.isMPU9250Online() || !_sensors.isBMP280Online(); 
    
    if (sensorFail) {
        static unsigned long lastSensorReset = 0;
        if (millis() - lastSensorReset > 10000) {
            DEBUG_PRINTLN("[TM] Sensores instáveis. Tentando reset...");
            _sensors.resetAll();
            lastSensorReset = millis();
        }
    }
    _systemHealth.setSystemError(STATUS_SENSOR_ERROR, sensorFail);
    
    if (activeModeConfig->httpEnabled) {
        bool wifiDown = !_comm.isWiFiConnected();
        if (wifiDown) {
            static unsigned long lastWifiRetry = 0;
            if (millis() - lastWifiRetry > 30000) {
                DEBUG_PRINTLN("[TM] WiFi caiu. Reconectando...");
                _comm.connectWiFi();
                lastWifiRetry = millis();
            }
        }
        _systemHealth.setSystemError(STATUS_WIFI_ERROR, wifiDown);
    } else {
        _systemHealth.setSystemError(STATUS_WIFI_ERROR, false);
    }
}

void TelemetryManager::_handleIncomingRadio() {
    String loraPacket;
    int rssi = 0;
    float snr = 0.0f;

    if (_comm.receiveLoRaPacket(loraPacket, rssi, snr)) {
        MissionData rxData;
        if (_comm.processLoRaPacket(loraPacket, rxData)) {
            rxData.rssi = rssi;
            rxData.snr = snr;
            rxData.lastLoraRx = millis();
            rxData.collectionTime = _rtc.isInitialized() ? _rtc.getUnixTime() : (millis()/1000);
            
            // FIX: Chamada simplificada (GroundNodeManager já sabe calcular prioridade)
            _groundNodes.updateNode(rxData);
            
            _storage.saveMissionData(rxData);
            
            DEBUG_PRINTF("[TM] Node %u RX: RSSI=%d dBm, SNR=%.1f dB\n", 
                         rxData.nodeId, rssi, snr);
        }
    }
}W

void TelemetryManager::_maintainGroundNetwork() {
    static unsigned long lastMaint = 0;
    if (millis() - lastMaint > 60000) { 
        lastMaint = millis();
        _groundNodes.cleanup(millis(), NODE_TTL_MS);
        _groundNodes.resetForwardFlags();
    }
}

void TelemetryManager::applyModeConfig(uint8_t modeIndex) {
    OperationMode mode = static_cast<OperationMode>(modeIndex);
    uint32_t wdtTimeout = WATCHDOG_TIMEOUT_PREFLIGHT; 

    switch (mode) {
        case MODE_PREFLIGHT: 
            activeModeConfig = &PREFLIGHT_CONFIG; 
            wdtTimeout = WATCHDOG_TIMEOUT_PREFLIGHT;
            break;
        case MODE_FLIGHT:    
            activeModeConfig = &FLIGHT_CONFIG; 
            wdtTimeout = WATCHDOG_TIMEOUT_FLIGHT;
            break;
        case MODE_SAFE:      
            activeModeConfig = &SAFE_CONFIG; 
            wdtTimeout = WATCHDOG_TIMEOUT_SAFE;
            break;
        default:             
            activeModeConfig = &PREFLIGHT_CONFIG; 
            wdtTimeout = WATCHDOG_TIMEOUT_PREFLIGHT;
            break;
    }
    
    currentSerialLogsEnabled = activeModeConfig->serialLogsEnabled;
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    _systemHealth.setWatchdogTimeout(wdtTimeout);
    
    DEBUG_PRINTF("[TelemetryManager] Modo: %d (LoRa=%d HTTP=%d Beacon=%d WDT=%ds)\n", 
                 mode, activeModeConfig->loraEnabled, activeModeConfig->httpEnabled,
                 activeModeConfig->beaconInterval > 0, wdtTimeout);
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT) return;
    if (_mission.start()) {
        _mode = MODE_FLIGHT;
        // _missionActive = true; // REMOVIDO: Redundante
        applyModeConfig(MODE_FLIGHT);
    }
}

void TelemetryManager::stopMission() {
    if (!_mission.isActive()) return; // CORRIGIDO: Checa direto no MissionController
    if (_mission.stop()) {
        _mode = MODE_PREFLIGHT;
        // _missionActive = false; // REMOVIDO: Redundante
        applyModeConfig(MODE_PREFLIGHT);
    }
}

void TelemetryManager::_sendTelemetry() {
    const GroundNodeBuffer& buf = _groundNodes.buffer();
    
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF("[TM] TX: UTC=%s | T=%.1f C | Bat=%.1f%% | Fix=%d | Nodes=%d\n",
                     _rtc.getUTCDateTime().c_str(), 
                     _telemetryData.temperature, 
                     _telemetryData.batteryPercentage,
                     _telemetryData.gpsFix,
                     buf.activeNodes);
    }
    
    _comm.sendTelemetry(_telemetryData, buf);
}

void TelemetryManager::_saveToStorage() {
    bool success = _storage.saveTelemetry(_telemetryData);
    
    if (success) {
        const GroundNodeBuffer& buf = _groundNodes.buffer();
        for(int i=0; i<buf.activeNodes; i++) {
            _storage.saveMissionData(buf.nodes[i]);
        }
    }
}

void TelemetryManager::_handleButtonEvents() {
    ButtonEvent evt = _button.update();
    if (evt == ButtonEvent::SHORT_PRESS) {
        if (_mode == MODE_FLIGHT) stopMission();
        else startMission();
    } else if (evt == ButtonEvent::LONG_PRESS) {
        applyModeConfig(MODE_SAFE);
        _mode = MODE_SAFE;
        DEBUG_PRINTLN("[TM] SAFE MODE ATIVADO (Manual)");
    }
}

void TelemetryManager::_updateLEDIndicator(unsigned long currentTime) {
    bool ledState = LOW;
    if (_mode == MODE_PREFLIGHT) ledState = HIGH; 
    else if (_mode == MODE_FLIGHT) ledState = (currentTime / 1000) % 2; 
    else if (_mode == MODE_SAFE) ledState = (currentTime / 200) % 2; 
    digitalWrite(LED_BUILTIN, ledState);
}

void TelemetryManager::_sendSafeBeacon() {
    uint8_t beacon[32];
    int offset = 0;
    
    beacon[offset++] = 0xBE;
    beacon[offset++] = 0xAC;
    beacon[offset++] = (TEAM_ID >> 8) & 0xFF;
    beacon[offset++] = TEAM_ID & 0xFF;
    beacon[offset++] = (uint8_t)_mode;
    
    uint16_t batVoltageInt = (uint16_t)(_power.getVoltage() * 100);
    beacon[offset++] = (batVoltageInt >> 8) & 0xFF;
    beacon[offset++] = batVoltageInt & 0xFF;
    
    uint32_t uptime = _systemHealth.getUptime() / 1000;
    beacon[offset++] = (uptime >> 24) & 0xFF;
    beacon[offset++] = (uptime >> 16) & 0xFF;
    beacon[offset++] = (uptime >> 8) & 0xFF;
    beacon[offset++] = uptime & 0xFF;
    
    beacon[offset++] = _systemHealth.getSystemStatus();
    
    uint16_t errors = _systemHealth.getErrorCount();
    beacon[offset++] = (errors >> 8) & 0xFF;
    beacon[offset++] = errors & 0xFF;
    
    uint32_t freeHeap = _systemHealth.getFreeHeap();
    beacon[offset++] = (freeHeap >> 24) & 0xFF;
    beacon[offset++] = (freeHeap >> 16) & 0xFF;
    beacon[offset++] = (freeHeap >> 8) & 0xFF;
    beacon[offset++] = freeHeap & 0xFF;
    
    HealthTelemetry health = _systemHealth.getHealthTelemetry();
    beacon[offset++] = (health.resetCount >> 8) & 0xFF;
    beacon[offset++] = health.resetCount & 0xFF;
    beacon[offset++] = health.resetReason;
    beacon[offset++] = _gps.hasFix() ? 1 : 0;
    
    DEBUG_PRINTLN("[TM] ENVIANDO BEACON SAFE MODE");
    if (_comm.sendLoRa(beacon, offset)) {
        DEBUG_PRINTLN("[TM] Beacon SAFE enviado com sucesso!");
    }
}

bool TelemetryManager::handleCommand(const String& cmd) {
    String cmdUpper = cmd;
    cmdUpper.toUpperCase();
    cmdUpper.trim();
    
    if (cmdUpper == "START_MISSION") {
        startMission();
        return true;
    }
    if (cmdUpper == "STOP_MISSION") {
        stopMission();
        return true;
    }
    if (cmdUpper == "SAFE_MODE") {
        applyModeConfig(MODE_SAFE);
        _mode = MODE_SAFE;
        DEBUG_PRINTLN("[TM] SAFE MODE ATIVADO (Comando)");
        return true;
    }
    if (cmdUpper == "DUTY_CYCLE") {
        auto& dc = _comm.getDutyCycleTracker();
        DEBUG_PRINTLN("=== DUTY CYCLE ===");
        DEBUG_PRINTF("Usado: %lu ms / %lu ms\n", dc.getAccumulatedTxTime(), 360000);
        DEBUG_PRINTF("Percentual: %.1f%%\n", dc.getDutyCyclePercent());
        DEBUG_PRINTLN("==================");
        return true;
    }
    return _commandHandler.handle(cmdUpper);
}

void TelemetryManager::testLoRaTransmission() { _comm.sendLoRa("TEST"); }
void TelemetryManager::sendCustomLoRa(const String& msg) { _comm.sendLoRa(msg); }
void TelemetryManager::printLoRaStats() {
    DEBUG_PRINTLN("=== LoRa Stats ===");
    DEBUG_PRINTF("SF Atual: %d\n", _comm.getCurrentSF());
    DEBUG_PRINTF("Último RSSI: %d dBm\n", _comm.getLastRSSI());
    DEBUG_PRINTF("Último SNR: %.1f dB\n", _comm.getLastSNR());
    DEBUG_PRINTLN("==================");
}