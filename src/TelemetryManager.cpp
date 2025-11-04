/**
 * @file TelemetryManager.cpp
 * @brief Implementação do gerenciador central de telemetria - Versão estável
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
}

bool TelemetryManager::begin() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT CubeSat - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    // Log inicial de heap
    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);
    
    // Inicializar display OLED
    DEBUG_PRINTLN("[TelemetryManager] Inicializando display...");
    _display.init();
    _display.flipScreenVertically();
    _display.setFont(ArialMT_Plain_10);
    _display.clear();
    _display.drawString(0, 0, "AgroSat-IoT");
    _display.drawString(0, 15, "Iniciando...");
    _display.display();
    
    delay(1000);
    
    // Inicializar subsistemas
    bool success = true;
    
    // 1. System Health (watchdog)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (!_health.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: System Health falhou!");
        success = false;
    }
    _logHeapUsage("System Health");
    
    // 2. Power Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    if (!_power.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Power Manager falhou!");
        _health.reportError(STATUS_BATTERY_LOW, "Power Manager init failed");
        success = false;
    }
    _logHeapUsage("Power Manager");
    
    // 3. Sensor Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (!_sensors.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Sensor Manager falhou!");
        _health.reportError(STATUS_SENSOR_ERROR, "Sensor Manager init failed");
        success = false;
    }
    _logHeapUsage("Sensor Manager");
    
    // 4. Storage Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
    if (!_storage.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Storage Manager falhou!");
        _health.reportError(STATUS_SD_ERROR, "Storage Manager init failed");
        // Não é crítico - continuar sem SD
    }
    _logHeapUsage("Storage Manager");
    
    // 5. Payload Manager (LoRa)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Payload Manager...");
    if (!_payload.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] ERRO: Payload Manager falhou!");
        _health.reportError(STATUS_LORA_ERROR, "Payload Manager init failed");
        // Não é crítico para telemetria
    }
    _logHeapUsage("Payload Manager");
    
    // 6. Communication Manager (WiFi)
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    if (!_comm.begin()) {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Communication Manager falhou!");
        _health.reportError(STATUS_WIFI_ERROR, "Communication Manager init failed");
        // Tentará reconectar durante operação
    }
    _logHeapUsage("Communication Manager");
    
    // Atualizar display
    _display.clear();
    if (success) {
        _display.drawString(0, 0, "Sistema OK!");
        _display.drawString(0, 15, "Modo: PRE-FLIGHT");
        _mode = MODE_PREFLIGHT;
    } else {
        _display.drawString(0, 0, "ERRO Sistema!");
        _display.drawString(0, 15, "Verificar logs");
        _mode = MODE_ERROR;
    }
    _display.display();
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("  Status: %s\n", success ? "OK" : "ERRO");
    DEBUG_PRINTF("  Heap final: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    return success;
}

void TelemetryManager::loop() {
    // Monitorar heap periodicamente (simplificado)
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
    
    // Coletar dados de telemetria
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
    
    // Pequeno delay para não sobrecarregar CPU
    delay(10);
}

void TelemetryManager::startMission() {
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO!");
    
    _mode = MODE_FLIGHT;
    _health.startMission();
    
    // Reset contadores
    _lastTelemetrySend = millis();
    _lastStorageSave = millis();
    
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
// MÉTODOS PRIVADOS (mantidos originais + novos métodos de heap)
// ============================================================================

void TelemetryManager::_collectTelemetryData() {
    // Timestamp
    _telemetryData.timestamp = millis();
    _telemetryData.missionTime = _health.getMissionTime();
    
    // Bateria
    _telemetryData.batteryVoltage = _power.getVoltage();
    _telemetryData.batteryPercentage = _power.getPercentage();
    
    // Sensores ambientais
    _telemetryData.temperature = _sensors.getTemperature();
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    
    // IMU
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
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
    
    // Verificar sensores offline
    if (!_sensors.isMPU6050Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "MPU6050 offline");
        _sensors.resetMPU6050();
    }
    
    if (!_sensors.isBMP280Online()) {
        _health.reportError(STATUS_SENSOR_ERROR, "BMP280 offline");
        _sensors.resetBMP280();
    }
    
    // Verificar duração da missão
    if (_mode == MODE_FLIGHT && !_health.isMissionActive()) {
        stopMission();
    }
    
    // Verificar heap crítico (NOVO)
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
    
    // Linha 2: Temperatura e pressão
    String line2 = String(_sensors.getTemperature(), 1) + "C " + 
                   String(_sensors.getPressure(), 0) + "hPa";
    _display.drawString(0, 15, line2);
    
    // Linha 3: Altitude
    String line3 = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
    _display.drawString(0, 30, line3);
    
    // Linha 4: Status WiFi, LoRa, SD e heap
    String line4 = "";
    line4 += _comm.isConnected() ? "W+" : "W-";
    line4 += " ";
    line4 += _payload.isOnline() ? "L+" : "L-";
    line4 += " ";
    line4 += _storage.isAvailable() ? "S+" : "S-";
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
    
    // Linha 3: Temperatura
    String line3 = "Temp: " + String(_sensors.getTemperature(), 1) + "C";
    _display.drawString(0, 30, line3);
    
    // Linha 4: Pacotes LoRa e heap
    String line4 = "LoRa: " + String(_missionData.packetsReceived) +
                   " " + String(ESP.getFreeHeap() / 1024) + "K";
    _display.drawString(0, 45, line4);
    
    _display.display();
}

void TelemetryManager::_displayError(const String& error) {
    _display.clear();
    _display.drawString(0, 0, "ERRO:");
    _display.drawString(0, 15, error);
    
    // Mostrar heap em caso de erro
    String heapInfo = "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB";
    _display.drawString(0, 30, heapInfo);
    
    _display.display();
}

// NOVOS MÉTODOS DE MONITORAMENTO
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
    
    if (currentHeap < 15000) {
        DEBUG_PRINTLN("[TelemetryManager] AVISO: Heap baixo!");
    }
}
