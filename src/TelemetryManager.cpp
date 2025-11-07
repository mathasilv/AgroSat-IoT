/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO CORRIGIDA - I2C centralizado, sem race conditions
 * @version 2.2.0
 */

#include "TelemetryManager.h"

TelemetryManager::TelemetryManager() :
    _display(OLED_ADDRESS),  // SEM SDA/SCL - usa Wire padrão já inicializado
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
        Wire.setClock(100000);  // 100kHz para máxima estabilidade
        Wire.setTimeout(500);   // Timeout de 500ms
        delay(300);             // Aguardar estabilização COMPLETA
        i2cInitialized = true;
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado em 100kHz");
    }
    
    return true;
}

bool TelemetryManager::begin() {
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
    
    // === PASSO CRÍTICO: Inicializar I2C ANTES de tudo ===
    _initI2CBus();
    
    // === Inicializar display DEPOIS do I2C ===
    DEBUG_PRINTLN("[TelemetryManager] Inicializando display...");
    bool displayOk = false;
    
    delay(100);  // Aguardar I2C estabilizar
    
    if (_display.init()) {
        _display.flipScreenVertically();
        _display.setFont(ArialMT_Plain_10);
        _display.clear();
        _display.drawString(0, 0, "AgroSat-IoT");
        _display.drawString(0, 15, "Initializing...");
        _display.display();
        displayOk = true;
        DEBUG_PRINTLN("[TelemetryManager] Display OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Display falhou");
    }
    
    delay(500);  // Aguardar display processar
    
    // Inicializar subsistemas COM delays entre cada um
    bool success = true;
    uint8_t subsystemsOk = 0;
    
    // 1. System Health
    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] System Health OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: System Health falhou!");
        success = false;
    }
    _logHeapUsage("System Health");
    delay(100);  // Delay entre subsistemas
    
    // 2. Power Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Power Manager falhou!");
        _health.reportError(STATUS_BATTERY_LOW, "Power Manager init failed");
        success = false;
    }
    _logHeapUsage("Power Manager");
    delay(100);
    
    // 3. Sensor Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("========== SENSORES DETECTADOS ==========");
        if (_sensors.isMPU6050Online()) DEBUG_PRINTLN("✓ MPU6050 (IMU 6-DOF)");
        if (_sensors.isMPU9250Online()) DEBUG_PRINTLN("✓ MPU9250 (IMU 9-DOF)");
        if (_sensors.isBMP280Online()) DEBUG_PRINTLN("✓ BMP280 (Pressão/Temp)");
        if (_sensors.isSHT20Online()) DEBUG_PRINTLN("✓ SHT20 (Temp/Umidade)");
        if (_sensors.isCCS811Online()) DEBUG_PRINTLN("✓ CCS811 (CO2/TVOC)");
        DEBUG_PRINTLN("========================================");
        DEBUG_PRINTLN("");
        
    } else {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Sensor Manager falhou!");
        _health.reportError(STATUS_SENSOR_ERROR, "Sensor Manager init failed");
        success = false;
    }
    _logHeapUsage("Sensor Manager");
    delay(200);  // Delay maior após sensores
    
    // 4. Storage Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Storage Manager OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Storage Manager falhou - sem SD");
        _health.reportError(STATUS_SD_ERROR, "Storage Manager init failed");
    }
    _logHeapUsage("Storage Manager");
    delay(100);
    
    // 5. Payload Manager (LoRa)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Payload Manager...");
    if (_payload.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Payload Manager OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Payload Manager falhou - sem LoRa");
        _health.reportError(STATUS_LORA_ERROR, "Payload Manager init failed");
    }
    _logHeapUsage("Payload Manager");
    delay(100);
    
    // 6. Communication Manager (WiFi)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Communication Manager falhou!");
        _health.reportError(STATUS_WIFI_ERROR, "Communication Manager init failed");
    }
    _logHeapUsage("Communication Manager");
    
    // Atualizar display com status
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
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("  Status: %s (%d/6 subsistemas)\n", success ? "OK" : "ERRO", subsystemsOk);
    DEBUG_PRINTF("  Heap final: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    return success;
}

