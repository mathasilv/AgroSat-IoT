// lib/Drivers/LoRaHal/LoRaHal.h - SX1276 HAL Driver
// CubeSat LoRa driver sobre HAL SPI/GPIO

#pragma once

#include "../../HAL/interface/SPI.h"
#include "../../HAL/interface/GPIO.h"

#define LORA_DEFAULT_SS 18
#define LORA_DEFAULT_RESET 14
#define LORA_DEFAULT_DIO0 26

class LoRaHal {
private:
    HAL::SPI* _spi;
    HAL::GPIO* _gpio;
    uint8_t _cs, _rst, _dio0;
public:
    LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio, uint8_t cs, uint8_t rst, uint8_t dio0);
    bool begin(uint32_t frequency);
    void setTxPower(int8_t power);
    void setSpreadingFactor(uint8_t sf);
    // ... TODO: implementar métodos completos
};