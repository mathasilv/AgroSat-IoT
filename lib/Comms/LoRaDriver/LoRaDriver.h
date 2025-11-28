// lib/Comms/LoRaDriver/LoRaDriver.h - LoRa Operational Driver
// Interface operacional para CommunicationManager

#pragma once

#include "../../Drivers/LoRaHal/LoRaHal.h"

namespace Comms {
    class LoRaDriver {
    private:
        LoRaHal _loraHal;
    public:
        LoRaDriver(HAL::SPI* spi, HAL::GPIO* gpio);
        bool begin(uint32_t frequency);
        bool send(const String& data);
        bool receive(String& packet, int& rssi, float& snr);
        void setTxPower(int8_t power);
        // TODO: mais métodos
    };
}