/**
 * @file TelemetryManager.cpp
 * @brief Gerenciador Central (Versão com GPS)
 */

#include "TelemetryManager.h"
#include "config.h"

// Variáveis Globais de Configuração (extern em config.h)
bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _sensors(), 
    _gps(), // [NOVO] Inicializa GPS
    _power(), _systemHealth(), _rtc(), _button(),
    _storage(), _comm(), _groundNodes(),
    _mission(_rtc, _groundNodes),
    // [NOVO] Passando _gps para o coletor (atualizaremos o Collector a seguir)
    _telemetryCollector(_sensors, _gps, _power, _systemHealth, _rtc, _groundNodes),
    _commandHandler(_sensors),
    _mode(MODE_INIT), _missionActive(false),
    _lastTelemetrySend(0), _lastStorageSave(0),
    _missionStartTime(0), _lastSensorReset(0)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    // Inicializa floats com NAN para indicar "sem leitura"
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN; _telemetryData.magY = NAN; _telemetryData.magZ = NAN;
    
    // [NOVO] Inicializa campos do GPS
    _telemetryData.latitude = 0.0;
    _telemetryData.longitude = 0.0;
    _telemetryData.gpsAltitude = 0.0;
    _telemetryData.satellites = 0;
    _telemetryData.gpsFix = false;
}

// === Inicialização ===

bool TelemetryManager::begin() {
    uint32_t initialHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initModeDefaults();

    bool success = true;
    uint8_t subsystemsOk = 0;

    _initSubsystems(subsystemsOk, success);
    _syncNTPIfAvailable();

    // === RECUPERAÇÃO DE MISSÃO ===
    if (_mission.begin()) { 
        DEBUG_PRINTLN("[TelemetryManager] Restaurando modo FLIGHT...");
        _mode = MODE_FLIGHT;
        _missionActive = true;
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
    // 1. RTC (Core)
    DEBUG_PRINTLN("[TelemetryManager] Init RTC (UTC)");
    if (_rtc.begin(&Wire)) subsystemsOk++;
    else success = false;

    // 2. Botão
    DEBUG_PRINTLN("[TelemetryManager] Init botao");
    _button.begin();

    // 3. SystemHealth
    DEBUG_PRINTLN("[TelemetryManager] Init SystemHealth");
    if (_systemHealth.begin()) subsystemsOk++;
    else success = false;

    // 4. Power
    DEBUG_PRINTLN("[TelemetryManager] Init PowerManager");
    if (_power.begin()) subsystemsOk++;
    else success = false;

    // 5. Sensores
    DEBUG_PRINTLN("[TelemetryManager] Init SensorManager");
    if (_sensors.begin()) subsystemsOk++;
    else success = false;
    
    // 6. GPS (NOVO)
    DEBUG_PRINTLN("[TelemetryManager] Init GPSManager");
    if (_gps.begin()) subsystemsOk++;
    // Nota: Falha no GPS não é fatal para o boot (não seta success = false)

    // 7. Storage
    DEBUG_PRINTLN("[TelemetryManager] Init Storage");
    if (_storage.begin()) {
        _storage.setRTCManager(&_rtc); 
        subsystemsOk++;
    } else success = false;

    // 8. Comunicação
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
    // Atualizado para 8 subsistemas
    DEBUG_PRINTF("[TelemetryManager] Init: %s, subsistemas=%d/8, heap usado=%lu bytes\n",
                 success ? "OK" : "ERRO", subsystemsOk, used);
}

// === Loop Principal ===

void TelemetryManager::loop() {
    uint32_t currentTime = millis();

    // 1. Monitoramento de Saúde
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

    // 2. Atualização de Subsistemas
    _power.update();
    _sensors.update(); 
    _gps.update(); // [NOVO] Processamento Serial do GPS
    _comm.update();
    _rtc.update();

    _handleButtonEvents();
    _updateLEDIndicator(currentTime);

    // 3. Gerenciamento de Rádio e Rede
    _handleIncomingRadio();
    _maintainGroundNetwork();

    // 4. Coleta e Envio de Telemetria (Local + Satélite)
    _telemetryCollector.collect(_telemetryData);
    _checkOperationalConditions();

    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }

    // 5. Salvamento Local (SD)
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
}

// === Métodos Privados de Organização ===

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
            
            _groundNodes.updateNode(rxData, _comm);
            if (_storage.isAvailable()) _storage.saveMissionData(rxData);
            
            DEBUG_PRINTF("[TM] Node %u RX: RSSI=%d dBm\n", rxData.nodeId, rssi);
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
    switch (mode) {
        case MODE_PREFLIGHT: activeModeConfig = &PREFLIGHT_CONFIG; break;
        case MODE_FLIGHT:    activeModeConfig = &FLIGHT_CONFIG; break;
        case MODE_SAFE:      activeModeConfig = &SAFE_CONFIG; break;
        default:             activeModeConfig = &PREFLIGHT_CONFIG; break;
    }
    
    currentSerialLogsEnabled = activeModeConfig->serialLogsEnabled;
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    DEBUG_PRINTF("[TelemetryManager] Modo: %d (LoRa=%d HTTP=%d)\n", 
                 mode, activeModeConfig->loraEnabled, activeModeConfig->httpEnabled);
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT) return;
    if (_mission.start()) {
        _mode = MODE_FLIGHT;
        _missionActive = true;
        applyModeConfig(MODE_FLIGHT);
    }
}

void TelemetryManager::stopMission() {
    if (!_missionActive) return;
    if (_mission.stop()) {
        _mode = MODE_PREFLIGHT;
        _missionActive = false;
        applyModeConfig(MODE_PREFLIGHT);
    }
}

void TelemetryManager::_sendTelemetry() {
    const GroundNodeBuffer& buf = _groundNodes.buffer();
    
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF("[TM] TX: UTC=%s | T=%.1f C | Bat=%.1f%% | Fix=%d\n",
                     _rtc.getUTCDateTime().c_str(), 
                     _telemetryData.temperature, 
                     _telemetryData.batteryPercentage,
                     _telemetryData.gpsFix); // [NOVO] Log de Fix
    }
    
    _comm.sendTelemetry(_telemetryData, buf);
}

void TelemetryManager::_saveToStorage() {
    if (_storage.isAvailable()) {
        _storage.saveTelemetry(_telemetryData);
        // Salva nós terrestres ativos também
        const GroundNodeBuffer& buf = _groundNodes.buffer();
        for(int i=0; i<buf.activeNodes; i++) {
            _storage.saveMissionData(buf.nodes[i]);
        }
    }
}

void TelemetryManager::_checkOperationalConditions() {
    if (_power.isCritical()) {
        _systemHealth.reportError(STATUS_BATTERY_CRIT, "Bateria Crítica");
        _power.enablePowerSave();
    }
    
    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck > 60000) {
        lastSensorCheck = millis();
        if (!_sensors.isMPU9250Online()) {
            DEBUG_PRINTLN("[TM] MPU Offline. Tentando resetar sensores...");
            _sensors.resetAll();
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

bool TelemetryManager::handleCommand(const String& cmd) {
    return _commandHandler.handle(cmd);
}

// Stubs para compatibilidade
void TelemetryManager::testLoRaTransmission() { _comm.sendLoRa("TEST"); }
void TelemetryManager::sendCustomLoRa(const String& msg) { _comm.sendLoRa(msg); }
void TelemetryManager::printLoRaStats() {}