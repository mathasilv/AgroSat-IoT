/**
 * @file TelemetryManager.cpp
 * @brief Dual-Mode Operation: PREFLIGHT verbose / FLIGHT lean
 * @version 2.3.0
 * @date 2025-11-07
 * 
 * CHANGELOG v2.3.0:
 * - Implementado sistema dual-mode com logging condicional
 * - PREFLIGHT: Logs verbosos completos + display ativo
 * - FLIGHT: Logs mínimos (apenas erros) + display desligado
 * - Otimização de consumo energético em modo FLIGHT
 */

#include "TelemetryManager.h"

// ============================================================================
// GLOBAL FLAG - Controle de modo (usado pelas macros em config.h)
// ============================================================================
bool g_isFlightMode = false;

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
        LOG_ALWAYS("[TelemetryManager] === INICIALIZANDO I2C GLOBAL ===\n");
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000);
        Wire.setTimeout(500);
        delay(300);
        i2cInitialized = true;
        LOG_ALWAYS("[TelemetryManager] I2C inicializado em 100kHz\n");
    }
    
    return true;
}

bool TelemetryManager::begin() {
    LOG_ALWAYS("\n");
    LOG_ALWAYS("========================================\n");
    LOG_ALWAYS("  AgroSat-IoT CubeSat - OBSAT Fase 2\n");
    LOG_ALWAYS("  Equipe: Orbitalis\n");
    LOG_ALWAYS("  Firmware: " FIRMWARE_VERSION "\n");
    LOG_ALWAYS("========================================\n\n");
    
    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    LOG_ALWAYS("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);
    
    _initI2CBus();
    
    LOG_ALWAYS("[TelemetryManager] Inicializando display...\n");
    bool displayOk = false;
    delay(100);
    
    if (_display.init()) {
        _display.flipScreenVertically();
        _display.setFont(ArialMT_Plain_10);
        _display.clear();
        _display.drawString(0, 0, "AgroSat-IoT");
        _display.drawString(0, 15, "Initializing...");
        _display.display();
        displayOk = true;
        LOG_ALWAYS("[TelemetryManager] Display OK\n");
    } else {
        LOG_ALWAYS("[TelemetryManager] AVISO: Display falhou\n");
    }
    
    delay(500);
    
    bool success = true;
    uint8_t subsystemsOk = 0;
    
    LOG_ALWAYS("[TelemetryManager] Inicializando System Health...\n");
    if (_health.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] System Health OK\n");
    } else {
        LOG_ERROR("System Health falhou!\n");
        success = false;
    }
    _logHeapUsage("System Health");
    delay(100);
    
    LOG_ALWAYS("[TelemetryManager] Inicializando Power Manager...\n");
    if (_power.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] Power Manager OK\n");
    } else {
        LOG_ERROR("Power Manager falhou!\n");
        _health.reportError(STATUS_BATTERY_LOW, "Power Manager init failed");
        success = false;
    }
    _logHeapUsage("Power Manager");
    delay(100);
    
    LOG_ALWAYS("[TelemetryManager] Inicializando Sensor Manager...\n");
    if (_sensors.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] Sensor Manager OK\n\n");
        
        LOG_ALWAYS("========== SENSORES DETECTADOS ==========\n");
        if (_sensors.isMPU6050Online()) LOG_ALWAYS("✓ MPU6050 (IMU 6-DOF)\n");
        if (_sensors.isMPU9250Online()) LOG_ALWAYS("✓ MPU9250 (IMU 9-DOF)\n");
        if (_sensors.isBMP280Online()) LOG_ALWAYS("✓ BMP280 (Pressão/Temp)\n");
        if (_sensors.isSHT20Online()) LOG_ALWAYS("✓ SHT20 (Temp/Umidade)\n");
        if (_sensors.isCCS811Online()) LOG_ALWAYS("✓ CCS811 (CO2/TVOC)\n");
        LOG_ALWAYS("========================================\n\n");
    } else {
        LOG_ERROR("Sensor Manager falhou!\n");
        _health.reportError(STATUS_SENSOR_ERROR, "Sensor Manager init failed");
        success = false;
    }
    _logHeapUsage("Sensor Manager");
    delay(200);
    
    LOG_ALWAYS("[TelemetryManager] Inicializando Storage Manager...\n");
    if (_storage.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] Storage Manager OK\n");
    } else {
        LOG_ALWAYS("[TelemetryManager] AVISO: Storage Manager falhou - sem SD\n");
        _health.reportError(STATUS_SD_ERROR, "Storage Manager init failed");
    }
    _logHeapUsage("Storage Manager");
    delay(100);
    
    LOG_ALWAYS("[TelemetryManager] Inicializando Payload Manager...\n");
    if (_payload.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] Payload Manager OK\n");
    } else {
        LOG_ALWAYS("[TelemetryManager] AVISO: Payload Manager falhou - sem LoRa\n");
        _health.reportError(STATUS_LORA_ERROR, "Payload Manager init failed");
    }
    _logHeapUsage("Payload Manager");
    delay(100);
    
    LOG_ALWAYS("[TelemetryManager] Inicializando Communication Manager...\n");
    if (_comm.begin()) {
        subsystemsOk++;
        LOG_ALWAYS("[TelemetryManager] Communication Manager OK\n");
    } else {
        LOG_ALWAYS("[TelemetryManager] AVISO: Communication Manager falhou!\n");
        _health.reportError(STATUS_WIFI_ERROR, "Communication Manager init failed");
    }
    _logHeapUsage("Communication Manager");
    
    if (displayOk) {
        _display.clear();
        if (success) {
            _display.drawString(0, 0, "Sistema OK!");
            _display.drawString(0, 15, "Modo: PRE-FLIGHT");
            String subsysInfo = String(subsystemsOk) + "/6 subsistemas";
            _display.drawString(0, 30, subsysInfo);
            _mode = MODE_PREFLIGHT;
        } else {
            _display.drawString(0, 0, "ERRO Sistema!");
            _display.drawString(0, 15, "Verificar logs");
            String subsysInfo = String(subsystemsOk) + "/6 OK";
            _display.drawString(0, 30, subsysInfo);
            _mode = MODE_ERROR;
        }
        _display.display();
    }
    
    LOG_ALWAYS("\n========================================\n");
    LOG_ALWAYS("  Status: %s (%d/6 subsistemas)\n", success ? "OK" : "ERRO", subsystemsOk);
    LOG_ALWAYS("  Heap final: %lu bytes\n", ESP.getFreeHeap());
    LOG_ALWAYS("========================================\n\n");
    
    return success;
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();
    
    // Heap check com intervalo ajustado por modo
    uint32_t heapInterval = g_isFlightMode ? FLIGHT_HEAP_CHECK_MS : PREFLIGHT_HEAP_CHECK_MS;
    if (currentTime - _lastHeapCheck >= heapInterval) {
        _lastHeapCheck = currentTime;
        _monitorHeap();
    }
    
    _health.update();
    delay(5);
    
    _power.update();
    delay(5);
    
    _sensors.update();
    delay(10);
    
    _comm.update();
    delay(5);
    
    _payload.update();
    delay(5);
    
    _collectTelemetryData();
    _checkOperationalConditions();
    
    if (currentTime - _lastTelemetrySend >= TELEMETRY_SEND_INTERVAL) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    if (currentTime - _lastStorageSave >= STORAGE_SAVE_INTERVAL) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // Display update APENAS em PREFLIGHT
    uint32_t displayInterval = g_isFlightMode ? FLIGHT_DISPLAY_UPDATE_MS : PREFLIGHT_DISPLAY_UPDATE_MS;
    if (!g_isFlightMode && displayInterval > 0 && (currentTime - _lastDisplayUpdate >= displayInterval)) {
        _lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    
    delay(20);
}

