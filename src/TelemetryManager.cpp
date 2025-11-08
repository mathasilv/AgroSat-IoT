/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO DUAL MODE: FLIGHT/PREFLIGHT + todas funções completas
 * @version 2.2.1
 * @date 2025-11-08
 */
#include "TelemetryManager.h"
#include "config.h"
// Flag global para logs de debug serial
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
        case MODE_PREFLIGHT: activeModeConfig = &PREFLIGHT_CONFIG; break;
        case MODE_FLIGHT: activeModeConfig = &FLIGHT_CONFIG; break;
        default: activeModeConfig = &PREFLIGHT_CONFIG; break;
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
    bool success = true; uint8_t subsystemsOk = 0;
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
    if (_mode == MODE_ERROR) { _displayError("Sistema com erro"); return; }
    if (_mode == MODE_PREFLIGHT) { _displayStatus(); } else { _displayTelemetry(); }
}
// === IMPLEMENTAÇÃO COMPLETA DOS PRIVADOS ===
void TelemetryManager::_collectTelemetryData() {
    _telemetryData.timestamp = millis();
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
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    if (_sensors.isSHT20Online()) {
        float hum = _sensors.getHumidity();
        if (!isnan(hum)) { _telemetryData.humidity = hum; }
    }
    if (_sensors.isCCS811Online()) {
        float co2 = _sensors.getCO2();
        float tvoc = _sensors.getTVOC();
        if (!isnan(co2) && co2 > 0) { _telemetryData.co2 = co2; }
        if (!isnan(tvoc) && tvoc > 0) { _telemetryData.tvoc = tvoc; }
    }
    if (_sensors.isMPU9250Online()) {
        float magX = _sensors.getMagX();
        float magY = _sensors.getMagY();
        float magZ = _sensors.getMagZ();
        if (!isnan(magX) && !isnan(magY) && !isnan(magZ)) {
            _telemetryData.magX = magX;
            _telemetryData.magY = magY;
            _telemetryData.magZ = magZ;
        }
    }
    _telemetryData.systemStatus = _health.getSystemStatus();
    _telemetryData.errorCount = _health.getErrorCount();
    String payloadStr = _payload.generatePayload();
    strncpy(_telemetryData.payload, payloadStr.c_str(), PAYLOAD_MAX_SIZE - 1);
    _telemetryData.payload[PAYLOAD_MAX_SIZE - 1] = '\0';
    _missionData = _payload.getMissionData();
}
void TelemetryManager::_sendTelemetry() {
    DEBUG_PRINTLN("[TelemetryManager] Enviando telemetria...");
    DEBUG_PRINTF("  Temp: %.2f°C, Pressão: %.2f hPa, Alt: %.1f m\n",
        _telemetryData.temperature, _telemetryData.pressure, _telemetryData.altitude);
    DEBUG_PRINTF("  Gyro: [%.4f, %.4f, %.4f] rad/s\n",
        _telemetryData.gyroX, _telemetryData.gyroY, _telemetryData.gyroZ);
    DEBUG_PRINTF("  Accel: [%.4f, %.4f, %.4f] m/s²\n",
        _telemetryData.accelX, _telemetryData.accelY, _telemetryData.accelZ);
    if (!isnan(_telemetryData.humidity)) { DEBUG_PRINTF("  Umidade: %.1f%%\n", _telemetryData.humidity); }
    if (!isnan(_telemetryData.co2)) { DEBUG_PRINTF("  CO2: %.0f ppm, TVOC: %.0f ppb\n", _telemetryData.co2, _telemetryData.tvoc); }
    if (!isnan(_telemetryData.magX)) { DEBUG_PRINTF("  Mag: [%.2f, %.2f, %.2f] µT\n", _telemetryData.magX, _telemetryData.magY, _telemetryData.magZ); }
    if (!_comm.isConnected()) { DEBUG_PRINTLN("[TelemetryManager] Tentando reconectar WiFi..."); _comm.connectWiFi(); }
    if (_comm.sendTelemetry(_telemetryData)) { DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada com sucesso!"); } else { DEBUG_PRINTLN("[TelemetryManager] Falha ao enviar telemetria"); _health.reportError(STATUS_WIFI_ERROR, "Telemetry send failed"); }
}
void TelemetryManager::_saveToStorage() {
    if (!_storage.isAvailable()) return;
    DEBUG_PRINTLN("[TelemetryManager] Salvando dados no SD...");
    if (_storage.saveTelemetry(_telemetryData)) { DEBUG_PRINTLN("[TelemetryManager] Telemetria salva no SD"); }
    if (_storage.saveMissionData(_missionData)) { DEBUG_PRINTLN("[TelemetryManager] Dados da missão salvos no SD"); }
}
void TelemetryManager::_checkOperationalConditions() {
    if (_power.isCritical()) { _health.reportError(STATUS_BATTERY_CRIT, "Critical battery level"); _power.enablePowerSave(); }
    else if (_power.isLow()) { _health.reportError(STATUS_BATTERY_LOW, "Low battery level"); }
    if (!_sensors.isMPU6050Online() && !_sensors.isMPU9250Online()) { _health.reportError(STATUS_SENSOR_ERROR, "IMU offline"); _sensors.resetAll(); }
    if (!_sensors.isBMP280Online()) { _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline"); _sensors.resetAll(); }
    if (_mode == MODE_FLIGHT && !_health.isMissionActive()) { stopMission(); }
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < 10000) { DEBUG_PRINTF("[TelemetryManager] CRÍTICO: Heap baixo: %lu bytes\n", currentHeap); _health.reportError(STATUS_WATCHDOG, "Critical low memory"); }
}
void TelemetryManager::_displayStatus() {
    _display.clear();
    String line1 = "PRE " + String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 0, line1);
    String line2;
    if (_sensors.isSHT20Online() && !isnan(_sensors.getHumidity())) {
        line2 = String(_sensors.getTemperature(), 1) + "C " + String(_sensors.getHumidity(), 0) + "%RH";
    } else {
        line2 = String(_sensors.getTemperature(), 1) + "C " + String(_sensors.getPressure(), 0) + "hPa";
    }
    _display.drawString(0, 15, line2);
    String line3;
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        line3 = "CO2: " + String(_sensors.getCO2(), 0) + "ppm";
    } else {
        line3 = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
    }
    _display.drawString(0, 30, line3);
    String line4 = "";
    line4 += _comm.isConnected() ? "W+" : "W-";
    line4 += " ";
    line4 += _payload.isOnline() ? "L+" : "L-";
    line4 += " ";
    line4 += _storage.isAvailable() ? "S+" : "S-";
    line4 += " ";
    if (_sensors.isSHT20Online()) line4 += "H+";
    if (_sensors.isCCS811Online()) line4 += "C+";
    if (_sensors.isMPU9250Online()) line4 += "9+";
    line4 += " " + String(ESP.getFreeHeap() / 1024) + "K";
    _display.drawString(0, 45, line4);
    _display.display();
}
void TelemetryManager::_displayTelemetry() {
    _display.clear();
    unsigned long missionTimeSec = _telemetryData.missionTime / 1000;
    unsigned long minutes = missionTimeSec / 60;
    unsigned long seconds = missionTimeSec % 60;
    String line1 = String(minutes) + ":" + String(seconds) + " " + String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 0, line1);
    String line2 = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
    _display.drawString(0, 15, line2);
    String line3;
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        line3 = "CO2: " + String(_sensors.getCO2(), 0) + "ppm";
    } else {
        line3 = "Temp: " + String(_sensors.getTemperature(), 1) + "C";
    }
    _display.drawString(0, 30, line3);
    String line4 = "LoRa:" + String(_missionData.packetsReceived);
    if (_sensors.isSHT20Online()) line4 += " H";
    if (_sensors.isCCS811Online()) line4 += " C";
    if (_sensors.isMPU9250Online()) line4 += " 9";
    line4 += " " + String(ESP.getFreeHeap() / 1024) + "K";
    _display.drawString(0, 45, line4);
    _display.display();
}
void TelemetryManager::_displayError(const String& error) {
    _display.clear();
    _display.drawString(0, 0, "ERRO:");
    _display.drawString(0, 15, error);
    String heapInfo = "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB";
    _display.drawString(0, 30, heapInfo);
    String sensorStatus = "";
    sensorStatus += _sensors.isBMP280Online() ? "B+" : "B-";
    sensorStatus += _sensors.isMPU6050Online() || _sensors.isMPU9250Online() ? " I+" : " I-";
    _display.drawString(0, 45, sensorStatus);
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
    uint8_t sensorsActive = 0;
    if (_sensors.isMPU6050Online()) sensorsActive++;
    if (_sensors.isMPU9250Online()) sensorsActive++;
    if (_sensors.isBMP280Online()) sensorsActive++;
    if (_sensors.isSHT20Online()) sensorsActive++;
    if (_sensors.isCCS811Online()) sensorsActive++;
    DEBUG_PRINTF("[TelemetryManager] Sensores ativos: %d\n", sensorsActive);
    if (currentHeap < 15000) { DEBUG_PRINTLN("[TelemetryManager] AVISO: Heap baixo!"); }
}
