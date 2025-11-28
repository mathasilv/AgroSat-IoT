// lib/Comms/CommsManager.h - Stub inicial para migração
#ifndef COMMS_MANAGER_H
#define COMMS_MANAGER_H

#include "HAL/interface/SPI.h"
#include "HAL/interface/GPIO.h"

namespace Comms {

class CommsManager {
private:
    HAL::SPI* _spi;
    HAL::GPIO* _gpio;

public:
    CommsManager(HAL::SPI* spi, HAL::GPIO* gpio);
    bool init();
};

} // namespace Comms

#endif