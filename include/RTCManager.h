#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <HAL/interface/I2C.h>
#include "config.h"

class RTCManager {
public:
    // Agora recebe o barramento HAL I2C
    explicit RTCManager(HAL::I2C& i2c);

    // Usa o HAL interno, não recebe mais Wire
    bool begin();
    bool syncWithNTP();
    
    // Métodos essenciais
    bool isInitialized() const { return _initialized; }
    String getDateTime();      // "2025-11-11 00:49:30"
    uint32_t getUnixTime();
    
private:
    RTC_DS3231 _rtc;
    HAL::I2C* _i2c;
    bool _initialized;
    
    bool _detectRTC();
    time_t _applyOffset(time_t utcTime) const;
};

#endif
