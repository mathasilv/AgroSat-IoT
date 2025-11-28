#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <HAL/interface/I2C.h>
#include "config.h"
#include "DS3231Hal.h"   // <-- obrigatório

class RTCManager {
public:
    explicit RTCManager(HAL::I2C& i2c);

    bool begin();
    bool syncWithNTP();

    bool    isInitialized() const { return _initialized; }
    String  getDateTime();
    uint32_t getUnixTime();

private:
    DS3231Hal _rtc;
    HAL::I2C* _i2c;
    bool      _initialized;

    bool  _detectRTC();
    time_t _applyOffset(time_t utcTime) const;
};

#endif
