#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include "config.h"

struct RTCStatus {
    bool initialized;
    bool ntpSynced;
    bool timeValid;
    uint32_t bootCount;
    uint32_t lastSyncTime;
    float temperature;
};

class RTCManager {
public:
    RTCManager();
    
    bool begin();
    bool syncWithNTP(const char* ssid = nullptr, const char* password = nullptr);
    
    DateTime getDateTime();
    RTCStatus getStatus() const { return _status; }
    float getTemperature();
    
    void update();
    
    // Métodos auxiliares para outros managers
    bool isInitialized() const { return _status.initialized; }
    uint32_t getUnixTime();
    String getISO8601();
    String getTimeString();
    String getDateString();

private:
    RTC_DS3231 _rtc;
    RTCStatus _status;
    TwoWire* _wire;
    
    bool _detectRTC();
    void _syncSystemClock(const DateTime& dt);
    
    // EEPROM DS3231 (endereço 0x57)
    static constexpr uint8_t EEPROM_I2C_ADDR = 0x57;
    static constexpr uint16_t EEPROM_ADDR_BOOT_COUNT = 0x00;
    
    bool eepromWrite(uint16_t addr, const uint8_t* data, size_t len);
    bool eepromRead(uint16_t addr, uint8_t* data, size_t len);
};

#endif // RTC_MANAGER_H
