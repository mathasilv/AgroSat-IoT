/**
 * @file LoRaTransmitter.h
 * @brief Transmissão LoRa com CAD, duty cycle e backoff
 * @version 1.0.0
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class LoRaTransmitter {
public:
    LoRaTransmitter();

    /**
     * @brief Envia payload LoRa com validações completas
     * @param data Payload em String (hex ou texto)
     * @param currentSF Spreading Factor atual
     * @return true se transmitiu com sucesso
     */
    bool send(const String& data, uint8_t currentSF);

    /**
     * @brief Adapta Spreading Factor baseado na altitude
     * @param altitude Altitude em metros
     * @param currentSF Referência para SF atual (será modificado)
     */
    void adaptSpreadingFactor(float altitude, uint8_t& currentSF);

    // Estatísticas
    void getStatistics(uint16_t& sent, uint16_t& failed) const;
    uint8_t getFailureCount() const;
    unsigned long getLastTxTime() const;

private:
    // Validações
    bool _validatePayloadSize(size_t size) const;
    bool _isChannelFree() const;
    bool _checkDutyCycle();

    // TX interno
    bool _transmit(const String& data, uint32_t timeout);

    // Estado
    uint16_t      _packetsSent;
    uint16_t      _packetsFailed;
    unsigned long _lastTx;
    uint8_t       _txFailureCount;
    unsigned long _lastTxFailure;
};
