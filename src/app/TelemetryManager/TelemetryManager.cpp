/**
 * @file TelemetryManager.cpp - Parte 1
 * @brief Gerenciador Central Completo
 * @version 10.0.0
 */

#include "TelemetryManager.h"
#include "config.h"

// Variáveis Globais de Configuração
bool currentSerialLogsEnabled = PREFLIGHT_CONFIG.serialLogsEnabled;
const ModeConfig* activeModeConfig = &PREFLIGHT_CONFIG;

TelemetryManager::TelemetryManager() :
    _sensors(), 
    _gps(),
    _power(), 
    _systemHealth(), 
    _rtc(), 
    _button(),
    _storage(), 
    _comm(), 
    _groundNodes(),
    _mission(_rtc, _groundNodes),
    _telemetryCollector(_sensors, _gps, _power, _systemHealth, _rtc, _groundNodes),
    _commandHandler(_sensors),
    _mode(MODE_INIT), 
    _missionActive(false),
    _lastTelemetrySend(0), 
    _lastStorageSave(0),
    _missionStartTime(0), 
    _lastSensorReset(0),
    _lastBeaconTime(0),      // NOVO 5.4
    _lastLinkBudgetCalc(0)   // NOVO 4.2
{
    memset(&_telemetryData, 0, sizeof(TelemetryData));
    
    // Inicializa floats com NAN
    _telemetryData.humidity = NAN;
    _telemetryData.co2 = NAN;
    _telemetryData.tvoc = NAN;
    _telemetryData.magX = NAN; 
    _telemetryData.magY = NAN; 
    _telemetryData.magZ = NAN;
    
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

    // Recuperação de Missão (se resetou durante voo)
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
    // 1. RTC
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
    
    // 6. GPS
    DEBUG_PRINTLN("[TelemetryManager] Init GPSManager");
    if (_gps.begin()) subsystemsOk++;

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
    DEBUG_PRINTF("[TelemetryManager] Init: %s, subsistemas=%d/8, heap usado=%lu bytes\n",
                 success ? "OK" : "ERRO", subsystemsOk, used);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================

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
    _power.adjustCpuFrequency();  // NOVO 5.2: CPU dinâmica
    _sensors.update(); 
    _gps.update();
    _comm.update();
    _rtc.update();

    _handleButtonEvents();
    _updateLEDIndicator(currentTime);

    // 3. Gerenciamento de Rádio e Rede
    _handleIncomingRadio();
    _maintainGroundNetwork();

    // 4. Coleta e Envio de Telemetria
    _telemetryCollector.collect(_telemetryData);
    
    // Atualizar SystemHealth com dados de telemetria (NOVO 5.6)
    _systemHealth.setCurrentMode((uint8_t)_mode);
    _systemHealth.setBatteryVoltage(_power.getVoltage());
    _systemHealth.setSDCardStatus(_storage.isAvailable());
    
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
    
    // === NOVO 5.4: BEACON AUTOMÁTICO EM SAFE MODE ===
    if (_mode == MODE_SAFE) {
        uint32_t beaconInterval = activeModeConfig->beaconInterval;
        if (beaconInterval > 0 && (currentTime - _lastBeaconTime >= beaconInterval)) {
            _sendSafeBeacon();
            _lastBeaconTime = currentTime;
        }
    }
    
    // === NOVO 4.2 + 5.2: LINK BUDGET E ADAPTIVE SF ===
    if (_gps.hasFix() && (currentTime - _lastLinkBudgetCalc >= 30000)) {
        _lastLinkBudgetCalc = currentTime;
        _updateLinkBudget();
        _applyAdaptiveSF();
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
            
            DEBUG_PRINTF("[TM] Node %u RX: RSSI=%d dBm, SNR=%.1f dB\n", 
                         rxData.nodeId, rssi, snr);
            
            // NOVO 5.2: Ajustar SF baseado na qualidade do link recebido
            _comm.adjustSFBasedOnLinkQuality(rssi, snr);
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

// ============================================================================
// Continuação TelemetryManager.cpp - Parte 2 FINAL
// ============================================================================

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
    
    DEBUG_PRINTF("[TelemetryManager] Modo: %d (LoRa=%d HTTP=%d Beacon=%d)\n", 
                 mode, activeModeConfig->loraEnabled, activeModeConfig->httpEnabled,
                 activeModeConfig->beaconInterval > 0);
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
        DEBUG_PRINTF("[TM] TX: UTC=%s | T=%.1f C | Bat=%.1f%% | Fix=%d | Nodes=%d\n",
                     _rtc.getUTCDateTime().c_str(), 
                     _telemetryData.temperature, 
                     _telemetryData.batteryPercentage,
                     _telemetryData.gpsFix,
                     buf.activeNodes);
    }
    
    _comm.sendTelemetry(_telemetryData, buf);
}

