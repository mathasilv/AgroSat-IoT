/**
 * @file TelemetryManager.cpp
 * @brief Implementação do gerenciador central de telemetria com sensores expandidos
 * @version 2.1.0
 */

#include "TelemetryManager.h"

TelemetryManager::TelemetryManager() :
    _display(OLED_ADDRESS, OLED_SDA, OLED_SCL),
    _mode(MODE_INIT),
    _lastTelemetrySend(0),
    _lastStorageSave(0),
    _lastDisplayUpdate(0),
    _lastHeapCheck(0),
    _minHeapSeen(UINT32_MAX)
{
    // Inicializar estruturas
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    memset(&_missionData, 0, sizeof(MissionData));
    
    // Inicializar campos opcionais como NaN
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
}

bool TelemetryManager::begin() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT CubeSat - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis - Sensores Expandidos");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    // Log inicial de heap
    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);
    
    // Inicializar display OLED
    DEBUG_PRINTLN("[TelemetryManager] Inicializando display...");
    bool displayOk = false;
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        if (_display.init()) {
            _display.flipScreenVertically();
            _display.setFont(ArialMT_Plain_10);
            _display.clear();
            _display.drawString(0, 0, "AgroSat-IoT");
            _display.drawString(0, 15, "Auto-detecting...");
            _display.display();
            displayOk = true;
            break;
        }
        delay(100);
    }
    
    if (!displayOk) {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Display não inicializou");
    }
    
    delay(1000);
    
    // Inicializar subsistemas
    bool success = true;
    uint8_t subsystemsOk = 0;
    
    // 1. System Health (watchdog)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] System Health OK");
    } else {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: System Health falhou!");
        success = false;
    }
    _logHeapUsage("System Health");
    
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
    
    // 3. Sensor Manager (com auto-detecção)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        // Log dos sensores detectados
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
    // Monitorar heap periodicamente
    uint32_t currentTime = millis();
    if (currentTime - _lastHeapCheck >= 60000) {
        _lastHeapCheck = currentTime;
        _monitorHeap();
    }
    
    // Atualizar subsistemas
    _health.update();
    _power.update();
    _sensors.update();
    _comm.update();
    _payload.update();
    
    // Coletar dados de telemetria (EXPANDIDO)
    _collectTelemetryData();
    
    // Verificar condições operacionais
    _checkOperationalConditions();
    
    // Enviar telemetria via WiFi (a cada 4 minutos)
    if (currentTime - _lastTelemetrySend >= TELEMETRY_SEND_INTERVAL) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    // Salvar no SD Card (a cada 1 minuto)
    if (currentTime - _lastStorageSave >= STORAGE_SAVE_INTERVAL) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // Atualizar display (a cada 2 segundos)
    if (currentTime - _lastDisplayUpdate >= 2000) {
        _lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    
    // Delay para preservar CPU
    delay(10);
}

void TelemetryManager::startMission() {
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO!");
    
    _mode = MODE_FLIGHT;
    _health.startMission();
    
    // Reset contadores
    _lastTelemetrySend = millis();
    _lastStorageSave = millis();
    
    // Log de sensores ativos na missão
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========== SENSORES ATIVOS NA MISSÃO ==========");
    if (_sensors.isMPU6050Online()) DEBUG_PRINTLN("✓ MPU6050: Giroscópio + Acelerômetro");
    if (_sensors.isMPU9250Online()) DEBUG_PRINTLN("✓ MPU9250: IMU 9-DOF com magnetômetro");
    if (_sensors.isBMP280Online()) DEBUG_PRINTLN("✓ BMP280: Pressão + Temperatura");
    if (_sensors.isSHT20Online()) DEBUG_PRINTLN("✓ SHT20: Temperatura + Umidade");
    if (_sensors.isCCS811Online()) DEBUG_PRINTLN("✓ CCS811: CO2 + TVOC");
    DEBUG_PRINTLN("==============================================");
    DEBUG_PRINTLN("");
    
    // Atualizar display
    _display.clear();
    _display.drawString(0, 0, "MISSAO INICIADA");
    _display.drawString(0, 20, "Modo: FLIGHT");
    _display.display();
}

