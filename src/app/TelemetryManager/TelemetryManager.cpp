/**
 * @file TelemetryManager.cpp
 * @brief Gerenciador Central (FIX: Timeouts e logs de mutex)
 * @version 11.1.0
 */

#include "TelemetryManager.h"
#include "config.h"
#include "Globals.h" 

// currentSerialLogsEnabled definido em Globals.cpp
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

// Contadores de falhas de mutex para diagnóstico
static uint16_t s_dataMutexTimeouts = 0;
static uint16_t s_i2cMutexTimeouts = 0;

TelemetryManager::TelemetryManager() :
    _sensors(), _gps(), _power(), _systemHealth(), _rtc(), 
    _button(), _storage(), _comm(), _groundNodes(),
    _mission(_rtc, _groundNodes),
    _telemetryCollector(_sensors, _gps, _power, _systemHealth, _rtc, _groundNodes, _mission),
    _commandHandler(_sensors),
    _mode(MODE_INIT), 
    _lastTelemetrySend(0), _lastStorageSave(0),
    _lastBeaconTime(0)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    _telemetryData.humidity = NAN; _telemetryData.co2 = NAN; _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN; _telemetryData.magY = NAN; _telemetryData.magZ = NAN;
    _telemetryData.latitude = 0.0; _telemetryData.longitude = 0.0;
    _telemetryData.gpsAltitude = 0.0; _telemetryData.satellites = 0; _telemetryData.gpsFix = false;
}

bool TelemetryManager::begin() {
    uint32_t initialHeap = ESP.getFreeHeap();
    _initModeDefaults();

    bool success = true;
    uint8_t subsystemsOk = 0;

    _initSubsystems(subsystemsOk, success);
    _syncNTPIfAvailable();

    if (_mission.begin()) { 
        DEBUG_PRINTLN("[TelemetryManager] Restaurando modo FLIGHT...");
        _mode = MODE_FLIGHT;
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
        _storage.setSystemHealth(&_systemHealth); 
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

// === Atualização Física (Task Sensores) ===
// FIX: Timeout aumentado de 100ms para 200ms e logs de falha adicionados
void TelemetryManager::updatePhySensors() {
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            _sensors.update(); 
            _rtc.update();
            xSemaphoreGive(xI2CMutex);
        } else {
            // FIX: Log de falha e contagem
            s_i2cMutexTimeouts++;
            if (s_i2cMutexTimeouts % 10 == 1) { // Log a cada 10 falhas
                DEBUG_PRINTF("[TM] AVISO: I2C mutex timeout (#%u)\n", s_i2cMutexTimeouts);
            }
            _systemHealth.incrementI2CError();
        }
        _gps.update();
        _power.update();
        _power.adjustCpuFrequency();
        xSemaphoreGive(xDataMutex);
    } else {
        // FIX: Log de falha
        s_dataMutexTimeouts++;
        if (s_dataMutexTimeouts % 10 == 1) {
            DEBUG_PRINTF("[TM] AVISO: Data mutex timeout (#%u)\n", s_dataMutexTimeouts);
        }
    }
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();

    _systemHealth.update(); 
    SystemHealth::HeapStatus heapStatus = _systemHealth.getHeapStatus();
    switch (heapStatus) {
        case SystemHealth::HeapStatus::HEAP_CRITICAL:
            if (_mode != MODE_SAFE) {
                DEBUG_PRINTLN("[TelemetryManager] MEMORIA CRITICA! Entrando em SAFE MODE.");
                applyModeConfig(MODE_SAFE);
                _mode = MODE_SAFE;
            }
            break;
        case SystemHealth::HeapStatus::HEAP_FATAL:
            DEBUG_PRINTLN("[TelemetryManager] MEMORIA FATAL. Reiniciando...");
            delay(1000);
            ESP.restart();
            break;
        default: break;
    }

    _comm.update();

    _handleButtonEvents();
    _updateLEDIndicator(currentTime);

    _handleIncomingRadio();
    _maintainGroundNetwork();

    // FIX: Timeout aumentado de 50ms para 100ms
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _telemetryCollector.collect(_telemetryData);
        xSemaphoreGive(xDataMutex);
    }

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
        if (activeModeConfig->beaconInterval > 0 && (currentTime - _lastBeaconTime >= activeModeConfig->beaconInterval)) {
            _sendSafeBeacon();
            _lastBeaconTime = currentTime;
        }
    }
}

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
            DEBUG_PRINTLN("[TM] Sensores instaveis. Tentando reset...");
            // FIX: Timeout aumentado e convertido para ms
            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                _sensors.resetAll();
                xSemaphoreGive(xI2CMutex);
            }
            lastSensorReset = millis();
        }
    }
    _systemHealth.setSystemError(STATUS_SENSOR_ERROR, sensorFail);
    
    if (activeModeConfig->httpEnabled) {
        bool wifiDown = !_comm.isWiFiConnected();
        if (wifiDown) {
            static unsigned long lastWifiRetry = 0;
            if (millis() - lastWifiRetry > 30000) {
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
            
            _groundNodes.updateNode(rxData);
            _storage.saveMissionData(rxData); 
            
            DEBUG_PRINTF("[TM] Node %u RX: RSSI=%d dBm, SNR=%.1f dB\n", 
                         rxData.nodeId, rssi, snr);
        }
    }
}

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
            activeModeConfig = &PREFLIGHT_CONFIG; wdtTimeout = WATCHDOG_TIMEOUT_PREFLIGHT; break;
        case MODE_FLIGHT:    
            activeModeConfig = &FLIGHT_CONFIG; wdtTimeout = WATCHDOG_TIMEOUT_FLIGHT; break;
        case MODE_SAFE:      
            activeModeConfig = &SAFE_CONFIG; wdtTimeout = WATCHDOG_TIMEOUT_SAFE; break;
        default:             
            activeModeConfig = &PREFLIGHT_CONFIG; wdtTimeout = WATCHDOG_TIMEOUT_PREFLIGHT; break;
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
        DEBUG_PRINTLN("[TelemetryManager] Transicao para MODE_FLIGHT confirmada.");
        _mode = MODE_FLIGHT;
        applyModeConfig(MODE_FLIGHT);
    }
}

