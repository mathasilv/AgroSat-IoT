#ifndef DS3231_HAL_H
#define DS3231_HAL_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "config.h"

struct RtcDateTime {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
};

class DS3231Hal {
public:
    explicit DS3231Hal(HAL::I2C& i2c, uint8_t addr = DS3231_ADDRESS);

    bool begin();
    bool lostPower();
    void clearLostPowerFlag();

    bool adjust(const RtcDateTime& dt);
    bool now(RtcDateTime& dt);
    uint32_t unixtime(const RtcDateTime& dt) const;

private:
    HAL::I2C* _i2c;
    uint8_t   _addr;
    bool      _initialized;

    static uint8_t _bcd2bin(uint8_t v) { return (v & 0x0F) + 10 * (v >> 4); }
    static uint8_t _bin2bcd(uint8_t v) { return (v % 10) | ((v / 10) << 4); }
};

#endif // DS3231_HAL_H
