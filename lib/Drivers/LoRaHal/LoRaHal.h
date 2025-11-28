// lib/Drivers/LoRaHal - CubeSat LoRa SX1276 HAL Driver
// Status: STUB - Implementation pending

#pragma once

#include "HAL/interface/SPI.h"
#include "HAL/interface/GPIO.h"

namespace Drivers {
    class LoRaHal {
    public:
        LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio);
        bool begin(uint32_t frequency);
        // ... métodos SX1276
    };
}