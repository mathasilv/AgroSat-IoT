/**
 * @file RTCManager.h
 * @brief Gerenciador de Tempo (RTC DS3231 + NTP)
 */

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

class RTCManager {
public:
    RTCManager();
    
    // Inicialização
    bool begin(TwoWire* wire = &Wire);
    void update();

    // Sincronização
    bool syncWithNTP();
    
    // Status
    bool isInitialized() const;
    
    // Getters de Tempo
    String getDateTime();           // Local
    String getUTCDateTime();        // UTC
    uint32_t getUnixTime();         // Epoch UTC
    DateTime getNow();

    // Alias de compatibilidade para MissionController
    String getLocalDateTime() { return getDateTime(); } 

private:
    RTC_DS3231 _rtc;
    TwoWire* _wire;
    bool _initialized;
    bool _lost_power;
    
    // Configurações
    static constexpr uint8_t DS3231_ADDR = 0x68;
    static constexpr long GMT_OFFSET_SEC = -3 * 3600; // UTC-3
    static constexpr int  DAYLIGHT_OFFSET_SEC = 0;
    static constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
    static constexpr const char* NTP_SERVER_2 = "time.nist.gov";
    
    // Helpers
    bool _detectRTC();
    void _syncSystemToRTC();
};

#endif