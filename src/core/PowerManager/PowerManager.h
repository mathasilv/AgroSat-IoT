#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "config.h"

class PowerManager {
public:
    PowerManager();

    bool begin();
    void update();

    float getVoltage();
    float getPercentage();

    bool isCritical();
    bool isLow();

    void enablePowerSave();
    void disablePowerSave();

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
