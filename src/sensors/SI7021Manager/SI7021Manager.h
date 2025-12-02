/**
 * @file SI7021Manager.h
 * @brief SI7021Manager - Wrapper com cache e recuperação
 * @version 3.2
 */
#ifndef SI7021MANAGER_H
#define SI7021MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SI7021.h"  // ← USA A BIBLIOTECA

class SI7021Manager {
public:
    SI7021Manager();
    
    bool begin();
    void update();
    void reset();
    
    // Getters (wrappers sobre SI7021 nativo)
    float getTemperature() const { return _lastTemp; }
    float getHumidity() const { return _lastHum; }
    
    // Status
    bool isOnline() const { return _online && _si7021.isOnline(); }
    bool isTempValid() const { return _online && _lastTemp > -100.0; }
    bool isHumValid() const { return _online && _lastHum >= 0.0; }
    uint8_t getFailCount() const { return _failCount; }
    uint32_t getWarmupProgress() const { return _warmupProgress; }
    
    void printStatus() const;
    
private:
    SI7021 _si7021;  // ← INSTÂNCIA DO DRIVER NATIVO
    bool _online;
    float _lastTemp;
    float _lastHum;
    uint8_t _failCount;
    uint32_t _lastRead;
    uint32_t _initTime;
    uint32_t _warmupProgress;
    
    static constexpr uint32_t READ_INTERVAL_MS = 2000;  // 2s
    static constexpr uint32_t WARMUP_TIME_MS = 5000;    // 5s
};

#endif
