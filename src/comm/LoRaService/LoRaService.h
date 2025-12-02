/**
 * @file LoRaService.h
 * @brief Serviço de rádio LoRa (orquestrador)
 * @version 2.0.0
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include "LoRaTransmitter.h"
#include "LoRaReceiver.h"

class LoRaService {
public:
    LoRaService();

    bool begin();
    bool init();
    bool retryInit(uint8_t maxAttempts = 3);

    void enable(bool enable);
    bool send(const String& data);
    bool receive(String& packet, int& rssi, float& snr);

    bool isOnline() const;
    int getLastRSSI() const;
    float getLastSNR() const;
    void getStatistics(uint16_t& sent, uint16_t& failed) const;

    void reconfigure(OperationMode mode);
    void adaptSpreadingFactor(float altitude);

private:
    void _configureParameters();

    // Componentes
    LoRaTransmitter _transmitter;
    LoRaReceiver    _receiver;

    // Estado
    bool    _initialized;
    bool    _enabled;
    uint8_t _currentSpreadingFactor;
};
