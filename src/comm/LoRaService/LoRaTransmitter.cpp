#include "LoRaTransmitter.h"

LoRaTransmitter::LoRaTransmitter() : _currentSF(LORA_SPREADING_FACTOR) {}

void LoRaTransmitter::setSpreadingFactor(int sf) {
    _currentSF = sf;
    LoRa.setSpreadingFactor(sf);
}

bool LoRaTransmitter::send(const String& data) {
    const int maxRetries = 3;        // Tenta até 3 vezes
    // const int maxWaitTimeMs = 2000; // (Opcional) Timeout total
    
    for (int i = 0; i <= maxRetries; i++) {
        // 1. Verifica Canal (Listen Before Talk)
        if (_isChannelFree()) {
            // Canal livre! Transmite.
            LoRa.beginPacket();
            LoRa.print(data);
            LoRa.endPacket(); 
            LoRa.receive(); // Volta para RX imediatamente
            
            if (i > 0) {
                // Debug: Mostra que o CSMA atuou
                DEBUG_PRINTF("[LoRa] TX OK apos %d tentativas (CSMA/CA)\n", i);
            }
            return true;
        }
        
        // 2. Canal Ocupado - Backoff Aleatório
        if (i < maxRetries) {
            DEBUG_PRINTLN("[LoRa] Canal ocupado. Aguardando...");
            // Espera aleatória entre 100ms e 500ms para evitar colisão
            int wait = random(100, 500);
            delay(wait);
        }
    }

    DEBUG_PRINTLN("[LoRa] Falha TX: Canal congestionado.");
    return false;
}

bool LoRaTransmitter::_isChannelFree() {
    // RSSI ambiente. Ajuste o limiar conforme seu ambiente.
    // -90dBm é seguro para a maioria dos ambientes rurais/espaciais
    return (LoRa.rssi() < -90); 
}