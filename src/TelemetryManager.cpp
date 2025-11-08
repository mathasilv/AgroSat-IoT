/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO DUAL MODE: FLIGHT/PREFLIGHT
 * @version 2.2.0
 * @date 2025-11-08
 */
#include "TelemetryManager.h"
#include "config.h"

// Flag global para os macros de debug serial
bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;

const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _display(OLED_ADDRESS),
    _mode(MODE_INIT),
    _lastTelemetrySend(0),
    _lastStorageSave(0),
    _lastDisplayUpdate(0),
    _lastHeapCheck(0),
    _minHeapSeen(UINT32_MAX)
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    memset(&_missionData, 0, sizeof(MissionData));
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
        DEBUG_PRINTLN("[TelemetryManager] === INICIALIZANDO I2C GLOBAL ===");
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000);
        Wire.setTimeout(500);
        delay(300);
        i2cInitialized = true;
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado em 100kHz");
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
        default:
            activeModeConfig = &PREFLIGHT_CONFIG;
            break;
    }
    currentSerialLogsEnabled = activeModeConfig->serialLogsEnabled;
}

bool TelemetryManager::begin() {
    applyModeConfig(MODE_PREFLIGHT);
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT CubeSat - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis ");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    _initI2CBus();

    bool displayOk = false;
    if (activeModeConfig->displayEnabled) {
        DEBUG_PRINTLN("[TelemetryManager] Inicializando display...");
        delay(100);
        if (_display.init()) {
            _display.flipScreenVertically();
            _display.setFont(ArialMT_Plain_10);
            _display.clear();
            _display.drawString(0, 0, "AgroSat-IoT");
            _display.drawString(0, 15, "Initializing...");
            _display.display();
            displayOk = true;
            DEBUG_PRINTLN("[TelemetryManager] Display OK");
        }
        delay(500);
    }

    bool success = true;
    uint8_t subsystemsOk = 0;
    if (_health.begin()) subsystemsOk++;
    if (_power.begin()) subsystemsOk++;
    if (_sensors.begin()) subsystemsOk++;
    if (_storage.begin()) subsystemsOk++;
    if (_payload.begin()) subsystemsOk++;
    if (_comm.begin()) subsystemsOk++;

    if (displayOk) {
        _display.clear();
        _display.drawString(0, 0, success ? "Sistema OK!" : "ERRO Sistema!");
        _display.drawString(0, 15, "Modo: PRE-FLIGHT");
        String subsysInfo = String(subsystemsOk) + "/6 subsistemas";
        _display.drawString(0, 30, subsysInfo);
        _display.display();
    }
    _mode = MODE_PREFLIGHT;
    return success;
}

void TelemetryManager::loop() {
    applyModeConfig(_mode);
    uint32_t currentTime = millis();
    _health.update(); delay(5);
    _power.update(); delay(5);
    _sensors.update(); delay(10);
    _comm.update(); delay(5);
    _payload.update(); delay(5);
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
    if (activeModeConfig->displayEnabled && (currentTime - _lastDisplayUpdate >= 2000)) {
        _lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    delay(20);
}

void TelemetryManager::startMission() {
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO!");
    _mode = MODE_FLIGHT;
    applyModeConfig(_mode);
    _health.startMission();
    _lastTelemetrySend = millis();
    _lastStorageSave = millis();
    if (activeModeConfig->displayEnabled) {
        _display.clear();
        _display.drawString(0, 0, "MISSAO INICIADA");
        _display.drawString(0, 20, "Modo: FLIGHT");
        _display.display();
    }
}

void TelemetryManager::stopMission() {
    DEBUG_PRINTLN("[TelemetryManager] MISSÃO FINALIZADA!");
    _mode = MODE_POSTFLIGHT;
    applyModeConfig(_mode);
    _sendTelemetry();
    _saveToStorage();
    if (activeModeConfig->displayEnabled) {
        _display.clear();
        _display.drawString(0, 0, "MISSAO COMPLETA");
        String heapInfo = "MIN: " + String(_minHeapSeen / 1024) + "KB";
        _display.drawString(0, 20, heapInfo);
        _display.display();
    }
}

OperationMode TelemetryManager::getMode() {
    return _mode;
}

void TelemetryManager::updateDisplay() {
    if (!activeModeConfig->displayEnabled) return;
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

// Demais métodos permanecem equivalentes, apenas condicionando chamadas de display/log conforme os novos controles
