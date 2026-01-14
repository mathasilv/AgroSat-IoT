/**
 * @file LoRaTransmitter.cpp
 * @brief Implementação do transmissor LoRa com retry e backoff
 */

#include "LoRaTransmitter.h"

LoRaTransmitter::LoRaTransmitter() : _currentSF(LORA_SPREADING_FACTOR) {}

void LoRaTransmitter::setSpreadingFactor(int sf) {
    _currentSF = sf;
    LoRa.setSpreadingFactor(sf);
}

// Envio Binário (Otimizado)
bool LoRaTransmitter::send(const uint8_t* data, size_t len) {
    const int maxRetries = 3;
    
    for (int i = 0; i <= maxRetries; i++) {
        if (_isChannelFree()) {
            LoRa.beginPacket();
            LoRa.write(data, len); // Envia bytes brutos
            LoRa.endPacket();
            LoRa.receive();
            return true;
        }
        // Backoff aleatório se canal ocupado
        if (i < maxRetries) delay(random(100, 500));
    }
    return false;
}

// Envio String (Compatibilidade)
bool LoRaTransmitter::send(const String& data) {
    return send((const uint8_t*)data.c_str(), data.length());
}

bool LoRaTransmitter::_isChannelFree() {
    // RSSI < -90dBm considerado livre
    return (LoRa.rssi() < -90);
}