void TelemetryManager::stopMission() {
    DEBUG_PRINTLN("[TelemetryManager] MISSÃO FINALIZADA!");
    
    _mode = MODE_POSTFLIGHT;
    
    // Última telemetria e salvamento
    _sendTelemetry();
    _saveToStorage();
    
    // Log de heap final
    uint32_t finalHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[TelemetryManager] Heap final: %lu, mínimo: %lu\n", finalHeap, _minHeapSeen);
    
    // Atualizar display
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

// ============================================================================
// MÉTODOS PRIVADOS - COLETA DE DADOS EXPANDIDA
// ============================================================================

void TelemetryManager::_collectTelemetryData() {
    // Timestamp
    _telemetryData.timestamp = millis();
    _telemetryData.missionTime = _health.getMissionTime();
    
    // Bateria
    _telemetryData.batteryVoltage = _power.getVoltage();
    _telemetryData.batteryPercentage = _power.getPercentage();
    
    // Sensores ambientais (OBRIGATÓRIOS OBSAT)
    _telemetryData.temperature = _sensors.getTemperature();
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    
    // IMU (OBRIGATÓRIO OBSAT)
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
    // SENSORES EXPANDIDOS (OPCIONAIS)
    // Apenas preenche se sensores estiverem online
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    // SHT20 - Umidade relativa
    if (_sensors.isSHT20Online()) {
        float hum = _sensors.getHumidity();
        if (!isnan(hum)) {
            _telemetryData.humidity = hum;
        }
    }
    
    // CCS811 - CO2 e TVOC
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
    
    // MPU9250 - Magnetômetro (9º DOF)
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
    
    // Status do sistema
    _telemetryData.systemStatus = _health.getSystemStatus();
    _telemetryData.errorCount = _health.getErrorCount();
    
    // Payload (missão LoRa)
    String payloadStr = _payload.generatePayload();
    strncpy(_telemetryData.payload, payloadStr.c_str(), PAYLOAD_MAX_SIZE - 1);
    _telemetryData.payload[PAYLOAD_MAX_SIZE - 1] = '\0';
    
    // Dados da missão
    _missionData = _payload.getMissionData();
}

void TelemetryManager::_sendTelemetry() {
    DEBUG_PRINTLN("[TelemetryManager] Enviando telemetria...");
    
    // Log dos dados sendo enviados
    DEBUG_PRINTF("  Temp: %.2f°C, Pressão: %.2f hPa, Alt: %.1f m\n", 
                _telemetryData.temperature, _telemetryData.pressure, _telemetryData.altitude);
    DEBUG_PRINTF("  Gyro: [%.4f, %.4f, %.4f] rad/s\n", 
                _telemetryData.gyroX, _telemetryData.gyroY, _telemetryData.gyroZ);
    DEBUG_PRINTF("  Accel: [%.4f, %.4f, %.4f] m/s²\n", 
                _telemetryData.accelX, _telemetryData.accelY, _telemetryData.accelZ);
    
    // Log campos opcionais
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
    
    // Verificar conexão WiFi
    if (!_comm.isConnected()) {
        DEBUG_PRINTLN("[TelemetryManager] Tentando reconectar WiFi...");
        _comm.connectWiFi();
    }
    
    // Enviar telemetria
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
    
    // Salvar telemetria
    if (_storage.saveTelemetry(_telemetryData)) {
        DEBUG_PRINTLN("[TelemetryManager] Telemetria salva no SD");
    }
    
    // Salvar dados da missão
    if (_storage.saveMissionData(_missionData)) {
        DEBUG_PRINTLN("[TelemetryManager] Dados da missão salvos no SD");
    }
}

void TelemetryManager::_checkOperationalConditions() {
    // Verificar bateria crítica
    if (_power.isCritical()) {
        _health.reportError(STATUS_BATTERY_CRIT, "Critical battery level");
        _power.enablePowerSave();
    } else if (_power.isLow()) {
        _health.reportError(STATUS_BATTERY_LOW, "Low battery level");
    }
    
    // Verificar sensores obrigatórios offline
    if (!_sensors.isMPU6050Online() && !_sensors.isMPU9250Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "IMU offline");
        _sensors.resetAll();
    }
    
    if (!_sensors.isBMP280Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline");
        _sensors.resetAll();
    }
    
    // Verificar duração da missão
    if (_mode == MODE_FLIGHT && !_health.isMissionActive()) {
        stopMission();
    }
    
    // Verificar heap crítico
    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < 10000) {
        DEBUG_PRINTF("[TelemetryManager] CRÍTICO: Heap baixo: %lu bytes\n", currentHeap);
        _health.reportError(STATUS_WATCHDOG, "Critical low memory");
    }
}

