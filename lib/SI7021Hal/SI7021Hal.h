#ifndef SI7021_HAL_H
#define SI7021_HAL_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "config.h"

// Driver mínimo do SI7021 usando HAL::I2C (sem Wire direto)
// - Umidade em %RH
// - Temperatura em °C (fórmulas do datasheet) [web:107][web:113]
class SI7021Hal {
public:
    explicit SI7021Hal(HAL::I2C& i2c);

    // Inicializa o sensor no endereço dado (padrão: SI7021_ADDRESS de config.h)
    bool begin(uint8_t addr = SI7021_ADDRESS);

    // Leitura de alta-nível
    bool readHumidity(float& humidity);     // %RH
    bool readTemperature(float& temperature); // °C

    bool isInitialized() const { return _initialized; }

private:
    HAL::I2C* _i2c;
    uint8_t   _addr;
    bool      _initialized;

    bool _writeCommand(uint8_t cmd);
    bool _readBytes(uint8_t* buf, size_t len);
};

#endif // SI7021_HAL_H
