#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include "config.h"

class RTCManager {
public:
    RTCManager();
    bool begin(TwoWire* wire = &Wire);
    bool syncWithNTP();
    
    // Métodos essenciais
    bool isInitialized() const { return _initialized; }
    String getDateTime();  // Retorna: "2025-11-11 00:49:30"
    uint32_t getUnixTime();
    
private:
    RTC_DS3231 _rtc;
    TwoWire* _wire;
    bool _initialized;
    
    bool _detectRTC();
    time_t _applyOffset(time_t utcTime) const;
};

#endif
