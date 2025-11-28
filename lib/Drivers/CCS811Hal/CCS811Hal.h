#ifndef CCS811_HAL_H
#define CCS811_HAL_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "config.h"

// Driver mínimo do CCS811 usando HAL::I2C (sem Adafruit_CCS811/Wire)
// - Emula a interface principal da Adafruit_CCS811:
//   begin(), available(), readData(), geteCO2(), getTVOC(), setEnvironmentalData() [web:203][web:108]
class CCS811Hal {
public:
    explicit CCS811Hal(HAL::I2C& i2c);

    bool begin(uint8_t addr = CCS811_ADDR_1);

    bool available();           // DATA_READY em STATUS [web:108]
    uint8_t readData();         // 0 = OK; !=0 erro
    uint16_t geteCO2() const { return _eco2; }
    uint16_t getTVOC() const { return _tvoc; }

    void setEnvironmentalData(float humidity, float temperature); // %RH, °C [web:245][web:246]

    bool isInitialized() const { return _initialized; }

private:
    HAL::I2C* _i2c;
    uint8_t   _addr;
    bool      _initialized;

    uint16_t  _eco2;
    uint16_t  _tvoc;

    // Helpers de I2C
    bool _write8(uint8_t reg, uint8_t value);
    bool _write(uint8_t reg, const uint8_t* data, size_t len);
    bool _read(uint8_t reg, uint8_t* data, size_t len);
    bool _writeCommand(uint8_t cmd);
};

#endif // CCS811_HAL_H
