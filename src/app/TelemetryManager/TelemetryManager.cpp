/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO OTIMIZADA PARA BALÃO METEOROLÓGICO (HAB) + UTC
 * @version 7.4.0 - HAB Edition + UTC Standard
 * @date 2025-12-01
 * 
 * CHANGELOG v7.4.0:
 * - [NEW] RTC 100% UTC (CubeSat padrão internacional)
 * - [FIX] Reset de flag forwarded ao atualizar nó
 * - [FIX] Reset periódico de flags para retransmissão contínua
 * - [NEW] Timestamp UTC em toda telemetria
 * - [REMOVED] Display manager dependencies
 */

#include "TelemetryManager.h"
#include "config.h"

bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    // Subsystems / controllers
    _sensors(),
    _power(),
    _systemHealth(),
    _rtc(),
    _button(),
    _storage(),
    _comm(),
    _groundNodes(),
    _mission(_rtc, _groundNodes),
    _telemetryCollector(_sensors, _power, _systemHealth, _rtc, _groundNodes),
    _commandHandler(_sensors),

    // State
    _mode(MODE_INIT),
    _missionActive(false),
    _lastTelemetrySend(0),
    _lastStorageSave(0),
    _missionStartTime(0),
    _lastSensorReset(0)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));

    _telemetryData.humidity = NAN;
    _telemetryData.co2      = NAN;
    _telemetryData.tvoc     = NAN;
    _telemetryData.magX     = NAN;
    _telemetryData.magY     = NAN;
    _telemetryData.magZ     = NAN;
}

// =============================
// Helpers de inicialização
// =============================

void TelemetryManager::_initModeDefaults() {
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);
}

void TelemetryManager::_initSubsystems(uint8_t& subsystemsOk, bool& success) {
    // RTC
    DEBUG_PRINTLN("[TelemetryManager] Init RTC (UTC)");
    if (_rtc.begin(&Wire)) {
        subsystemsOk++;
        DEBUG_PRINTF("[TelemetryManager] RTC OK: %s (unix=%lu)\n",
                     _rtc.getUTCDateTime().c_str(),
                     _rtc.getUnixTime());
    } else {
        success = false;
    }

    // Botão
    DEBUG_PRINTLN("[TelemetryManager] Init botao");
    _button.begin();

    // SystemHealth
    DEBUG_PRINTLN("[TelemetryManager] Init SystemHealth");
    if (_systemHealth.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] SystemHealth OK");
    } else {
        success = false;
    }

    // PowerManager
    DEBUG_PRINTLN("[TelemetryManager] Init PowerManager");
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] PowerManager OK");
    } else {
        success = false;
    }

    // SensorManager
    DEBUG_PRINTLN("[TelemetryManager] Init SensorManager");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] SensorManager OK");
        DEBUG_PRINTF("[TelemetryManager] MPU9250: %s\n", _sensors.isMPU9250Online() ? "OK" : "FAIL");
        DEBUG_PRINTF("[TelemetryManager] BMP280: %s\n", _sensors.isBMP280Online() ? "OK" : "FAIL");
        DEBUG_PRINTF("[TelemetryManager] SI7021: %s\n", _sensors.isSI7021Online() ? "OK" : "FAIL");
        DEBUG_PRINTF("[TelemetryManager] CCS811: %s\n", _sensors.isCCS811Online() ? "OK" : "FAIL");
    } else {
        success = false;
    }

    // Storage
    DEBUG_PRINTLN("[TelemetryManager] Init Storage");
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Storage OK");
    } else {
        success = false;
    }

    // Communication
    DEBUG_PRINTLN("[TelemetryManager] Init Communication");
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication OK");
    } else {
        success = false;
    }
}

void TelemetryManager::_syncNTPIfAvailable() {
    if (_rtc.isInitialized() && WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("[TelemetryManager] Sincronizando NTP");
        if (_rtc.syncWithNTP()) {
            DEBUG_PRINTF("[TelemetryManager] NTP OK, local: %s, unix=%lu\n",
                         _rtc.getDateTime().c_str(),
                         _rtc.getUnixTime());
        } else {
            DEBUG_PRINTLN("[TelemetryManager] NTP FAIL (mantendo RTC atual)");
        }
    } else {
        DEBUG_PRINTLN("[TelemetryManager] NTP indisponivel (WiFi/RTC)");
    }
}

void TelemetryManager::_logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap) {
    uint32_t postInitHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Init: %s, subsistemas=%d/6, heap=%lu bytes (usado=%lu)\n",
                 success ? "OK" : "ERRO",
                 subsystemsOk,
                 postInitHeap,
                 initialHeap - postInitHeap);
}

// =============================
// API pública
// =============================

