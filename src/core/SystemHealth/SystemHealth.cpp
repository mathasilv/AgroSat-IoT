/**
 * @file SystemHealth.cpp
 * @brief Implementação do Monitoramento com Telemetria Detalhada
 * @version 2.2.1 (FIX: Watchdog Dinâmico)
 */

#include "SystemHealth.h"
#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

SystemHealth::SystemHealth() :
    _healthy(true), _systemStatus(STATUS_OK), _errorCount(0),
    _minFreeHeap(0xFFFFFFFF), _heapStatus(HeapStatus::HEAP_OK), _lastHeapCheck(0),
    _bootTime(0), _missionStartTime(0), _missionActive(false),
    _lastWatchdogFeed(0), _lastHealthCheck(0),
    _currentWdtTimeout(WATCHDOG_TIMEOUT_PREFLIGHT), // Default seguro
    _resetCount(0), _resetReason(0), _crcErrors(0), _i2cErrors(0),
    _watchdogResets(0), _sdCardStatus(0), _currentMode(0), _batteryVoltage(0.0f)
{}

bool SystemHealth::begin() {
    DEBUG_PRINTLN("[SystemHealth] Inicializando...");
    _bootTime = millis();
    _minFreeHeap = ESP.getFreeHeap();
    
    _loadPersistentData();
    
    _resetReason = (uint8_t)esp_reset_reason();
    
    if (_resetReason == ESP_RST_TASK_WDT || _resetReason == ESP_RST_WDT) {
        _watchdogResets++;
        DEBUG_PRINTF("[SystemHealth] Reset por Watchdog! Total: %d\n", _watchdogResets);
    }
    
    _incrementResetCount();

    // CORRIGIDO: Usa a variável _currentWdtTimeout em vez de constante fixa
    esp_task_wdt_init(_currentWdtTimeout, true); 
    esp_task_wdt_add(NULL); 
    
    DEBUG_PRINTF("[SystemHealth] Watchdog: %d s | Heap: %u bytes | ResetCount: %d\n", 
                 _currentWdtTimeout, _minFreeHeap, _resetCount);
    
    return true;
}

void SystemHealth::update() {
    uint32_t now = millis();

    // CORRIGIDO: Usa o método centralizado feedWatchdog()
    // Feed a cada 1/3 do tempo configurado
    if (now - _lastWatchdogFeed > (_currentWdtTimeout * 1000 / 3)) {
        feedWatchdog(); // <--- AQUI (era esp_task_wdt_reset() + atualização manual)
    }

    if (now - _lastHealthCheck > SYSTEM_HEALTH_INTERVAL) {
        _checkResources();
        _lastHealthCheck = now;
        _savePersistentData();
    }
}
// === NOVO: Reconfigura o Hardware do Watchdog ===
void SystemHealth::setWatchdogTimeout(uint32_t seconds) {
    if (seconds == _currentWdtTimeout) return; // Evita reconfiguração desnecessária

    // É necessário desinicializar antes de reconfigurar no ESP32
    esp_task_wdt_deinit();
    
    _currentWdtTimeout = seconds;
    
    esp_task_wdt_init(_currentWdtTimeout, true);
    esp_task_wdt_add(NULL); // Readiciona a task atual (loop)
    
    DEBUG_PRINTF("[SystemHealth] Watchdog reconfigurado para %d segundos\n", _currentWdtTimeout);
    
    // Força um feed imediato para sincronizar
    feedWatchdog();
}

void SystemHealth::feedWatchdog() {
    esp_task_wdt_reset();
    _lastWatchdogFeed = millis();
}

void SystemHealth::_checkResources() {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _minFreeHeap) _minFreeHeap = freeHeap;

    if (freeHeap < 5000) {
        _heapStatus = HeapStatus::HEAP_FATAL;
        reportError(STATUS_WATCHDOG, "Heap FATAL (<5kB)");
    } else if (freeHeap < 10000) {
        _heapStatus = HeapStatus::HEAP_CRITICAL;
        if (!(_systemStatus & STATUS_WATCHDOG)) { 
            reportError(STATUS_WATCHDOG, "Heap Crítico (<10kB)");
        }
    } else if (freeHeap < 30000) {
        _heapStatus = HeapStatus::HEAP_LOW;
    } else {
        _heapStatus = HeapStatus::HEAP_OK;
    }

    float cpuTemp = _readInternalTemp();
    if (cpuTemp > 80.0) {
        reportError(STATUS_TEMP_ALARM, "CPU Superaquecida");
    }
}

float SystemHealth::_readInternalTemp() {
    uint8_t raw = temprature_sens_read();
    return (raw - 32) / 1.8f; 
}

void SystemHealth::reportError(uint8_t errorCode, const String& description) {
    _errorCount++;
    _systemStatus |= errorCode;
    DEBUG_PRINTF("[SystemHealth] ERRO #%d (0x%02X): %s\n", 
                 _errorCount, errorCode, description.c_str());
}

void SystemHealth::startMission() {
    _missionStartTime = millis();
    _missionActive = true;
    DEBUG_PRINTLN("[SystemHealth] Missão Iniciada!");
}

unsigned long SystemHealth::getMissionTime() {
    if (!_missionActive) return 0;
    return millis() - _missionStartTime;
}

unsigned long SystemHealth::getUptime() {
    return millis() - _bootTime;
}

float SystemHealth::getCPUTemperature() {
    return _readInternalTemp();
}

uint32_t SystemHealth::getFreeHeap() {
    return ESP.getFreeHeap();
}

HealthTelemetry SystemHealth::getHealthTelemetry() {
    HealthTelemetry health;
    health.uptime = getUptime() / 1000;
    health.resetCount = _resetCount;
    health.resetReason = _resetReason;
    health.minFreeHeap = _minFreeHeap;
    health.currentFreeHeap = ESP.getFreeHeap();
    health.cpuTemp = getCPUTemperature();
    health.sdCardStatus = _sdCardStatus;
    health.crcErrors = _crcErrors;
    health.i2cErrors = _i2cErrors;
    health.watchdogResets = _watchdogResets;
    health.currentMode = _currentMode;
    health.batteryVoltage = _batteryVoltage;
    return health;
}

void SystemHealth::_loadPersistentData() {
    if (!_prefs.begin("system_health", true)) {
        DEBUG_PRINTLN("[SystemHealth] Falha ao abrir NVS (leitura)");
        return;
    }
    _resetCount = _prefs.getUShort("reset_cnt", 0);
    _watchdogResets = _prefs.getUShort("wdt_resets", 0);
    _crcErrors = _prefs.getUShort("crc_err", 0);
    _i2cErrors = _prefs.getUShort("i2c_err", 0);
    _prefs.end();
    DEBUG_PRINTF("[SystemHealth] Dados carregados: Resets=%d, WDT=%d\n", 
                 _resetCount, _watchdogResets);
}

void SystemHealth::_savePersistentData() {
    if (!_prefs.begin("system_health", false)) {
        DEBUG_PRINTLN("[SystemHealth] Falha ao abrir NVS (escrita)");
        return;
    }
    _prefs.putUShort("reset_cnt", _resetCount);
    _prefs.putUShort("wdt_resets", _watchdogResets);
    _prefs.putUShort("crc_err", _crcErrors);
    _prefs.putUShort("i2c_err", _i2cErrors);
    _prefs.end();
}

void SystemHealth::_incrementResetCount() {
    _resetCount++;
    if (!_prefs.begin("system_health", false)) return;
    _prefs.putUShort("reset_cnt", _resetCount);
    _prefs.end();
}