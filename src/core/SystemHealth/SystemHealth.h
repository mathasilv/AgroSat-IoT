/**
 * @file SystemHealth.h
 * @brief Monitoramento de Saúde em Tempo Real (Status Dinâmico)
 * @version 2.2.0
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "config.h"

struct HealthTelemetry {
    uint32_t uptime;
    uint16_t resetCount;
    uint8_t resetReason;
    uint32_t minFreeHeap;
    uint32_t currentFreeHeap;
    float cpuTemp;
    uint8_t sdCardStatus;
    uint16_t crcErrors;
    uint16_t i2cErrors;
    uint16_t watchdogResets;
    uint8_t currentMode;
    float batteryVoltage;
};

class SystemHealth {
public:
    SystemHealth();

    enum class HeapStatus { 
        HEAP_OK, HEAP_LOW, HEAP_CRITICAL, HEAP_FATAL 
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
    
    HealthTelemetry getHealthTelemetry();
    void incrementCRCError() { _crcErrors++; }
    void incrementI2CError() { _i2cErrors++; }
    
    // Status do Cartão SD (Específico)
    void setSDCardStatus(bool ok) { 
        _sdCardStatus = ok ? 0 : 1; 
        setSystemError(STATUS_SD_ERROR, !ok);
    }

    // =========================================================
    // NOVO: Método Genérico para Status em Tempo Real
    // =========================================================
    void setSystemError(uint8_t errorFlag, bool active) {
        if (active) {
            // Só incrementa o contador se o erro for NOVO (borda de subida)
            // Isso evita contagem infinita enquanto o erro persiste
            if (!(_systemStatus & errorFlag)) {
                _errorCount++;
            }
            _systemStatus |= errorFlag;  // Liga o bit
        } else {
            _systemStatus &= ~errorFlag; // Desliga o bit
        }
    }

    void setCurrentMode(uint8_t mode) { _currentMode = mode; }
    void setBatteryVoltage(float voltage) { _batteryVoltage = voltage; }

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
    
    uint16_t _resetCount;
    uint8_t _resetReason;
    uint16_t _crcErrors;
    uint16_t _i2cErrors;
    uint16_t _watchdogResets;
    uint8_t _sdCardStatus;
    uint8_t _currentMode;
    float _batteryVoltage;
    
    Preferences _prefs;

    void _checkResources();
    float _readInternalTemp();
    void _loadPersistentData();
    void _savePersistentData();
    void _incrementResetCount();
};

#endif