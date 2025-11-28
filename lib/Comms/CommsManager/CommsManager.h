// lib/Comms/CommsManager/CommsManager.h - CubeSat Communications Orchestrator
// Interface principal mantendo compatibilidade atual

#pragma once

#include "LoRaDriver/LoRaDriver.h"
#include "../../HAL/interface/SPI.h"
#include "../../HAL/interface/GPIO.h"

namespace Comms {
    class CommsManager {
    private:
        LoRaDriver _loraDriver;
        HAL::SPI* _spi;
        HAL::GPIO* _gpio;
        
    public:
        CommsManager(HAL::SPI* spi, HAL::GPIO* gpio);
        bool begin();
        bool sendLoRa(const String& data);
        bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
        bool sendTelemetry(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
        // Mantém TODA interface atual do CommunicationManager
    };
}