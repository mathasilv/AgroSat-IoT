/**
 * @file SI7021Manager.h
 * @brief SI7021Manager V3.0 - FIX Compilação
 */
#ifndef SI7021MANAGER_H
#define SI7021MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class SI7021Manager {
public:
    SI7021Manager();
    
    bool begin();
    void update();
    void reset();
    
    float getHumidity() const;
    float getTemperature() const;
    bool isOnline() const { return _online; }
    bool isTempValid() const { return _tempValid; }
    uint8_t getFailCount() const { return _failCount; }
    uint32_t getWarmupProgress() const;
    
    void printStatus() const;
    
private:
    float _humidity, _temperature;
    bool _online, _tempValid;
    uint8_t _failCount;
    uint32_t _initTime, _lastReadTime, _warmupProgress;
    
    static constexpr uint32_t READ_INTERVAL = 2000;
    static constexpr uint32_t WARMUP_TIME = 5000;
    
    // CONSTANTES LOCAIS (sem conflito com config.h)
    static constexpr uint8_t _SI7021_ADDR = 0x40;
    static constexpr uint8_t _CMD_SOFT_RESET = 0xFE;
    static constexpr uint8_t _CMD_MEASURE_RH_NOHOLD = 0xF5;
    static constexpr uint8_t _CMD_MEASURE_T_NOHOLD = 0xF3;
    
    bool _testRealHumidityRead();
    bool _validateHumidity(float hum);
    bool _validateTemperature(float temp);
};

#endif