void TelemetryManager::applyModeConfig(uint8_t modeIndex) {
    OperationMode mode = static_cast<OperationMode>(modeIndex);
    
    switch (mode) {
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
    
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    DEBUG_PRINTF("[TelemetryManager] Modo aplicado: %d | Logs: %s | LoRa: %s\n",
                 mode,
                 activeModeConfig->serialLogsEnabled ? "ON" : "OFF",
                 activeModeConfig->loraEnabled ? "ON" : "OFF");
}

bool TelemetryManager::begin() {
    uint32_t initialHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initModeDefaults();

    bool success        = true;
    uint8_t subsystemsOk = 0;

    _initSubsystems(subsystemsOk, success);
    _syncNTPIfAvailable();

    _logInitSummary(success, subsystemsOk, initialHeap);
    return success;
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();

    // Atualizar subsistemas principais
    _systemHealth.update();
    SystemHealth::HeapStatus heapStatus = _systemHealth.getHeapStatus();

    switch (heapStatus) {
        case SystemHealth::HeapStatus::CRITICAL_HEAP:
            applyModeConfig(MODE_SAFE);
            _mode = MODE_SAFE;
            _missionActive = false;
            break;

        case SystemHealth::HeapStatus::FATAL_HEAP:
            if (activeModeConfig->serialLogsEnabled) {
                DEBUG_PRINTF("[TelemetryManager] Restart por heap fatal (%lu bytes)\n",
                             ESP.getFreeHeap());
            }
            delay(3000);
            ESP.restart();
            break;

        default:
            break;
    }

    _power.update();

    // Agendamento das leituras de sensores
    const unsigned long FAST_PERIOD_MS   = 500;   
    const unsigned long SLOW_PERIOD_MS   = 2000;   
    const unsigned long HEALTH_PERIOD_MS = 60000;  

    if (currentTime - _lastFastSensorUpdate >= FAST_PERIOD_MS) {
        _lastFastSensorUpdate = currentTime;
        _sensors.updateFast();
    }

    if (currentTime - _lastSlowSensorUpdate >= SLOW_PERIOD_MS) {
        _lastSlowSensorUpdate = currentTime;
        _sensors.updateSlow();
    }

    if (currentTime - _lastSensorHealthUpdate >= HEALTH_PERIOD_MS) {
        _lastSensorHealthUpdate = currentTime;
        _sensors.updateHealth();
    }

    _comm.update();
    _rtc.update();

    _handleButtonEvents();
    _updateLEDIndicator(currentTime);

    // Recepção LoRa
    String loraPacket;
    int rssi = 0;
    float snr = 0.0f;

    if (_comm.receiveLoRaPacket(loraPacket, rssi, snr)) {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTF("[TelemetryManager] LoRa RX: RSSI=%d dBm SNR=%.1f dB\n", rssi, snr);
        }

        MissionData receivedData;
        memset(&receivedData, 0, sizeof(MissionData));

        if (_comm.processLoRaPacket(loraPacket, receivedData)) {
            receivedData.rssi       = rssi;
            receivedData.snr        = snr;
            receivedData.lastLoraRx = millis();

            if (_rtc.isInitialized()) {
                receivedData.collectionTime = _rtc.getUnixTime();
            } else {
                receivedData.collectionTime = millis() / 1000;
            }

            receivedData.forwarded = false;

            _groundNodes.updateNode(receivedData, _comm);

            if (_storage.isAvailable()) {
                _storage.saveMissionData(receivedData);
            }
            
            if (activeModeConfig->serialLogsEnabled) {
                DEBUG_PRINTF("[TelemetryManager] Nó %u recebido: Solo=%.0f%% RSSI=%d dBm\n",
                             receivedData.nodeId,
                             receivedData.soilMoisture,
                             rssi);
            }
        }
    }

    // Limpeza periódica de nós inativos
    static unsigned long lastCleanup = 0;
    if (currentTime - lastCleanup > 600000UL) {
        lastCleanup = currentTime;
        _groundNodes.cleanup(currentTime, NODE_TTL_MS);
    }

    // Reset periódico de flags de retransmissão
    static unsigned long lastFlagReset = 0;
    const unsigned long FLAG_RESET_INTERVAL = 60000UL;

    if (currentTime - lastFlagReset >= FLAG_RESET_INTERVAL) {
        lastFlagReset = currentTime;

        uint8_t resetCount = _groundNodes.resetForwardFlags();
        if (resetCount > 0 && activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTF("[TelemetryManager] %d nos prontos para retransmissao periodica\n",
                         resetCount);
        }
    }

    // Coletar telemetria e checar condições de operação
    _telemetryCollector.collect(_telemetryData);
    _checkOperationalConditions();

    // Transmissão periódica
    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }

    // Salvamento periódico em SD
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }

    delay(5);
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT || _mode == MODE_POSTFLIGHT) {
        DEBUG_PRINTLN("[TelemetryManager] Missao ja em andamento");
        return;
    }
    
    if (_mission.start()) {
        _mode = MODE_FLIGHT;
        _missionActive = true;
        applyModeConfig(MODE_FLIGHT);
        
        DEBUG_PRINTLN("[TelemetryManager] Modo FLIGHT ativado");
        DEBUG_PRINTLN("[TelemetryManager] Coleta continua de dados terrestres");
    }
}

