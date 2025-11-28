#include "DS3231Hal.h"

// Registradores principais (datasheet DS3231) [web:296][web:300]
static constexpr uint8_t REG_SECONDS = 0x00;
static constexpr uint8_t REG_MINUTES = 0x01;
static constexpr uint8_t REG_HOURS   = 0x02;
static constexpr uint8_t REG_DAY     = 0x03;
static constexpr uint8_t REG_DATE    = 0x04;
static constexpr uint8_t REG_MONTH   = 0x05;
static constexpr uint8_t REG_YEAR    = 0x06;
static constexpr uint8_t REG_CONTROL = 0x0E;
static constexpr uint8_t REG_STATUS  = 0x0F;

// STATUS bits
static constexpr uint8_t OSF_BIT     = 0x80; // Oscillator Stop Flag [web:296]

DS3231Hal::DS3231Hal(HAL::I2C& i2c, uint8_t addr)
    : _i2c(&i2c),
      _addr(addr),
      _initialized(false)
{
}

bool DS3231Hal::begin() {
    if (!_i2c) return false;

    // Ler segundos para verificar se responde (similar ao teu _detectRTC) [web:296][web:303]
    uint8_t secReg = _i2c->readRegister(_addr, REG_SECONDS);
    uint8_t seconds = secReg & 0x7F;
    if (seconds > 59) {
        return false;
    }

    // Habilitar oscilador, desabilitar square-wave se quiser (CONTROL) [web:296]
    uint8_t ctrl = _i2c->readRegister(_addr, REG_CONTROL);
    ctrl &= ~0x80;   // EOSC = 0 → oscilador ligado
    ctrl &= ~0x04;   // INTCN/SQW conforme necessidade (aqui mantemos simples)
    uint8_t buf[2] = {REG_CONTROL, ctrl};
    _i2c->write(_addr, buf, 2);

    _initialized = true;
    return true;
}

bool DS3231Hal::lostPower() {
    if (!_initialized || !_i2c) return false;
    uint8_t status = _i2c->readRegister(_addr, REG_STATUS);
    return (status & OSF_BIT) != 0;
}

void DS3231Hal::clearLostPowerFlag() {
    if (!_initialized || !_i2c) return;
    uint8_t status = _i2c->readRegister(_addr, REG_STATUS);
    status &= ~OSF_BIT;
    uint8_t buf[2] = {REG_STATUS, status};
    _i2c->write(_addr, buf, 2);
}

bool DS3231Hal::adjust(const RtcDateTime& dt) {
    if (!_initialized || !_i2c) return false;

    // Ano base 2000 (como RTClib) [web:296]
    uint8_t year  = dt.year >= 2000 ? dt.year - 2000 : 0;
    uint8_t month = dt.month;
    uint8_t day   = dt.day;
    uint8_t hour  = dt.hour;
    uint8_t min   = dt.minute;
    uint8_t sec   = dt.second;

    uint8_t data[8];
    data[0] = REG_SECONDS;
    data[1] = _bin2bcd(sec & 0x7F);
    data[2] = _bin2bcd(min);
    data[3] = _bin2bcd(hour);     // 24h mode
    data[4] = 0x01;               // dia da semana (1..7) — você pode calcular depois
    data[5] = _bin2bcd(day);
    data[6] = _bin2bcd(month);
    data[7] = _bin2bcd(year);

    return _i2c->write(_addr, data, sizeof(data));
}

bool DS3231Hal::now(RtcDateTime& dt) {
    if (!_initialized || !_i2c) return false;

    uint8_t reg = REG_SECONDS;
    if (!_i2c->write(_addr, &reg, 1)) return false;

    uint8_t buf[7] = {0};
    if (!_i2c->read(_addr, buf, sizeof(buf))) return false;

    uint8_t sec   = _bcd2bin(buf[0] & 0x7F);
    uint8_t min   = _bcd2bin(buf[1]);
    uint8_t hour  = _bcd2bin(buf[2] & 0x3F);     // 24h [web:296]
    uint8_t date  = _bcd2bin(buf[4]);
    uint8_t month = _bcd2bin(buf[5] & 0x1F);
    uint8_t year  = _bcd2bin(buf[6]);            // 0–99 → 2000–2099

    dt.year   = 2000 + year;
    dt.month  = month;
    dt.day    = date;
    dt.hour   = hour;
    dt.minute = min;
    dt.second = sec;

    return true;
}

// Implementação simples de unix time (UTC) baseada em algoritmo padrão
uint32_t DS3231Hal::unixtime(const RtcDateTime& dt) const {
    // Converter para dias desde 1970-01-01 e depois para segundos.
    // Implementação simples suficiente para 1970–2100.
    uint16_t y = dt.year;
    uint8_t  m = dt.month;
    uint8_t  d = dt.day;

    if (m <= 2) {
        y -= 1;
        m += 12;
    }

    uint32_t days = 365UL * (y - 1970)
                  + (y - 1969) / 4
                  - (y - 1901) / 100
                  + (y - 1601) / 400
                  + (153 * (m - 3) + 2) / 5
                  + d - 1;

    uint32_t seconds =
        days * 86400UL +
        dt.hour * 3600UL +
        dt.minute * 60UL +
        dt.second;

    return seconds;
}
