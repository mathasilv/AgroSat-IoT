/**
 * @file SystemHealth.cpp
 * @brief Implementação do Monitoramento
 */

#include "SystemHealth.h"

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
    _lastWatchdogFeed(0), _lastHealthCheck(0)
{}

bool SystemHealth::begin() {
    DEBUG_PRINTLN("[SystemHealth] Inicializando...");
    _bootTime = millis();
    _minFreeHeap = ESP.getFreeHeap();

    esp_task_wdt_init(WATCHDOG_TIMEOUT, true); 
    esp_task_wdt_add(NULL); 
    
    DEBUG_PRINTF("[SystemHealth] Watchdog: %d s | Heap: %u bytes\n", 
                 WATCHDOG_TIMEOUT, _minFreeHeap);
    
    return true;
}

void SystemHealth::update() {
    uint32_t now = millis();

    if (now - _lastWatchdogFeed > (WATCHDOG_TIMEOUT * 1000 / 3)) {
        esp_task_wdt_reset();
        _lastWatchdogFeed = now;
    }

    if (now - _lastHealthCheck > SYSTEM_HEALTH_INTERVAL) {
        _checkResources();
        _lastHealthCheck = now;
    }
}

void SystemHealth::feedWatchdog() {
    esp_task_wdt_reset();
    _lastWatchdogFeed = millis();
}

void SystemHealth::_checkResources() {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _minFreeHeap) _minFreeHeap = freeHeap;

    // Lógica de Memória Crítica e Fatal
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