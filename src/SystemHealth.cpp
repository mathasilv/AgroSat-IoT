/**
 * @file SystemHealth.cpp
 * @brief Implementação do monitoramento de saúde do sistema
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
    _healthy(true),
    _systemStatus(STATUS_OK),
    _errorCount(0),
    _bootTime(0),
    _missionStartTime(0),
    _missionActive(false),
    _lastWatchdogFeed(0),
    _lastHealthCheck(0),
    _minFreeHeap(0xFFFFFFFF),
    _maxCPUTemp(0.0)
{
}

bool SystemHealth::begin() {
    DEBUG_PRINTLN("[SystemHealth] Inicializando monitoramento...");
    
    _bootTime = millis();
    
    // Configurar watchdog timer
    DEBUG_PRINTF("[SystemHealth] Configurando watchdog (%d segundos)...\n", 
                 WATCHDOG_TIMEOUT);
    
    esp_task_wdt_init(WATCHDOG_TIMEOUT, true);  // Timeout e panic
    esp_task_wdt_add(NULL);  // Adicionar task atual
    
    _lastWatchdogFeed = millis();
    
    // Obter memória livre inicial
    _minFreeHeap = ESP.getFreeHeap();
    
    DEBUG_PRINTF("[SystemHealth] Memória livre: %d bytes\n", _minFreeHeap);
    DEBUG_PRINTF("[SystemHealth] CPU: %.1f MHz\n", getCpuFrequencyMhz() / 1.0);
    
    return true;
}

void SystemHealth::_updateHeapStatus(uint32_t now) {
    const uint32_t HEAP_CHECK_INTERVAL_MS = 30000;
    if (now - _lastHeapCheck < HEAP_CHECK_INTERVAL_MS) {
        return;
    }
    _lastHeapCheck = now;

    uint32_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < _minHeapSeen) {
        _minHeapSeen = currentHeap;
    }

    if (currentHeap < 5000) {
        _heapStatus = HeapStatus::FATAL_HEAP;
        reportError(STATUS_WATCHDOG, "Heap fatal");
    } else if (currentHeap < 8000) {
        _heapStatus = HeapStatus::CRITICAL_HEAP;
        reportError(STATUS_WATCHDOG, "Heap critical");
    } else if (currentHeap < 15000) {
        _heapStatus = HeapStatus::LOW_HEAP;
        reportError(STATUS_WATCHDOG, "Heap low");
    } else {
        _heapStatus = HeapStatus::OK;
    }
}
void SystemHealth::update() {
    uint32_t currentTime = millis();

    if (currentTime - _lastHealthCheck >= SYSTEM_HEALTH_INTERVAL) {
        _lastHealthCheck = currentTime;
        _checkSystemHealth();
    }

    _updateHeapStatus(currentTime);

    feedWatchdog();
}

void SystemHealth::feedWatchdog() {
    uint32_t currentTime = millis();
    
    // Alimentar watchdog a cada 5 segundos (timeout é 30s)
    if (currentTime - _lastWatchdogFeed >= 5000) {
        esp_task_wdt_reset();
        _lastWatchdogFeed = currentTime;
    }
}

void SystemHealth::reportError(uint8_t errorCode, const String& description) {
    _errorCount++;
    _systemStatus |= errorCode;  // Adicionar flag de erro
    
    DEBUG_PRINTF("[SystemHealth] ERRO #%d (0x%02X): %s\n", 
                 _errorCount, errorCode, description.c_str());
    
    // Verificar se sistema ainda está saudável
    _checkSystemHealth();
}

float SystemHealth::getCPUTemperature() {
    return _readCPUTemperature();
}

unsigned long SystemHealth::getUptime() {
    return millis() - _bootTime;
}

unsigned long SystemHealth::getMissionTime() {
    if (!_missionActive) return 0;
    return millis() - _missionStartTime;
}

uint32_t SystemHealth::getFreeHeap() {
    return ESP.getFreeHeap();
}

float SystemHealth::getCPUUsage() {
    // Implementação simplificada
    // Em produção, medir tempo em tasks vs idle
    return 0.0;  // Placeholder
}

bool SystemHealth::isHealthy() {
    return _healthy;
}

uint8_t SystemHealth::getSystemStatus() {
    return _systemStatus;
}

void SystemHealth::startMission() {
    _missionStartTime = millis();
    _missionActive = true;
    
    DEBUG_PRINTLN("[SystemHealth] ========================================");
    DEBUG_PRINTLN("[SystemHealth]     MISSÃO AGROSAT-IoT INICIADA");
    DEBUG_PRINTLN("[SystemHealth] ========================================");
    DEBUG_PRINTF("[SystemHealth] Duração: %.1f horas\n", 
                 MISSION_DURATION_MS / 3600000.0);
}

bool SystemHealth::isMissionActive() {
    if (!_missionActive) return false;
    
    // Verificar se missão ainda está dentro do tempo
    unsigned long missionTime = getMissionTime();
    return (missionTime < MISSION_DURATION_MS);
}

uint16_t SystemHealth::getErrorCount() {
    return _errorCount;
}

void SystemHealth::resetErrorCount() {
    _errorCount = 0;
    _systemStatus = STATUS_OK;
    DEBUG_PRINTLN("[SystemHealth] Contadores de erro resetados");
}

void SystemHealth::getStatistics(uint32_t& uptime, uint32_t& freeHeap, 
                                float& cpuTemp, uint16_t& errors) {
    uptime = getUptime();
    freeHeap = getFreeHeap();
    cpuTemp = getCPUTemperature();
    errors = _errorCount;
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

void SystemHealth::_checkSystemHealth() {
    // Verificar memória
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _minFreeHeap) {
        _minFreeHeap = freeHeap;
    }
    
    // Alerta de memória baixa (< 10KB)
    if (freeHeap < 10240) {
        DEBUG_PRINTF("[SystemHealth] AVISO: Memória baixa (%d bytes)\n", freeHeap);
        _healthy = false;
    }
    
    // Verificar temperatura
    float cpuTemp = _readCPUTemperature();
    if (cpuTemp > _maxCPUTemp) {
        _maxCPUTemp = cpuTemp;
    }
    
    // Alerta de temperatura alta (> 75°C)
    if (cpuTemp > 75.0) {
        DEBUG_PRINTF("[SystemHealth] AVISO: Temperatura alta (%.1f°C)\n", cpuTemp);
        _systemStatus |= STATUS_TEMP_ALARM;
    }
    
    // Verificar status dos subsistemas
    if (_systemStatus != STATUS_OK) {
        _healthy = false;
    } else {
        _healthy = true;
    }
}

float SystemHealth::_readCPUTemperature() {
    // Ler temperatura interna do ESP32
    // Fórmula: T = (raw - 32) / 1.8
    // Nota: sensor interno tem precisão limitada (~±5°C)
    
    uint8_t raw = temprature_sens_read();
    float tempF = (raw - 32) / 1.8;
    float tempC = (tempF - 32.0) * 5.0 / 9.0;
    
    return tempC;
}