void TelemetryManager::stopMission() {
    if (!_missionActive) {
        return;
    }
    
    if (_mission.stop()) {
        _mode = MODE_PREFLIGHT;
        _missionActive = false;
        applyModeConfig(MODE_PREFLIGHT);
        
        DEBUG_PRINTLN("[TelemetryManager] Modo PREFLIGHT restaurado");
    }
}

OperationMode TelemetryManager::getMode() {
    return _mode;
}

void TelemetryManager::_sendTelemetry() {
    const GroundNodeBuffer& buf = _groundNodes.buffer();
    
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF(
            "[TelemetryManager] TX: UTC=%s T=%.2fC P=%.2fhPa Alt=%.1fm Bat=%.1f%% Nodes=%d\n",
            _rtc.getUTCDateTime().c_str(),
            _telemetryData.temperatureBMP,
            _telemetryData.pressure,
            _telemetryData.altitude,
            _telemetryData.batteryPercentage,
            buf.activeNodes
        );
    }
    
    bool sendSuccess = _comm.sendTelemetry(_telemetryData, buf);
    
    if (activeModeConfig->serialLogsEnabled) {
        if (sendSuccess) {
            DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada");
        } else {
            DEBUG_PRINTLN("[TelemetryManager] Erro ao enviar telemetria");
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
    
    const GroundNodeBuffer& buf = _groundNodes.buffer();
    for (uint8_t i = 0; i < buf.activeNodes; i++) {
        _storage.saveMissionData(buf.nodes[i]);
    }
}

void TelemetryManager::_checkOperationalConditions() {
    if (_power.isCritical()) { 
        _systemHealth.reportError(STATUS_BATTERY_CRIT, "Critical battery level"); 
        _power.enablePowerSave(); 
    } else if (_power.isLow()) { 
        _systemHealth.reportError(STATUS_BATTERY_LOW, "Low battery level"); 
    }
    
    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck >= 60000) {
        lastSensorCheck = millis();
        
        if (!_sensors.isMPU9250Online()) { 
            _systemHealth.reportError(STATUS_SENSOR_ERROR, "IMU offline"); 
            
            if (millis() - _lastSensorReset >= 300000) {
                DEBUG_PRINTLN("[TelemetryManager] Tentando recuperacao de sensores...");
                _sensors.resetAll();
                _lastSensorReset = millis();
            }
        }
        
        if (!_sensors.isBMP280Online()) { 
            _systemHealth.reportError(STATUS_SENSOR_ERROR, "BMP280 offline"); 
        }
    }
}

void TelemetryManager::testLoRaTransmission() {
    DEBUG_PRINTLN("[TelemetryManager] Testando transmissao LoRa...");
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
    if (event == ButtonEvent::NONE) {
        return;
    }

    // Feedback visual simples no LED
    auto blinkLed = [](uint8_t times, uint16_t onMs, uint16_t offMs) {
        for (uint8_t i = 0; i < times; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(onMs);
            digitalWrite(LED_BUILTIN, LOW);
            delay(offMs);
        }
    };

    if (event == ButtonEvent::SHORT_PRESS) {
        if (_mode == MODE_PREFLIGHT || _mode == MODE_POSTFLIGHT) {
            DEBUG_PRINTLN("[TelemetryManager] Botao: START MISSION (PREFLIGHT -> FLIGHT)");
            blinkLed(3, 80, 80);
            startMission();
        } else if (_mode == MODE_FLIGHT) {
            DEBUG_PRINTLN("[TelemetryManager] Botao: STOP MISSION (FLIGHT -> PREFLIGHT)");
            blinkLed(3, 80, 80);
            stopMission();
        } else {
            DEBUG_PRINTF("[TelemetryManager] Botao ignorado no modo atual (%d)\n", _mode);
        }
    } 
    else if (event == ButtonEvent::LONG_PRESS) {
        DEBUG_PRINTLN("[TelemetryManager] Botao: SAFE MODE (long press)");
        blinkLed(5, 50, 50);

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

bool TelemetryManager::handleCommand(const String& cmd) {
    return _commandHandler.handle(cmd);
}
