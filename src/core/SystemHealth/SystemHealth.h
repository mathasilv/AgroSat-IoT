/**
 * @file SystemHealth.h
 * @brief Monitoramento de Saúde do Sistema
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"

class SystemHealth {
public:
    SystemHealth();

    // Enum seguro contra macros do Arduino (LOW)
    enum class HeapStatus { 
        HEAP_OK, 
        HEAP_LOW, 
        HEAP_CRITICAL, 
        HEAP_FATAL 
    };

    bool begin();
    void update();
    void feedWatchdog();

    void reportError(uint8_t errorCode, const String& description);
    
    float getCPUTemperature();
    uint32_t getFreeHeap();
    uint32_t getMinFreeHeap() const { return _minFreeHeap; }
    unsigned long getUptime();
    unsigned long getMissionTime();
    uint16_t getErrorCount() const { return _errorCount; }
    uint8_t getSystemStatus() const { return _systemStatus; }
    HeapStatus getHeapStatus() const { return _heapStatus; }

    void startMission();
    bool isMissionActive() const { return _missionActive; }

private:
    bool _healthy;
    uint8_t _systemStatus;
    uint16_t _errorCount;
    
    uint32_t _minFreeHeap;
    HeapStatus _heapStatus;
    uint32_t _lastHeapCheck;

    unsigned long _bootTime;
    unsigned long _missionStartTime;
    bool _missionActive;
    uint32_t _lastWatchdogFeed;
    uint32_t _lastHealthCheck;

    void _checkResources();
    float _readInternalTemp();
};

#endif