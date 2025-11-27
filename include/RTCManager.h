#pragma once

/**
 * @file RTCManager.h
 * @brief Gerenciador RTC usando HAL I2C
 * @version 2.0.0
 */

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include "hal/hal.h"
#include "config.h"

class RTCManager {
public:
    RTCManager();
    bool begin();
    bool syncWithNTP();
    
    bool isInitialized() const { return _initialized; }
    String getDateTime();
    uint32_t getUnixTime();
    
private:
    RTC_DS3231 _rtc;
    bool _initialized;
    
    bool _detectRTC();
    time_t _applyOffset(time_t utcTime) const;
};

#endif