void TelemetryManager::startMission() {
    LOG_ALWAYS("\n========================================\n");
    LOG_ALWAYS("[TelemetryManager] TRANSIÇÃO: PREFLIGHT → FLIGHT\n");
    LOG_ALWAYS("========================================\n\n");
    
    // Último diagnóstico PREFLIGHT
    LOG_PREFLIGHT("========== STATUS PRÉ-MISSÃO (ÚLTIMA VERIFICAÇÃO) ==========\n");
    LOG_PREFLIGHT("Heap: %lu KB (mínimo visto: %lu KB)\n", ESP.getFreeHeap()/1024, _minHeapSeen/1024);
    LOG_PREFLIGHT("Bateria: %.2fV (%.0f%%)\n", _power.getVoltage(), _power.getPercentage());
    LOG_PREFLIGHT("Sensores ativos:\n");
    if (_sensors.isMPU6050Online()) LOG_PREFLIGHT("  ✓ MPU6050\n");
    if (_sensors.isBMP280Online()) LOG_PREFLIGHT("  ✓ BMP280\n");
    if (_sensors.isSHT20Online()) LOG_PREFLIGHT("  ✓ SHT20\n");
    if (_sensors.isCCS811Online()) LOG_PREFLIGHT("  ✓ CCS811\n");
    LOG_PREFLIGHT("WiFi: %s\n", _comm.isConnected() ? "Conectado" : "Desconectado");
    LOG_PREFLIGHT("SD Card: %s\n", _storage.isAvailable() ? "OK" : "Falha");
    LOG_PREFLIGHT("LoRa: %s\n", _payload.isOnline() ? "Online" : "Offline");
    LOG_PREFLIGHT("===========================================================\n\n");
    
    // Ativar modo FLIGHT
    _mode = MODE_FLIGHT;
    g_isFlightMode = true;
    _health.startMission();
    
    // Configurar LEAN MODE
    LOG_ALWAYS("[Flight] Ativando LEAN MODE:\n");
    
    if (!FLIGHT_ENABLE_DISPLAY) {
        _display.displayOff();
        LOG_ALWAYS("  - Display OLED: DESLIGADO (economia ~15mA)\n");
    }
    
    LOG_ALWAYS("  - Serial logging: MÍNIMO (apenas erros)\n");
    LOG_ALWAYS("  - Heap check: %lus interval\n", FLIGHT_HEAP_CHECK_MS/1000);
    LOG_ALWAYS("[Flight] Configuração lean concluída.\n\n");
    
    _lastTelemetrySend = millis();
    _lastStorageSave = millis();
    
    LOG_ALWAYS("[Flight] MISSÃO INICIADA | T+0s\n");
    LOG_ALWAYS("[Flight] Próxima telemetria em %lus\n\n", TELEMETRY_SEND_INTERVAL/1000);
}