void TelemetryManager::_saveToStorage() {
    if (_storage.isAvailable()) {
        _storage.saveTelemetry(_telemetryData);
        
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

// ============================================================================
// NOVO 5.4: BEACON AUTOMÁTICO EM MODO SAFE
// ============================================================================
void TelemetryManager::_sendSafeBeacon() {
    uint8_t beacon[32];
    int offset = 0;
    
    // Header mágico: BEACON
    beacon[offset++] = 0xBE;
    beacon[offset++] = 0xAC;
    
    // Team ID
    beacon[offset++] = (TEAM_ID >> 8) & 0xFF;
    beacon[offset++] = TEAM_ID & 0xFF;
    
    // Modo atual
    beacon[offset++] = (uint8_t)_mode;
    
    // Tensão da bateria (centivolt)
    uint16_t batVoltageInt = (uint16_t)(_power.getVoltage() * 100);
    beacon[offset++] = (batVoltageInt >> 8) & 0xFF;
    beacon[offset++] = batVoltageInt & 0xFF;
    
    // Uptime (segundos)
    uint32_t uptime = _systemHealth.getUptime() / 1000;
    beacon[offset++] = (uptime >> 24) & 0xFF;
    beacon[offset++] = (uptime >> 16) & 0xFF;
    beacon[offset++] = (uptime >> 8) & 0xFF;
    beacon[offset++] = uptime & 0xFF;
    
    // Status do sistema
    beacon[offset++] = _systemHealth.getSystemStatus();
    
    // Contador de erros
    uint16_t errors = _systemHealth.getErrorCount();
    beacon[offset++] = (errors >> 8) & 0xFF;
    beacon[offset++] = errors & 0xFF;
    
    // Heap livre
    uint32_t freeHeap = _systemHealth.getFreeHeap();
    beacon[offset++] = (freeHeap >> 24) & 0xFF;
    beacon[offset++] = (freeHeap >> 16) & 0xFF;
    beacon[offset++] = (freeHeap >> 8) & 0xFF;
    beacon[offset++] = freeHeap & 0xFF;
    
    // NOVO 5.6: Dados de Health Telemetry
    HealthTelemetry health = _systemHealth.getHealthTelemetry();
    beacon[offset++] = (health.resetCount >> 8) & 0xFF;
    beacon[offset++] = health.resetCount & 0xFF;
    
    beacon[offset++] = health.resetReason;
    
    // GPS Fix (1 byte)
    beacon[offset++] = _gps.hasFix() ? 1 : 0;
    
    // Enviar beacon (prioriza transmissão mesmo se duty cycle próximo do limite)
    DEBUG_PRINTLN("[TM] ========================================");
    DEBUG_PRINTLN("[TM] ENVIANDO BEACON SAFE MODE");
    DEBUG_PRINTF("[TM] Bat: %.2fV | Uptime: %lu s | Heap: %lu bytes\n",
                 _power.getVoltage(), uptime, freeHeap);
    DEBUG_PRINTF("[TM] Erros: %d | Resets: %d | GPS: %s\n",
                 errors, health.resetCount, _gps.hasFix() ? "FIX" : "NOFIX");
    DEBUG_PRINTLN("[TM] ========================================");
    
    if (_comm.sendLoRa(beacon, offset)) {
        DEBUG_PRINTLN("[TM] Beacon SAFE enviado com sucesso!");
    } else {
        DEBUG_PRINTLN("[TM] ERRO ao enviar Beacon SAFE.");
    }
}

// ============================================================================
// NOVO 4.2: LINK BUDGET CALCULATOR
// ============================================================================
void TelemetryManager::_updateLinkBudget() {
    if (!_gps.hasFix()) {
        DEBUG_PRINTLN("[TM] Sem GPS fix. Link Budget não calculado.");
        return;
    }
    
    // Posição do satélite (do GPS)
    double satLat = _gps.getLatitude();
    double satLon = _gps.getLongitude();
    float satAlt = _gps.getAltitude() / 1000.0f;  // m -> km
    
    // Posição do nó terrestre (exemplo: Goiânia - UFG)
    // NOTA: Ajustar para posição real do nó de teste
    double groundLat = -16.6869;
    double groundLon = -49.2648;
    
    // Calcular link budget
    LinkBudget budget = _linkBudget.calculate(
        satLat, satLon, satAlt,
        groundLat, groundLon,
        _comm.getCurrentSF(),
        LORA_SIGNAL_BANDWIDTH
    );
    
    // Log detalhado
    DEBUG_PRINTLN("[TM] ========================================");
    DEBUG_PRINTLN("[TM] LINK BUDGET ATUALIZADO");
    DEBUG_PRINTF("[TM] Distância: %.1f / %.1f km\n", 
                 budget.currentDistance, budget.maxDistance);
    DEBUG_PRINTF("[TM] Path Loss: %.1f dB\n", budget.pathLoss);
    DEBUG_PRINTF("[TM] Link Margin: %.1f dB (%s)\n", 
                 budget.linkMargin, budget.isViable ? "VIÁVEL" : "CRÍTICO");
    DEBUG_PRINTF("[TM] SF Recomendado: %d (Atual: %d)\n", 
                 budget.recommendedSF, _comm.getCurrentSF());
    DEBUG_PRINTLN("[TM] ========================================");
}

// ============================================================================
// NOVO 5.2: ADAPTIVE SPREADING FACTOR
// ============================================================================
void TelemetryManager::_applyAdaptiveSF() {
    LinkBudget budget = _linkBudget.getLastBudget();
    
    // Estratégia 1: Baseado na viabilidade do link
    if (!budget.isViable) {
        DEBUG_PRINTLN("[TM] Link budget insuficiente! Forçando SF12.");
        _comm.setSpreadingFactor(LORA_SPREADING_FACTOR_SAFE);
        return;
    }
    
    // Estratégia 2: Baseado na margem de link
    if (budget.linkMargin > 15.0f) {
        // Margem excelente: usar SF mais baixo (mais rápido)
        _comm.setSpreadingFactor(7);
        DEBUG_PRINTLN("[TM] Margem excelente (>15dB) -> SF7");
    } 
    else if (budget.linkMargin > 10.0f) {
        _comm.setSpreadingFactor(8);
        DEBUG_PRINTLN("[TM] Margem boa (>10dB) -> SF8");
    }
    else if (budget.linkMargin > 5.0f) {
        // Margem moderada: usar SF recomendado pelo calculador
        _comm.setSpreadingFactor(budget.recommendedSF);
        DEBUG_PRINTF("[TM] Margem moderada -> SF%d (recomendado)\n", 
                     budget.recommendedSF);
    }
    else {
        // Margem baixa: ser conservador
        _comm.setSpreadingFactor(LORA_SPREADING_FACTOR_SAFE);
        DEBUG_PRINTLN("[TM] Margem baixa (<5dB) -> SF12 (conservador)");
    }
}

// ============================================================================
// COMANDOS
// ============================================================================
bool TelemetryManager::handleCommand(const String& cmd) {
    String cmdUpper = cmd;
    cmdUpper.toUpperCase();
    cmdUpper.trim();
    
    // Comando LINK_BUDGET (NOVO 4.2)
    if (cmdUpper == "LINK_BUDGET") {
        _updateLinkBudget();
        
        LinkBudget b = _linkBudget.getLastBudget();
        DEBUG_PRINTLN("=== LINK BUDGET ===");
        DEBUG_PRINTF("Distância: %.1f / %.1f km\n", b.currentDistance, b.maxDistance);
        DEBUG_PRINTF("Path Loss: %.1f dB\n", b.pathLoss);
        DEBUG_PRINTF("Link Margin: %.1f dB\n", b.linkMargin);
        DEBUG_PRINTF("SF Recomendado: %d\n", b.recommendedSF);
        DEBUG_PRINTF("Viável: %s\n", b.isViable ? "SIM" : "NÃO");
        DEBUG_PRINTLN("===================");
        return true;
    }
    
    // Comando START_MISSION
    if (cmdUpper == "START_MISSION") {
        startMission();
        return true;
    }
    
    // Comando STOP_MISSION
    if (cmdUpper == "STOP_MISSION") {
        stopMission();
        return true;
    }
    
    // Comando SAFE_MODE
    if (cmdUpper == "SAFE_MODE") {
        applyModeConfig(MODE_SAFE);
        _mode = MODE_SAFE;
        DEBUG_PRINTLN("[TM] SAFE MODE ATIVADO (Comando)");
        return true;
    }
    
    // Comando DUTY_CYCLE (NOVO 4.8)
    if (cmdUpper == "DUTY_CYCLE") {
        auto& dc = _comm.getDutyCycleTracker();
        DEBUG_PRINTLN("=== DUTY CYCLE ===");
        DEBUG_PRINTF("Usado: %lu ms / %lu ms\n", 
                     dc.getAccumulatedTxTime(), 360000);  // 6 min
        DEBUG_PRINTF("Percentual: %.1f%% (Limite: 10%%)\n", 
                     dc.getDutyCyclePercent());
        DEBUG_PRINTF("Disponível: %lu ms\n", dc.getRemainingTime());
        DEBUG_PRINTLN("==================");
        return true;
    }
    
    // Delegar para CommandHandler (sensores, calibração, etc.)
    return _commandHandler.handle(cmdUpper);
}

// Stubs de compatibilidade
void TelemetryManager::testLoRaTransmission() { 
    _comm.sendLoRa("TEST"); 
}

void TelemetryManager::sendCustomLoRa(const String& msg) { 
    _comm.sendLoRa(msg); 
}

void TelemetryManager::printLoRaStats() {
    DEBUG_PRINTLN("=== LoRa Stats ===");
    DEBUG_PRINTF("SF Atual: %d\n", _comm.getCurrentSF());
    DEBUG_PRINTF("Último RSSI: %d dBm\n", _comm.getLastRSSI());
    DEBUG_PRINTF("Último SNR: %.1f dB\n", _comm.getLastSNR());
    DEBUG_PRINTLN("==================");
}