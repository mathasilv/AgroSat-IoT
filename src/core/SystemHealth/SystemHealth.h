/**
 * @file SystemHealth.h
 * @brief Monitoramento de Saúde do Sistema com Telemetria Detalhada
 * @version 2.0.0 (MELHORIA 5.6 - Health Telemetry Implementada)
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// NOVO 5.6: Estrutura de Telemetria de Saúde Detalhada
// ============================================================================
struct HealthTelemetry {
    uint32_t uptime;           // Segundos desde boot
    uint16_t resetCount;       // Número total de resets (persistido)
    uint8_t resetReason;       // Razão do último reset
    uint32_t minFreeHeap;      // Menor heap livre já visto
    uint32_t currentFreeHeap;  // Heap livre atual
    float cpuTemp;             // Temperatura CPU (°C)
    uint8_t sdCardStatus;      // 0=OK, 1=Erro
    uint16_t crcErrors;        // Erros de CRC acumulados
    uint16_t i2cErrors;        // Erros I2C acumulados
    uint16_t watchdogResets;   // Resets por watchdog
    uint8_t currentMode;       // Modo operacional atual
    float batteryVoltage;      // Tensão da bateria
};

class SystemHealth {
public:
    SystemHealth();

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
    
    // Getters
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
    
    // NOVO 5.6: Telemetria de Saúde Detalhada
    HealthTelemetry getHealthTelemetry();
    void incrementCRCError() { _crcErrors++; }
    void incrementI2CError() { _i2cErrors++; }
    void setSDCardStatus(bool ok) { _sdCardStatus = ok ? 0 : 1; }
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
    
    // NOVO 5.6: Contadores de Health
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