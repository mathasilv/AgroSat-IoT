#include "LoRaTransmitter.h"

LoRaTransmitter::LoRaTransmitter() : _currentSF(LORA_SPREADING_FACTOR) {}

void LoRaTransmitter::setSpreadingFactor(int sf) {
    _currentSF = sf;
    LoRa.setSpreadingFactor(sf);
}

bool LoRaTransmitter::send(const String& data) {
    // CAD (Channel Activity Detection) simples
    // Se o RSSI atual for alto, o canal está ocupado
    if (!_isChannelFree()) {
        DEBUG_PRINTLN("[LoRa] Canal ocupado. Abortando TX.");
        return false;
    }

    LoRa.beginPacket();
    LoRa.print(data);
    LoRa.endPacket(); // Bloqueante, mas rápido o suficiente para nossos payloads
    
    // Volta para modo de recepção imediatamente
    LoRa.receive();
    
    return true;
}

bool LoRaTransmitter::_isChannelFree() {
    // Verifica ruído no canal antes de transmitir
    int rssi = LoRa.rssi();
    return (rssi < -95); // Limiar de canal livre (ajuste conforme ambiente)
}