void TelemetryManager::stopMission() {
    if (!_mission.isActive()) return;
    if (_mission.stop()) {
        DEBUG_PRINTLN("[TelemetryManager] Missao finalizada. Retornando para PREFLIGHT.");
        _mode = MODE_PREFLIGHT;
        applyModeConfig(MODE_PREFLIGHT);
    }
}

void TelemetryManager::_sendTelemetry() {
    GroundNodeBuffer& buf = _groundNodes.buffer();
    
    if (activeModeConfig->serialLogsEnabled) {
        String ts = _rtc.getUTCDateTime(); 
        DEBUG_PRINTF("[TM] TX: UTC=%s | T=%.1f C | Bat=%.1f%% | Fix=%d | Nodes=%d\n",
                     ts.c_str(), 
                     _telemetryData.temperature, 
                     _telemetryData.batteryPercentage,
                     _telemetryData.gpsFix,
                     buf.activeNodes);
    }
    _comm.sendTelemetry(_telemetryData, buf);
}

// Buffer estatico para dados de storage (evita copia na fila)
static TelemetryData s_storageData;
static GroundNodeBuffer s_storageNodes;
static SemaphoreHandle_t s_storageMutex = NULL;

void TelemetryManager::_saveToStorage() {
    // Criar mutex na primeira chamada
    if (s_storageMutex == NULL) {
        s_storageMutex = xSemaphoreCreateMutex();
    }
    
    if (xStorageQueue != NULL && s_storageMutex != NULL) {
        // Copiar dados para buffer estatico (protegido por mutex proprio)
        if (xSemaphoreTake(s_storageMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            memcpy(&s_storageData, &_telemetryData, sizeof(TelemetryData));
            s_storageNodes = _groundNodes.buffer();
            xSemaphoreGive(s_storageMutex);
            
            // Envia sinal para a task processar
            uint8_t signal = 1;
            if (xQueueSend(xStorageQueue, &signal, 0) != pdTRUE) {
                DEBUG_PRINTLN("[TM] AVISO: Fila SD cheia.");
            }
        }
    }
}

void TelemetryManager::processStoragePacket(const StorageQueueMessage& msg) {
    (void)msg;
    
    if (s_storageMutex == NULL) return;
    
    TelemetryData localData;
    GroundNodeBuffer localNodes;
    
    if (xSemaphoreTake(s_storageMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        memcpy(&localData, &s_storageData, sizeof(TelemetryData));
        localNodes = s_storageNodes;
        xSemaphoreGive(s_storageMutex);
    } else {
        return;
    }
    
    // Verificar se dados sao validos (timestamp ou bateria devem ter valor)
    if (localData.timestamp == 0 && localData.batteryVoltage < 0.1f) {
        return; // Dados ainda nao coletados
    }
    
    bool saved = _storage.saveTelemetry(localData);
    if (saved) {
        for(int i=0; i<localNodes.activeNodes; i++) {
            _storage.saveMissionData(localNodes.nodes[i]);
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
    
    beacon[offset++] = 0xBE; beacon[offset++] = 0xAC;
    beacon[offset++] = (TEAM_ID >> 8) & 0xFF; beacon[offset++] = TEAM_ID & 0xFF;
    beacon[offset++] = (uint8_t)_mode;
    
    uint16_t batVoltageInt = (uint16_t)(_power.getVoltage() * 100);
    beacon[offset++] = (batVoltageInt >> 8) & 0xFF; beacon[offset++] = batVoltageInt & 0xFF;
    
    uint32_t uptime = _systemHealth.getUptime() / 1000;
    beacon[offset++] = (uptime >> 24) & 0xFF; beacon[offset++] = (uptime >> 16) & 0xFF;
    beacon[offset++] = (uptime >> 8) & 0xFF; beacon[offset++] = uptime & 0xFF;
    
    beacon[offset++] = _systemHealth.getSystemStatus();
    
    uint16_t errors = _systemHealth.getErrorCount();
    beacon[offset++] = (errors >> 8) & 0xFF; beacon[offset++] = errors & 0xFF;
    
    uint32_t freeHeap = _systemHealth.getFreeHeap();
    beacon[offset++] = (freeHeap >> 24) & 0xFF; beacon[offset++] = (freeHeap >> 16) & 0xFF;
    beacon[offset++] = (freeHeap >> 8) & 0xFF; beacon[offset++] = freeHeap & 0xFF;
    
    HealthTelemetryExtended health = _systemHealth.getHealthTelemetry();
    beacon[offset++] = (health.resetCount >> 8) & 0xFF; beacon[offset++] = health.resetCount & 0xFF;
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

    // Protecao contra buffer overflow
    if (cmdUpper.length() > 32) return false;
    
    if (cmdUpper == "START_MISSION") { startMission(); return true; }
    if (cmdUpper == "STOP_MISSION") { stopMission(); return true; }
    if (cmdUpper == "SAFE_MODE") { applyModeConfig(MODE_SAFE); _mode = MODE_SAFE; return true; }
    if (cmdUpper == "DUTY_CYCLE") {
        auto& dc = _comm.getDutyCycleTracker();
        DEBUG_PRINTLN("=== DUTY CYCLE ===");
        DEBUG_PRINTF("Usado: %lu ms / %lu ms\n", dc.getAccumulatedTxTime(), 360000UL);
        DEBUG_PRINTF("Percentual: %.1f%%\n", dc.getDutyCyclePercent());
        DEBUG_PRINTLN("==================");
        return true;
    }
    // FIX: Comando para ver estatísticas de mutex
    if (cmdUpper == "MUTEX_STATS") {
        DEBUG_PRINTLN("=== MUTEX STATS ===");
        DEBUG_PRINTF("Data Mutex Timeouts: %u\n", s_dataMutexTimeouts);
        DEBUG_PRINTF("I2C Mutex Timeouts: %u\n", s_i2cMutexTimeouts);
        DEBUG_PRINTLN("===================");
        return true;
    }
    return _commandHandler.handle(cmdUpper);
}
