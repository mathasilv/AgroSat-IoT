/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO DUAL MODE: FLIGHT/PREFLIGHT + SENSORES PION REAIS
 * @version 3.0.0
 * @date 2025-11-09
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
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(50000);  // 50kHz para SI7021
        Wire.setTimeout(5000);
        delay(300);
        i2cInitialized = true;
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado");
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
    
    if (!activeModeConfig->displayEnabled) {
        _display.displayOff();
    } else {
        _display.displayOn();
    }
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

    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ System Health OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! System Health FAILED");
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ Power Manager OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! Power Manager FAILED");
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ Sensor Manager OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! Sensor Manager FAILED");
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ Storage Manager OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! Storage Manager FAILED");
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Payload Manager...");
    if (_payload.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ Payload Manager OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! Payload Manager FAILED");
    }

    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] ✓ Communication Manager OK");
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] ! Communication Manager FAILED");
    }

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
        delay(1500);
        _display.displayOff();
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
    
    // Inicializar opcionais como NAN
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    if (_sensors.isSI7021Online()) {
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
    
    if (!_comm.isConnected()) { 
        DEBUG_PRINTLN("[TelemetryManager] Tentando reconectar WiFi..."); 
        _comm.connectWiFi(); 
    }
    
    if (_comm.sendTelemetry(_telemetryData)) { 
        DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada com sucesso!"); 
    } else { 
        DEBUG_PRINTLN("[TelemetryManager] Falha ao enviar telemetria"); 
        _health.reportError(STATUS_WIFI_ERROR, "Telemetry send failed"); 
    }
}

void TelemetryManager::_saveToStorage() {
    if (!_storage.isAvailable()) return;
    DEBUG_PRINTLN("[TelemetryManager] Salvando dados no SD...");
    if (_storage.saveTelemetry(_telemetryData)) { 
        DEBUG_PRINTLN("[TelemetryManager] Telemetria salva no SD"); 
    }
    if (_storage.saveMissionData(_missionData)) { 
        DEBUG_PRINTLN("[TelemetryManager] Dados da missão salvos no SD"); 
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
    
    if (_mode == MODE_FLIGHT && !_health.isMissionActive()) { 
        stopMission(); 
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

    String co2Str;
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        co2Str = "CO2: " + String(_sensors.getCO2(), 0) + "ppm TVOC: " + String(_sensors.getTVOC(), 0) + "ppb";
    } else {
        co2Str = "Altitude: " + String(_sensors.getAltitude(), 0) + "m";
    }
    _display.drawString(0, 36, co2Str);

    String statusStr = "";
    statusStr += _comm.isConnected() ? "[WiFi]" : "[NoWiFi]";
    statusStr += _payload.isOnline() ? "[Payload]" : "[NoPayload]";
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

    String batStr = "Bat: " + String(_power.getPercentage(), 0) + "% (" + String(_power.getVoltage(), 2) + "V)";
    _display.drawString(0, 12, batStr);

    String altStr = "Alt: " + String(_sensors.getAltitude(), 0) + " m";
    _display.drawString(0, 24, altStr);

    String tempStr = "Temp: " + String(_sensors.getTemperature(), 1) + " C";
    _display.drawString(0, 36, tempStr);

    String co2Str = "";
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        co2Str = "CO2: " + String(_sensors.getCO2(), 0) + " ppm TVOC: " + String(_sensors.getTVOC(), 0) + " ppb";
        _display.drawString(0, 48, co2Str);
    }

    String bottomLine = "LoRa pkts: " + String(_missionData.packetsReceived) + "  Heap: " + String(ESP.getFreeHeap() / 1024) + "K";
    _display.drawString(0, 56, bottomLine);

    _display.display();
}

void TelemetryManager::_displayError(const String& error) {
    _display.clear();
    _display.setFont(ArialMT_Plain_10);
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
    if (_sensors.isSI7021Online()) sensorsActive++;
    if (_sensors.isCCS811Online()) sensorsActive++;
    
    DEBUG_PRINTF("[TelemetryManager] Sensores ativos: %d\n", sensorsActive);
    if (currentHeap < 15000) { 
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Heap baixo!"); 
    }
}
