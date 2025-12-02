/**
 * @file LoRaReceiver.h
 * @brief Recepção LoRa com filtros de qualidade
 * @version 1.0.0
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class LoRaReceiver {
public:
    LoRaReceiver();

    /**
     * @brief Tenta receber um pacote LoRa
     * @param packet String para armazenar o payload recebido
     * @param rssi RSSI do pacote recebido
     * @param snr SNR do pacote recebido
     * @return true se recebeu um pacote válido
     */
    bool receive(String& packet, int& rssi, float& snr);

    /**
     * @brief Retorna o último RSSI válido medido
     */
    int getLastRSSI() const;

    /**
     * @brief Retorna o último SNR válido medido
     */
    float getLastSNR() const;

    /**
     * @brief Estatísticas de recepção
     */
    void getStatistics(uint16_t& received, uint16_t& rejected) const;

private:
    // Filtros de qualidade
    bool _isSignalQualityGood(int rssi, float snr) const;

    // Estado
    int      _lastRSSI;
    float    _lastSNR;
    uint16_t _packetsReceived;
    uint16_t _packetsRejected;
};