void TelemetryManager::loop() {
    uint32_t currentTime = millis();
    
    // Monitorar heap
    if (currentTime - _lastHeapCheck >= 60000) {
        _lastHeapCheck = currentTime;
        _monitorHeap();
    }
    
    // Atualizar subsistemas COM delays (evitar race condition I2C)
    _health.update();
    delay(5);  // Pequeno delay entre cada update
    
    _power.update();
    delay(5);
    
    _sensors.update();
    delay(10);  // Delay maior após sensors (usa I2C pesado)
    
    _comm.update();
    delay(5);
    
    _payload.update();
    delay(5);
    
    // Coletar dados
    _collectTelemetryData();
    
    // Verificar condições
    _checkOperationalConditions();
    
    // Enviar telemetria
    if (currentTime - _lastTelemetrySend >= TELEMETRY_SEND_INTERVAL) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    // Salvar no SD
    if (currentTime - _lastStorageSave >= STORAGE_SAVE_INTERVAL) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // Atualizar display
    if (currentTime - _lastDisplayUpdate >= 2000) {
        _lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    
    // Delay total do loop: ~50ms
    delay(20);
}

// ... (resto dos métodos IGUAIS ao código anterior)
// startMission(), stopMission(), getMode(), updateDisplay(), etc.
// _collectTelemetryData(), _sendTelemetry(), _saveToStorage()
// _checkOperationalConditions(), _displayStatus(), _displayTelemetry()
// _displayError(), _logHeapUsage(), _monitorHeap()

void TelemetryManager::startMission() {
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO!");
    
    _mode = MODE_FLIGHT;
    _health.startMission();
    
    _lastTelemetrySend = millis();
    _lastStorageSave = millis();
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========== SENSORES ATIVOS NA MISSÃO ==========");
    if (_sensors.isMPU6050Online()) DEBUG_PRINTLN("✓ MPU6050: Giroscópio + Acelerômetro");
    if (_sensors.isMPU9250Online()) DEBUG_PRINTLN("✓ MPU9250: IMU 9-DOF com magnetômetro");
    if (_sensors.isBMP280Online()) DEBUG_PRINTLN("✓ BMP280: Pressão + Temperatura");
    if (_sensors.isSHT20Online()) DEBUG_PRINTLN("✓ SHT20: Temperatura + Umidade");
    if (_sensors.isCCS811Online()) DEBUG_PRINTLN("✓ CCS811: CO2 + TVOC");
    DEBUG_PRINTLN("==============================================");
    DEBUG_PRINTLN("");
    
    _display.clear();
    _display.drawString(0, 0, "MISSAO INICIADA");
    _display.drawString(0, 20, "Modo: FLIGHT");
    _display.display();
}

void TelemetryManager::stopMission() {
    DEBUG_PRINTLN("[TelemetryManager] MISSÃO FINALIZADA!");
    
    _mode = MODE_POSTFLIGHT;
    
    _sendTelemetry();
    _saveToStorage();
    
    uint32_t finalHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Heap final: %lu, mínimo: %lu\n", finalHeap, _minHeapSeen);
    
    _display.clear();
    _display.drawString(0, 0, "MISSAO COMPLETA");
    String heapInfo = "MIN: " + String(_minHeapSeen / 1024) + "KB";
    _display.drawString(0, 20, heapInfo);
    _display.display();
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
    
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    if (_sensors.isSHT20Online()) {
        float hum = _sensors.getHumidity();
        if (!isnan(hum)) {
            _telemetryData.humidity = hum;
        }
    }
    
    if (_sensors.isCCS811Online()) {
        float co2 = _sensors.getCO2();
        float tvoc = _sensors.getTVOC();
        if (!isnan(co2) && co2 > 0) {
            _telemetryData.co2 = co2;
        }
        if (!isnan(tvoc) && tvoc > 0) {
            _telemetryData.tvoc = tvoc;
        }
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
    
    if (!isnan(_telemetryData.humidity)) {
        DEBUG_PRINTF("  Umidade: %.1f%%\n", _telemetryData.humidity);
    }
    if (!isnan(_telemetryData.co2)) {
        DEBUG_PRINTF("  CO2: %.0f ppm, TVOC: %.0f ppb\n", _telemetryData.co2, _telemetryData.tvoc);
    }
    if (!isnan(_telemetryData.magX)) {
        DEBUG_PRINTF("  Mag: [%.2f, %.2f, %.2f] µT\n", 
                    _telemetryData.magX, _telemetryData.magY, _telemetryData.magZ);
    }
    
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
    DEBUG_PRINTF("[TelemetryManager] %s - Heap: %lu bytes\n", component.c_str(), currentHeap);
}

void TelemetryManager::_monitorHeap() {
    uint32_t currentHeap = ESP.getFreeHeap();
    
    if (currentHeap < _minHeapSeen) {
        _minHeapSeen = currentHeap;
    }
    
    DEBUG_PRINTF("[TelemetryManager] Heap: %lu KB, Min: %lu KB\n", 
                currentHeap / 1024, _minHeapSeen / 1024);
    
    uint8_t sensorsActive = 0;
    if (_sensors.isMPU6050Online()) sensorsActive++;
    if (_sensors.isMPU9250Online()) sensorsActive++;
    if (_sensors.isBMP280Online()) sensorsActive++;
    if (_sensors.isSHT20Online()) sensorsActive++;
    if (_sensors.isCCS811Online()) sensorsActive++;
    
    DEBUG_PRINTF("[TelemetryManager] Sensores ativos: %d\n", sensorsActive);
    
    if (currentHeap < 15000) {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Heap baixo!");
    }
}
