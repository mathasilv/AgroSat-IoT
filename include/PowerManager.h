#pragma once

/**
 * @file PowerManager.h
 * @brief Gerenciamento de energia usando HAL ADC
 * @version 2.0.0
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "hal/hal.h"
#include "config.h"

class PowerManager {
public:
    PowerManager();
    
    bool begin();
    void update();
    
    float getVoltage();
    float getPercentage();
    float getCurrent();
    float getPower();
    uint16_t getTimeRemaining();
    
    bool isCritical();
    bool isLow();
    
    void enablePowerSave();
    void disablePowerSave();
    void deepSleep(uint64_t durationSeconds);
    void getStatistics(float& avgVoltage, float& minVoltage, float& maxVoltage);

private:
    float _voltage;
    float _percentage;
    float _avgVoltage;
    float _minVoltage;
    float _maxVoltage;
    uint32_t _lastReadTime;
    uint16_t _sampleCount;
    bool _powerSaveEnabled;
    
    float _readVoltage();
    float _voltageToPercentage(float voltage);
};

#endif // POWER_MANAGER_H