void TelemetryManager::_displayStatus() {
    _display.clear();
    
    // Linha 1: Modo e bateria
    String line1 = "PRE " + String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 0, line1);
    
    // Linha 2: Temperatura e pressão (ou umidade se SHT20 disponível)
    String line2;
    if (_sensors.isSHT20Online() && !isnan(_sensors.getHumidity())) {
        line2 = String(_sensors.getTemperature(), 1) + "C " + 
                String(_sensors.getHumidity(), 0) + "%RH";
    } else {
        line2 = String(_sensors.getTemperature(), 1) + "C " + 
                String(_sensors.getPressure(), 0) + "hPa";
    }
    _display.drawString(0, 15, line2);
    
    // Linha 3: Altitude ou CO2 (se disponível)
    String line3;
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        line3 = "CO2: " + String(_sensors.getCO2(), 0) + "ppm";
    } else {
        line3 = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
    }
    _display.drawString(0, 30, line3);
    
    // Linha 4: Status dos subsistemas + heap
    String line4 = "";
    line4 += _comm.isConnected() ? "W+" : "W-";
    line4 += " ";
    line4 += _payload.isOnline() ? "L+" : "L-";
    line4 += " ";
    line4 += _storage.isAvailable() ? "S+" : "S-";
    line4 += " ";
    
    // Indicadores de sensores extras
    if (_sensors.isSHT20Online()) line4 += "H+";
    if (_sensors.isCCS811Online()) line4 += "C+";
    if (_sensors.isMPU9250Online()) line4 += "9+";
    
    line4 += " " + String(ESP.getFreeHeap() / 1024) + "K";
    _display.drawString(0, 45, line4);
    
    _display.display();
}

void TelemetryManager::_displayTelemetry() {
    _display.clear();
    
    // Linha 1: Tempo de missão e bateria
    unsigned long missionTimeSec = _telemetryData.missionTime / 1000;
    unsigned long minutes = missionTimeSec / 60;
    unsigned long seconds = missionTimeSec % 60;
    String line1 = String(minutes) + ":" + String(seconds) + " " + 
                   String(_power.getPercentage(), 0) + "%";
    _display.drawString(0, 0, line1);
    
    // Linha 2: Altitude
    String line2 = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
    _display.drawString(0, 15, line2);
    
    // Linha 3: Temperatura ou CO2
    String line3;
    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        line3 = "CO2: " + String(_sensors.getCO2(), 0) + "ppm";
    } else {
        line3 = "Temp: " + String(_sensors.getTemperature(), 1) + "C";
    }
    _display.drawString(0, 30, line3);
    
    // Linha 4: LoRa, sensores extras e heap
    String line4 = "LoRa:" + String(_missionData.packetsReceived);
    
    // Adicionar indicadores de sensores extras
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
    
    // Mostrar heap e sensores
    String heapInfo = "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB";
    _display.drawString(0, 30, heapInfo);
    
    // Status dos sensores críticos
    String sensorStatus = "";
    sensorStatus += _sensors.isBMP280Online() ? "B+" : "B-";
    sensorStatus += _sensors.isMPU6050Online() || _sensors.isMPU9250Online() ? " I+" : " I-";
    _display.drawString(0, 45, sensorStatus);
    
    _display.display();
}

// MÉTODOS DE MONITORAMENTO DE HEAP
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
    
    // Log resumo de sensores ativos
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
