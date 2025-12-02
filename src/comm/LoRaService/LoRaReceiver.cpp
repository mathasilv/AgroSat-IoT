/**
 * @file LoRaReceiver.cpp
 * @brief Implementação da recepção LoRa
 */

#include "LoRaReceiver.h"

LoRaReceiver::LoRaReceiver() :
    _lastRSSI(0),
    _lastSNR(0.0f),
    _packetsReceived(0),
    _packetsRejected(0)
{}

bool LoRaReceiver::receive(String& packet, int& rssi, float& snr) {
    // Verificar se há pacote disponível
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        return false;
    }

    // Ler payload
    packet = "";
    while (LoRa.available()) {
        packet += char(LoRa.read());
    }

    // Obter métricas
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();

    // Filtrar por qualidade de sinal
    if (!_isSignalQualityGood(rssi, snr)) {
        _packetsRejected++;
        return false;
    }

    // Pacote válido
    _lastRSSI = rssi;
    _lastSNR = snr;
    _packetsReceived++;

    DEBUG_PRINTLN("[LoRaReceiver] ━━━━━ PACOTE RECEBIDO ━━━━━");
    DEBUG_PRINTF("[LoRaReceiver] Dados: %s\n", packet.c_str());
    DEBUG_PRINTF("[LoRaReceiver] RSSI: %d dBm\n", rssi);
    DEBUG_PRINTF("[LoRaReceiver] SNR: %.1f dB\n", snr);
    DEBUG_PRINTF("[LoRaReceiver] Tamanho: %d bytes\n", packet.length());
    DEBUG_PRINTF("[LoRaReceiver] Total recebido: %d\n", _packetsReceived);
    DEBUG_PRINTLN("[LoRaReceiver] ━━━━━━━━━━━━━━━━━━━━━━━━━");

    return true;
}

int LoRaReceiver::getLastRSSI() const {
    return _lastRSSI;
}

float LoRaReceiver::getLastSNR() const {
    return _lastSNR;
}

void LoRaReceiver::getStatistics(uint16_t& received, uint16_t& rejected) const {
    received = _packetsReceived;
    rejected = _packetsRejected;
}

// ========================================
// MÉTODOS PRIVADOS
// ========================================

bool LoRaReceiver::_isSignalQualityGood(int rssi, float snr) const {
    const int MIN_RSSI = -120;
    const float MIN_SNR = -15.0f;

    if (rssi < MIN_RSSI) {
        DEBUG_PRINTF("[LoRaReceiver] Pacote descartado: RSSI=%d dBm (< %d dBm)\n",
                     rssi, MIN_RSSI);
        return false;
    }

    if (snr < MIN_SNR) {
        DEBUG_PRINTF("[LoRaReceiver] Pacote descartado: SNR=%.1f dB (< %.1f dB)\n",
                     snr, MIN_SNR);
        return false;
    }

    return true;
}