void TelemetryManager::stopMission() {
    LOG_ALWAYS("\n[TelemetryManager] MISSÃO FINALIZADA!\n");
    
    _mode = MODE_POSTFLIGHT;
    g_isFlightMode = false;
    
    _sendTelemetry();
    _saveToStorage();
    
    uint32_t finalHeap = ESP.getFreeHeap();
    LOG_ALWAYS("[TelemetryManager] Heap final: %lu KB, mínimo: %lu KB\n", 
               finalHeap/1024, _minHeapSeen/1024);
    
    if (PREFLIGHT_ENABLE_DISPLAY) {
        _display.displayOn();
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
    if (_mode == MODE_ERROR) {
        _displayError("Sistema com erro");
        return;
    }
    
    if (_mode == MODE_PREFLIGHT) {
        _displayStatus();
    } else if (_mode == MODE_FLIGHT && FLIGHT_ENABLE_DISPLAY) {
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
    
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    if (_sensors.isSHT20Online()) {
        float hum = _sensors.getHumidity();
        if (!isnan(hum)) _telemetryData.humidity = hum;
    }
    
    if (_sensors.isCCS811Online()) {
        float co2 = _sensors.getCO2();
        float tvoc = _sensors.getTVOC();
        if (!isnan(co2) && co2 > 0) _telemetryData.co2 = co2;
        if (!isnan(tvoc) && tvoc > 0) _telemetryData.tvoc = tvoc;
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
    // PREFLIGHT: Logging completo
    LOG_PREFLIGHT("\n========== ENVIANDO TELEMETRIA ==========\n");
    LOG_PREFLIGHT("Timestamp: %lu ms\n", _telemetryData.timestamp);
    LOG_PREFLIGHT("Bateria: %.2fV (%.0f%%)\n", _telemetryData.batteryVoltage, _telemetryData.batteryPercentage);
    LOG_PREFLIGHT("Temp: %.2f°C | Pressão: %.2f hPa | Alt: %.1f m\n", 
                  _telemetryData.temperature, _telemetryData.pressure, _telemetryData.altitude);
    LOG_PREFLIGHT("Gyro: [%.4f, %.4f, %.4f] rad/s\n", 
                  _telemetryData.gyroX, _telemetryData.gyroY, _telemetryData.gyroZ);
    LOG_PREFLIGHT("Accel: [%.4f, %.4f, %.4f] m/s²\n", 
                  _telemetryData.accelX, _telemetryData.accelY, _telemetryData.accelZ);
    
    if (!isnan(_telemetryData.humidity)) {
        LOG_PREFLIGHT("Umidade: %.1f%%\n", _telemetryData.humidity);
    }
    if (!isnan(_telemetryData.co2)) {
        LOG_PREFLIGHT("CO2: %.0f ppm | TVOC: %.0f ppb\n", _telemetryData.co2, _telemetryData.tvoc);
    }
    if (!isnan(_telemetryData.magX)) {
        LOG_PREFLIGHT("Mag: [%.2f, %.2f, %.2f] µT\n", 
                      _telemetryData.magX, _telemetryData.magY, _telemetryData.magZ);
    }
    LOG_PREFLIGHT("Payload: %s\n", _telemetryData.payload);
    LOG_PREFLIGHT("========================================\n");
    
    // FLIGHT: Log minimalista
    unsigned long missionTimeSec = _telemetryData.missionTime / 1000;
    LOG_FLIGHT("[Flight] T+%lus | Bat:%.1fV | Alt:%.0fm | ", 
               missionTimeSec, _telemetryData.batteryVoltage, _telemetryData.altitude);
    
    if (!_comm.isConnected()) {
        LOG_ERROR("WiFi desconectado. Tentando reconectar...\n");
        _comm.connectWiFi();
    }
    
    if (_comm.sendTelemetry(_telemetryData)) {
        LOG_FLIGHT("OK\n");
        LOG_PREFLIGHT("Telemetria enviada com sucesso!\n\n");
    } else {
        LOG_ERROR("Falha no envio HTTP\n");
        _health.reportError(STATUS_WIFI_ERROR, "Telemetry send failed");
    }
}

void TelemetryManager::_saveToStorage() {
    if (!_storage.isAvailable()) return;
    
    LOG_PREFLIGHT("[TelemetryManager] Salvando dados no SD...\n");
    
    if (_storage.saveTelemetry(_telemetryData)) {
        LOG_PREFLIGHT("[TelemetryManager] Telemetria salva no SD\n");
    }
    
    if (_storage.saveMissionData(_missionData)) {
        LOG_PREFLIGHT("[TelemetryManager] Dados da missão salvos no SD\n");
    }
}

void TelemetryManager::_checkOperationalConditions() {
    if (_power.isCritical()) {
        _health.reportError(STATUS_BATTERY_CRIT, "Critical battery level");
        _power.enablePowerSave();
        LOG_ERROR("Bateria crítica! Power save ativado.\n");
    } else if (_power.isLow()) {
        _health.reportError(STATUS_BATTERY_LOW, "Low battery level");
        LOG_PREFLIGHT("[TelemetryManager] AVISO: Bateria baixa\n");
    }
    
    if (!_sensors.isMPU6050Online() && !_sensors.isMPU9250Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "IMU offline");
        LOG_ERROR("IMU offline! Tentando resetar...\n");
        _sensors.resetAll();
    }
    
    if (!_sensors.isBMP280Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline");
        LOG_ERROR("BMP280 offline! Tentando resetar...\n");
        _sensors.resetAll();
    }
    
    if (_mode == MODE_FLIGHT && !_health.isMissionActive()) {
        stopMission();
    }
    
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < 10000) {
        LOG_ERROR("CRÍTICO: Heap baixo: %lu bytes\n", currentHeap);
        _health.reportError(STATUS_WATCHDOG, "Critical low memory");
    }
}

void TelemetryManager::_displayStatus() {
    _display.clear();
    
    String line1 = "PRE " + String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 0, line1);
    
    String line2;
    if (_sensors.isSHT20Online() && !isnan(_sensors.getHumidity())) {
        line2 = String(_sensors.getTemperature(), 1) + "C " + 
                String(_sensors.getHumidity(), 0) + "%RH";
    } else {
        line2 = String(_sensors.getTemperature(), 1) + "C " + 
                String(_sensors.getPressure(), 0) + "hPa";
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
    String line1 = String(minutes) + ":" + String(seconds) + " " + 
                   String(_power.getPercentage(), 0) + "%";
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
    if (currentHeap < _minHeapSeen) {
        _minHeapSeen = currentHeap;
    }
    LOG_ALWAYS("[TelemetryManager] %s - Heap: %lu bytes\n", component.c_str(), currentHeap);
}

void TelemetryManager::_monitorHeap() {
    uint32_t currentHeap = ESP.getFreeHeap();
    
    if (currentHeap < _minHeapSeen) {
        _minHeapSeen = currentHeap;
    }
    
    // PREFLIGHT: Diagnóstico completo
    LOG_PREFLIGHT("\n========== HEAP MONITOR ==========\n");
    LOG_PREFLIGHT("Heap atual: %lu KB\n", currentHeap / 1024);
    LOG_PREFLIGHT("Heap mínimo: %lu KB\n", _minHeapSeen / 1024);
    LOG_PREFLIGHT("Fragmentação: %.1f%%\n", 
                  (1.0 - (float)ESP.getMaxAllocHeap() / currentHeap) * 100);
    
    uint8_t sensorsActive = 0;
    if (_sensors.isMPU6050Online()) sensorsActive++;
    if (_sensors.isMPU9250Online()) sensorsActive++;
    if (_sensors.isBMP280Online()) sensorsActive++;
    if (_sensors.isSHT20Online()) sensorsActive++;
    if (_sensors.isCCS811Online()) sensorsActive++;
    LOG_PREFLIGHT("Sensores ativos: %d\n", sensorsActive);
    LOG_PREFLIGHT("==================================\n\n");
    
    // FLIGHT: Apenas alerta se crítico
    if (g_isFlightMode && currentHeap < 15000) {
        LOG_ERROR("Heap crítico: %lu KB\n", currentHeap / 1024);
    }
}