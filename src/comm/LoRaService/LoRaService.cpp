#include "LoRaService.h"
#include <SPI.h>

LoRaService::LoRaService() : _online(false) {}

bool LoRaService::begin() {
    DEBUG_PRINTLN("[LoRa] Inicializando...");
    
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRa] ERRO: Falha ao iniciar hardware.");
        _online = false;
        return false;
    }

    // Configurações Padrão
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    if (LORA_CRC_ENABLED) LoRa.enableCrc();

    DEBUG_PRINTLN("[LoRa] Online!");
    _online = true;
    
    // Inicia escuta
    LoRa.receive();
    
    return true;
}

bool LoRaService::send(const String& data) {
    if (!_online) return false;
    return _transmitter.send(data);
}

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    if (!_online) return false;
    return _receiver.receive(packet, rssi, snr);
}

void LoRaService::setSpreadingFactor(int sf) {
    if (_online) {
        _transmitter.setSpreadingFactor(sf);
    }
}