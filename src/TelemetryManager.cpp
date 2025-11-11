/**
 * @file TelemetryManager.cpp
 * @brief VERSÃO DUAL MODE COM RTC DS3231 SIMPLIFICADO
 * @version 4.0.0
 * @date 2025-11-11
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
    _missionActive(false),
    _missionStartTime(0),
    _lastDisplayUpdate(0),
    _lastHeapCheck(0),
    _minHeapSeen(UINT32_MAX),
    _useNewDisplay(true)
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
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("[TelemetryManager] Inicializando barramento I2C...");
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000);
        Wire.setTimeOut(200);
        
        delay(300);
        i2cInitialized = true;
        
        DEBUG_PRINTLN("[TelemetryManager] I2C inicializado (100 kHz, timeout 200ms)");
        
        DEBUG_PRINTLN("[TelemetryManager] Dispositivos no barramento:");
        
        uint8_t devicesFound = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                DEBUG_PRINTF("[TelemetryManager]   - 0x%02X\n", addr);
                devicesFound++;
            }
        }
        
        if (devicesFound == 0) {
            DEBUG_PRINTLN("[TelemetryManager] Nenhum dispositivo I2C encontrado!");
        } else {
            DEBUG_PRINTF("[TelemetryManager] Total: %d dispositivo(s) I2C\n", devicesFound);
        }
        
        DEBUG_PRINTLN("[TelemetryManager] ========================================");
        DEBUG_PRINTLN("");
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
        case MODE_SAFE:
            activeModeConfig = &SAFE_CONFIG;
            break;
        default: 
            activeModeConfig = &PREFLIGHT_CONFIG; 
            break;
    }
    
    // Aplica flag global de logs
    currentSerialLogsEnabled = activeModeConfig->serialLogsEnabled;
    
    // ✅ CONTROLE FÍSICO DO DISPLAY
    if (_useNewDisplay) {
        if (activeModeConfig->displayEnabled) {
            _displayMgr.turnOn();
        } else {
            _displayMgr.turnOff();
        }
    } else {
        if (!activeModeConfig->displayEnabled) {
            _display.displayOff();
        } else {
            _display.displayOn();
        }
    }
    
    // ✅ NOVO: Aplicar configuração LoRa/HTTP
    _comm.enableLoRa(activeModeConfig->loraEnabled);
    _comm.enableHTTP(activeModeConfig->httpEnabled);
    
    DEBUG_PRINTF("[TelemetryManager] Modo aplicado: %d | Display: %s | Logs: %s | LoRa: %s | HTTP: %s\n",
                 mode,
                 activeModeConfig->displayEnabled ? "ON" : "OFF",
                 activeModeConfig->serialLogsEnabled ? "ON" : "OFF",
                 activeModeConfig->loraEnabled ? "ON" : "OFF",
                 activeModeConfig->httpEnabled ? "ON" : "OFF");
}


bool TelemetryManager::begin() {
    // ✅ Aplicar config PREFLIGHT uma única vez no início
    _mode = MODE_PREFLIGHT;
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  AgroSat-IoT CubeSat - OBSAT Fase 2");
    DEBUG_PRINTLN("  Equipe: Orbitalis");
    DEBUG_PRINTLN("  Firmware: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");

    uint32_t initialHeap = ESP.getFreeHeap();
    _minHeapSeen = initialHeap;
    DEBUG_PRINTF("[TelemetryManager] Heap inicial: %lu bytes\n", initialHeap);

    // Inicializar I2C PRIMEIRO
    _initI2CBus();

    bool displayOk = false;
    
    // ✅ TENTAR INICIALIZAR DISPLAYMANAGER PRIMEIRO
    if (activeModeConfig->displayEnabled) {
        DEBUG_PRINTLN("[TelemetryManager] Inicializando DisplayManager...");
        
        if (_displayMgr.begin()) {
            _useNewDisplay = true;
            displayOk = true;
            _displayMgr.showBoot();
            delay(2000);
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager OK");
        } else {
            DEBUG_PRINTLN("[TelemetryManager] DisplayManager FAILED, tentando sistema legado...");
            _useNewDisplay = false;
            
            delay(100);
            if (_display.init()) {
                _display.flipScreenVertically();
                _display.setFont(ArialMT_Plain_10);
                _display.clear();
                _display.drawString(0, 0, "AgroSat-IoT");
                _display.drawString(0, 15, "Initializing...");
                _display.display();
                displayOk = true;
                DEBUG_PRINTLN("[TelemetryManager] Display legado OK");
            } else {
                DEBUG_PRINTLN("[TelemetryManager] Display FAILED (não-crítico)");
            }
        }
        delay(500);
    }

    bool success = true;
    uint8_t subsystemsOk = 0;

    // RTC
    DEBUG_PRINTLN("[TelemetryManager] Inicializando RTC Manager...");
    if (_rtc.begin(&Wire)) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] RTC Manager OK");
        DEBUG_PRINTF("[TelemetryManager] RTC: %s\n", _rtc.getDateTime().c_str());
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("RTC", true);
            delay(500);
        }
    } else {
        DEBUG_PRINTLN("[TelemetryManager] RTC Manager FAILED (não-crítico)");
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("RTC", false);
            delay(500);
        }
    }

    // System Health
    DEBUG_PRINTLN("[TelemetryManager] Inicializando System Health...");
    if (_health.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] System Health OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Health", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] System Health FAILED");
    }

    // Power Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Power Manager...");
    if (_power.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Power", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Power Manager FAILED");
    }

    // Sensor Manager
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Sensor Manager...");
    if (_sensors.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("MPU9250", _sensors.isMPU9250Online());
            delay(500);
            _displayMgr.showSensorInit("BMP280", _sensors.isBMP280Online());
            delay(500);
            _displayMgr.showSensorInit("SI7021", _sensors.isSI7021Online());
            delay(500);
            _displayMgr.showSensorInit("CCS811", _sensors.isCCS811Online());
            delay(1000);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Sensor Manager FAILED");
    }

    // Storage
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Storage Manager...");
    if (_storage.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Storage Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("Storage", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Storage Manager FAILED");
    }

    // Payload
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Payload Manager...");
    if (_payload.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Payload Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("LoRa", true);
            delay(500);
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Payload Manager FAILED");
    }

    // Communication
    DEBUG_PRINTLN("[TelemetryManager] Inicializando Communication Manager...");
    if (_comm.begin()) {
        subsystemsOk++;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager OK");
        
        if (_useNewDisplay && displayOk) {
            _displayMgr.showSensorInit("WiFi", _comm.isConnected());
            delay(500);
        }
        
        // Sincronizar RTC com NTP
        if (_rtc.isInitialized() && _comm.isConnected()) {
            DEBUG_PRINTLN("[TelemetryManager] WiFi disponível, sincronizando RTC com NTP...");
            
            if (_rtc.syncWithNTP()) {
                DEBUG_PRINTLN("[TelemetryManager] RTC sincronizado com NTP");
            } else {
                DEBUG_PRINTLN("[TelemetryManager] Falha na sincronização NTP (não-crítico)");
            }
        }
    } else {
        success = false;
        DEBUG_PRINTLN("[TelemetryManager] Communication Manager FAILED");
    }

    // ✅ MOSTRAR TELA FINAL
    if (_useNewDisplay && displayOk) {
        _displayMgr.showReady();
    } else if (displayOk) {
        _display.clear();
        _display.drawString(0, 0, success ? "Sistema OK!" : "ERRO Sistema!");
        _display.drawString(0, 15, "Modo: PRE-FLIGHT");
        String subsysInfo = String(subsystemsOk) + "/7 subsistemas";
        _display.drawString(0, 30, subsysInfo);
        
        if (_rtc.isInitialized()) {
            String dt = _rtc.getDateTime();
            String timeOnly = dt.substring(11, 19);
            _display.drawString(0, 45, "RTC: " + timeOnly);
        } else {
            _display.drawString(0, 45, "RTC: OFF");
        }
        
        _display.display();
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("Sistema inicializado: %s\n", success ? "OK" : "COM ERROS");
    DEBUG_PRINTF("Subsistemas online: %d/7\n", subsystemsOk);
    DEBUG_PRINTF("Heap disponível: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    return success;
}

void TelemetryManager::loop() {
    // ✅ NÃO chamar applyModeConfig() aqui!
    uint32_t currentTime = millis();
    
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
    
    if (currentTime - _lastTelemetrySend >= activeModeConfig->telemetrySendInterval) {
        _lastTelemetrySend = currentTime;
        _sendTelemetry();
    }
    
    if (currentTime - _lastStorageSave >= activeModeConfig->storageSaveInterval) {
        _lastStorageSave = currentTime;
        _saveToStorage();
    }
    
    // ✅ SÓ ATUALIZA DISPLAY SE ESTIVER HABILITADO
    if (activeModeConfig->displayEnabled) {
        if (_useNewDisplay) {
            _displayMgr.updateTelemetry(_telemetryData);
        } else {
            if (currentTime - _lastDisplayUpdate >= 2000) {
                _lastDisplayUpdate = currentTime;
                updateDisplay();
            }
        }
    }
    
    delay(20);
}

void TelemetryManager::startMission() {
    if (_mode == MODE_FLIGHT || _mode == MODE_POSTFLIGHT) {
        DEBUG_PRINTLN("[TelemetryManager] Missão já em andamento!");
        return;
    }
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] INICIANDO MISSÃO");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    // ✅ Log de timestamp ANTES de desligar logs
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Início: %s\n", _rtc.getDateTime().c_str());
    }
    
    // ✅ Mensagem no display ANTES de desligar
    if (_useNewDisplay && _displayMgr.isOn()) {
        _displayMgr.displayMessage("MISSAO", "INICIADA");
        delay(2000);  // Tempo para usuário ver a mensagem
    }
    
    // ✅ AGORA sim, mudar para FLIGHT e aplicar config
    _mode = MODE_FLIGHT;
    _missionActive = true;
    _missionStartTime = millis();
    
    applyModeConfig(MODE_FLIGHT);  // ← Aqui desliga display/logs
    
    // ✅ Última mensagem antes do silêncio (se logs ainda estiverem ativos)
    DEBUG_PRINTLN("[TelemetryManager] Modo FLIGHT ativado");
    DEBUG_PRINTLN("[TelemetryManager] Display: OFF | Logs: OFF");
    DEBUG_PRINTLN("[TelemetryManager] Missão iniciada com sucesso!");
}

void TelemetryManager::stopMission() {
    if (!_missionActive) {
        DEBUG_PRINTLN("[TelemetryManager] Nenhuma missão ativa!");
        return;
    }
    
    // ✅ REATIVAR LOGS/DISPLAY ANTES DE MOSTRAR MENSAGENS
    applyModeConfig(MODE_PREFLIGHT);
    
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    DEBUG_PRINTLN("[TelemetryManager] ENCERRANDO MISSÃO");
    DEBUG_PRINTLN("[TelemetryManager] ==========================================");
    
    uint32_t missionDuration = millis() - _missionStartTime;
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Fim: %s\n", _rtc.getDateTime().c_str());
    }
    
    DEBUG_PRINTF("[TelemetryManager] Duração: %lu segundos\n", missionDuration / 1000);
    
    _mode = MODE_POSTFLIGHT;
    _missionActive = false;
    
    if (_useNewDisplay) {
        char durationStr[32];
        snprintf(durationStr, sizeof(durationStr), "%lus", missionDuration / 1000);
        _displayMgr.displayMessage("MISSAO COMPLETA", durationStr);
    }
    
    DEBUG_PRINTLN("[TelemetryManager] Missão encerrada com sucesso!");
}

OperationMode TelemetryManager::getMode() {
    return _mode;
}
void TelemetryManager::updateDisplay() {
    // Este método continua existindo como fallback
    // Só será usado se DisplayManager falhar
    
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
    // ========================================
    // TIMESTAMPS
    // ========================================
    if (_rtc.isInitialized()) {
        _telemetryData.timestamp = _rtc.getUnixTime();
    } else {
        _telemetryData.timestamp = millis();
    }
    
    _telemetryData.missionTime = _health.getMissionTime();
    
    // ========================================
    // DADOS OBRIGATÓRIOS (SEMPRE PRESENTES)
    // ========================================
    _telemetryData.batteryVoltage = _power.getVoltage();
    _telemetryData.batteryPercentage = _power.getPercentage();
    
    // Temperatura primária (BMP280) - OBRIGATÓRIA OBSAT
    _telemetryData.temperature = _sensors.getTemperature();
    
    // Pressão e altitude (BMP280)
    _telemetryData.pressure = _sensors.getPressure();
    _telemetryData.altitude = _sensors.getAltitude();
    
    // IMU (MPU9250) - giroscópio
    _telemetryData.gyroX = _sensors.getGyroX();
    _telemetryData.gyroY = _sensors.getGyroY();
    _telemetryData.gyroZ = _sensors.getGyroZ();
    
    // IMU (MPU9250) - acelerômetro
    _telemetryData.accelX = _sensors.getAccelX();
    _telemetryData.accelY = _sensors.getAccelY();
    _telemetryData.accelZ = _sensors.getAccelZ();
    
    // ========================================
    // DADOS OPCIONAIS (SENSORES EXTRAS)
    // ========================================
    // Inicializar como NAN (não disponível)
    _telemetryData.temperatureSI = NAN;
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN;
    _telemetryData.magY = NAN;
    _telemetryData.magZ = NAN;
    
    // SI7021: TEMPERATURA + UMIDADE
    if (_sensors.isSI7021Online()) {
        float tempSI = _sensors.getTemperatureSI7021();
        if (!isnan(tempSI) && tempSI >= TEMP_MIN_VALID && tempSI <= TEMP_MAX_VALID) {
            _telemetryData.temperatureSI = tempSI;
        }
        
        float hum = _sensors.getHumidity();
        if (!isnan(hum) && hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
            _telemetryData.humidity = hum;
        }
    }
    
    // CCS811: CO2 + TVOC
    if (_sensors.isCCS811Online()) {
        float co2 = _sensors.getCO2();
        float tvoc = _sensors.getTVOC();
        
        if (!isnan(co2) && co2 >= CO2_MIN_VALID && co2 <= CO2_MAX_VALID) {
            _telemetryData.co2 = co2;
        }
        
        if (!isnan(tvoc) && tvoc >= TVOC_MIN_VALID && tvoc <= TVOC_MAX_VALID) {
            _telemetryData.tvoc = tvoc;
        }
    }
    
    // MPU9250: MAGNETÔMETRO
    if (_sensors.isMPU9250Online()) {
        float magX = _sensors.getMagX();
        float magY = _sensors.getMagY();
        float magZ = _sensors.getMagZ();
        
        if (!isnan(magX) && !isnan(magY) && !isnan(magZ)) {
            if (magX >= MAG_MIN_VALID && magX <= MAG_MAX_VALID &&
                magY >= MAG_MIN_VALID && magY <= MAG_MAX_VALID &&
                magZ >= MAG_MIN_VALID && magZ <= MAG_MAX_VALID) {
                _telemetryData.magX = magX;
                _telemetryData.magY = magY;
                _telemetryData.magZ = magZ;
            }
        }
    }
    
    // ========================================
    // STATUS DO SISTEMA
    // ========================================
    _telemetryData.systemStatus = _health.getSystemStatus();
    _telemetryData.errorCount = _health.getErrorCount();
    
    // ========================================
    // PAYLOAD DA MISSÃO
    // ========================================
    String payloadStr = _payload.generatePayload();
    strncpy(_telemetryData.payload, payloadStr.c_str(), PAYLOAD_MAX_SIZE - 1);
    _telemetryData.payload[PAYLOAD_MAX_SIZE - 1] = '\0';
    
    _missionData = _payload.getMissionData();
    
    // ========================================
    // DEBUG (apenas em modo PRE-FLIGHT)
    // ========================================
    if (currentSerialLogsEnabled) {
    }
}



void TelemetryManager::_sendTelemetry() {
    // ========================================
    // DEBUG: MOSTRAR DADOS COLETADOS
    // ========================================
    if (activeModeConfig->serialLogsEnabled) {
        DEBUG_PRINTF("[TelemetryManager] Enviando telemetria [%s]...\n", 
                     _rtc.getDateTime().c_str());
        
        // ✅ TEMPERATURAS (BMP280 + SI7021)
        DEBUG_PRINTF("  Temp BMP280: %.2f°C", _telemetryData.temperature);
        
        if (!isnan(_telemetryData.temperatureSI)) {
            DEBUG_PRINTF(" | Temp SI7021: %.2f°C", _telemetryData.temperatureSI);
            float delta = _telemetryData.temperature - _telemetryData.temperatureSI;
            DEBUG_PRINTF(" | Delta: %.2f°C\n", delta);
        } else {
            DEBUG_PRINTLN();
        }
        
        // Pressão e altitude
        DEBUG_PRINTF("  Pressão: %.2f hPa, Alt: %.1f m\n", 
                     _telemetryData.pressure, 
                     _telemetryData.altitude);
        
        // IMU - Giroscópio
        DEBUG_PRINTF("  Gyro: [%.4f, %.4f, %.4f] rad/s\n", 
                     _telemetryData.gyroX, 
                     _telemetryData.gyroY, 
                     _telemetryData.gyroZ);
        
        // IMU - Acelerômetro
        DEBUG_PRINTF("  Accel: [%.4f, %.4f, %.4f] m/s²\n", 
                     _telemetryData.accelX, 
                     _telemetryData.accelY, 
                     _telemetryData.accelZ);
        
        // Umidade (se disponível)
        if (!isnan(_telemetryData.humidity)) {
            DEBUG_PRINTF("  Umidade: %.1f%%\n", _telemetryData.humidity);
        }
        
        // Qualidade do ar (se disponível)
        if (!isnan(_telemetryData.co2) || !isnan(_telemetryData.tvoc)) {
            DEBUG_PRINTF("  CO2: %.0f ppm, TVOC: %.0f ppb\n", 
                        _telemetryData.co2, 
                        _telemetryData.tvoc);
        }
        
        // Magnetômetro (se disponível)
        if (!isnan(_telemetryData.magX)) {
            DEBUG_PRINTF("  Mag: [%.2f, %.2f, %.2f] µT\n", 
                        _telemetryData.magX, 
                        _telemetryData.magY, 
                        _telemetryData.magZ);
        }
    }
    
    // ========================================
    // ENVIAR VIA COMMUNICATION MANAGER
    // ========================================
    bool sendSuccess = _comm.sendTelemetry(_telemetryData);
    
    if (sendSuccess) {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTLN("[TelemetryManager] Telemetria enviada com sucesso!");
        }
    } else {
        if (activeModeConfig->serialLogsEnabled) {
            DEBUG_PRINTLN("[TelemetryManager] ERRO ao enviar telemetria");
        }
    }
}



void TelemetryManager::_saveToStorage() {
    if (!_storage.isAvailable()) return;
    
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[TelemetryManager] Salvando dados no SD [%s]...\n", _rtc.getDateTime().c_str());
    } else {
        DEBUG_PRINTLN("[TelemetryManager] Salvando dados no SD...");
    }
    
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

    // Mostrar hora do RTC ou altitude
    String timeLine;
    if (_rtc.isInitialized()) {
        String dt = _rtc.getDateTime();
        timeLine = "RTC: " + dt.substring(11, 19);  // Pegar apenas HH:MM:SS
    } else {
        if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
            timeLine = "CO2: " + String(_sensors.getCO2(), 0) + "ppm";
        } else {
            timeLine = "Alt: " + String(_sensors.getAltitude(), 0) + "m";
        }
    }
    _display.drawString(0, 36, timeLine);

    String statusStr = "";
    statusStr += _comm.isConnected() ? "[WiFi]" : "[NoWiFi]";
    statusStr += _payload.isOnline() ? "[LoRa]" : "[NoLoRa]";
    statusStr += _storage.isAvailable() ? "[SD]" : "[NoSD]";
    statusStr += _rtc.isInitialized() ? "[RTC]" : "";
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

    if (_sensors.isCCS811Online() && !isnan(_sensors.getCO2())) {
        String co2Str = "CO2: " + String(_sensors.getCO2(), 0) + " ppm";
        _display.drawString(0, 48, co2Str);
    }

    String bottomLine = "Heap: " + String(ESP.getFreeHeap() / 1024) + "K";
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
