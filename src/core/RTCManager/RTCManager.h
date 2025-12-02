#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

// ✅ DEFINES NO HEADER - visíveis em todos .cpp
#define DATETIME_BUFFER_SIZE    24
#define RTC_I2C_TIMEOUT_MS      100
#define DS3231_ADDRESS          0x68

#ifndef RTC_TIMEZONE_OFFSET
#define RTC_TIMEZONE_OFFSET     (-3 * 3600)  // Brasil -3 UTC
#endif

#ifndef NTP_SERVER_PRIMARY
#define NTP_SERVER_PRIMARY      "pool.ntp.org"
#endif
#ifndef NTP_SERVER_SECONDARY
#define NTP_SERVER_SECONDARY    "time.nist.gov"
#endif

class RTCManager {
public:
    RTCManager();
    bool begin(TwoWire* wire = &Wire);
    bool syncWithNTP();
    
    bool isInitialized() const;
    String getDateTime();           // ✅ Original API
    uint32_t getUnixTime();         // ✅ Original API
    
    // ✅ Helpers para TelemetryManager (compatibilidade)
    String getUTCDateTime() { return getDateTime(); }  // Alias
    String getLocalDateTime() { return getDateTime(); } // Alias
    void update() {}  // No-op para compatibilidade
    
private:
    RTC_DS3231 _rtc;
    TwoWire* _wire;
    bool _initialized;
    bool _lost_power;
    mutable char _datetime_buffer[DATETIME_BUFFER_SIZE];
    
    bool _detectRTC();
    time_t _applyOffset(time_t utcTime) const;
};

// ✅ Inline com define disponível
inline uint32_t RTCManager::getUnixTime() {
    if (!_initialized) return 0;
    return static_cast<uint32_t>(_rtc.now().unixtime() + RTC_TIMEZONE_OFFSET);
}

inline bool RTCManager::isInitialized() const {
    return _initialized && !_lost_power;
}

#endif
