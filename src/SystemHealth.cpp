/**
 * @file SystemHealth.cpp
 * @brief Clean: Monitoramento de saúde embarcado, só o essencial para missão
 * @version 2.3.0
 * @date 2025-11-07
 *
 * CHANGELOG v2.3.0:
 * - Removeu getCPUUsage, getStatistics, resetErrorCount, flag _healthy
 * - Corrigida fórmula de temperatura CPU
 * - Feed watchdog usa macro do config
 * - Debug dual-mode (LOG_PREFLIGHT/LOG_ERROR)
 * - Health check só atualiza status global
 */

#include "SystemHealth.h"
#include "mission_config.h"

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

SystemHealth::SystemHealth() :
    _systemStatus(STATUS_OK),
    _errorCount(0),
    _bootTime(0),
    _missionStartTime(0),
    _missionActive(false),
    _lastWatchdogFeed(0),
    _lastHealthCheck(0),
    _minFreeHeap(0xFFFFFFFF),
    _maxCPUTemp(0.0)
{}

bool SystemHealth::begin() {
    LOG_PREFLIGHT("[SystemHealth] Inicializando monitoramento...\n");
    _bootTime = millis();
    LOG_PREFLIGHT("[SystemHealth] Configurando watchdog (%d segundos)...\n", WATCHDOG_TIMEOUT_S);
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
    _lastWatchdogFeed = millis();
    _minFreeHeap = ESP.getFreeHeap();
    LOG_PREFLIGHT("[SystemHealth] Memória livre: %d bytes\n", _minFreeHeap);
    LOG_PREFLIGHT("[SystemHealth] CPU: %.1f MHz\n", getCpuFrequencyMhz() / 1.0);
    return true;
}

void SystemHealth::update() {
    uint32_t currentTime = millis();
    if (currentTime - _lastHealthCheck >= SYSTEM_HEALTH_INTERVAL_MS) {
        _lastHealthCheck = currentTime;
        _checkSystemHealth();
    }
    feedWatchdog();
}

void SystemHealth::feedWatchdog() {
    uint32_t currentTime = millis();
    // Alimenta watchdog com intervalo seguro do config
    if (currentTime - _lastWatchdogFeed >= (WATCHDOG_TIMEOUT_S * 500)) {
        esp_task_wdt_reset();
        _lastWatchdogFeed = currentTime;
    }
}

void SystemHealth::reportError(uint8_t errorCode, const String& description) {
    _errorCount++;
    _systemStatus |= errorCode;
    LOG_ERROR("[SystemHealth] ERRO #%d (0x%02X): %s\n", _errorCount, errorCode, description.c_str());
    _checkSystemHealth();
}

float SystemHealth::getCPUTemperature() {
    return _readCPUTemperature();
}

unsigned long SystemHealth::getUptime() { return millis() - _bootTime; }
unsigned long SystemHealth::getMissionTime() { return _missionActive ? (millis() - _missionStartTime) : 0; }
uint32_t SystemHealth::getFreeHeap() { return ESP.getFreeHeap(); }

bool SystemHealth::isHealthy() {
    // Agora calculado direto via status global
    return (_systemStatus == STATUS_OK);
}

uint8_t SystemHealth::getSystemStatus() { return _systemStatus; }

void SystemHealth::startMission() {
    _missionStartTime = millis();
    _missionActive = true;
    LOG_PREFLIGHT("[SystemHealth] ========================================\n");
    LOG_PREFLIGHT("[SystemHealth]     MISSÃO AGROSAT-IoT INICIADA\n");
    LOG_PREFLIGHT("[SystemHealth] ========================================\n");
    LOG_PREFLIGHT("[SystemHealth] Duração: %.1f horas\n", MISSION_DURATION_MS / 3600000.0);
}

bool SystemHealth::isMissionActive() {
    return _missionActive && (getMissionTime() < MISSION_DURATION_MS);
}

uint16_t SystemHealth::getErrorCount() { return _errorCount; }

// ==================== PRIVADOS ======================

void SystemHealth::_checkSystemHealth() {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _minFreeHeap) _minFreeHeap = freeHeap;
    if (freeHeap < 10240) { LOG_ERROR("[SystemHealth] AVISO: Memória baixa (%d bytes)\n", freeHeap); }
    float cpuTemp = _readCPUTemperature();
    if (cpuTemp > _maxCPUTemp) _maxCPUTemp = cpuTemp;
    if (cpuTemp > 75.0) { LOG_ERROR("[SystemHealth] AVISO: Temperatura alta (%.1f°C)\n", cpuTemp); _systemStatus |= STATUS_TEMP_ALARM; }
}

float SystemHealth::_readCPUTemperature() {
    uint8_t raw = temprature_sens_read();
    // Melhor aproximação para ESP32
    return (raw - 32) / 1.8;
}
