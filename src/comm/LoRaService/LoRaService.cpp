/**
 * @file LoRaService.cpp
 * @brief Implementação LoRa Limpa
 */

#include "LoRaService.h"
#include "Globals.h" 

volatile int LoRaService::_rxPacketSize = 0;

// Função auxiliar Time-on-Air (Mantida, pois é crítica para o Duty Cycle)
uint32_t calculateTimeOnAir(int bytes, int sf) {
    float ts = pow(2, sf) / (LORA_SIGNAL_BANDWIDTH); 
    float tPreamble = (LORA_PREAMBLE_LENGTH + 4.25f) * ts;
    float payloadSymb = 8 + max(ceil((8.0f*bytes - 4.0f*sf + 28.0f + 16.0f)/(4.0f*sf)) * (LORA_CODING_RATE), 0.0f);
    float tPayload = payloadSymb * ts;
    return (uint32_t)((tPreamble + tPayload) * 1000);
}

LoRaService::LoRaService() : _currentSF(LORA_SPREADING_FACTOR), _lastRSSI(0), _lastSNR(0) {}

bool LoRaService::begin() {
    DEBUG_PRINTLN("[LoRa] Inicializando Hardware (Interrupt Mode)...");
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRa] ERRO: Falha ao iniciar SX1276!");
        return false;
    }
    
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    if (LORA_CRC_ENABLED) LoRa.enableCrc();
    
    LoRa.onReceive(LoRaService::onDio0Rise);
    LoRa.receive();
    
    DEBUG_PRINTF("[LoRa] Online! Freq=%.1f MHz, SF=%d\n", LORA_FREQUENCY/1E6, _currentSF);
    return true;
}

void IRAM_ATTR LoRaService::onDio0Rise(int packetSize) {
    _rxPacketSize = packetSize;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xLoRaRxSemaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    if (xSemaphoreTake(xLoRaRxSemaphore, 0) == pdTRUE) {
        if (_rxPacketSize == 0) return false; 
        packet = "";
        while (LoRa.available()) packet += (char)LoRa.read();
        rssi = LoRa.packetRssi();
        snr = LoRa.packetSnr();
        _lastRSSI = rssi;
        _lastSNR = snr;
        _rxPacketSize = 0;
        return true;
    }
    return false;
}

// Implementação apenas do método Binário
bool LoRaService::send(const uint8_t* data, size_t len, bool isAsync) {
    LoRa.beginPacket();
    LoRa.write(data, len);
    LoRa.endPacket(isAsync); 
    
    uint32_t airTime = calculateTimeOnAir(len, _currentSF);
    _dutyCycle.recordTransmission(airTime);
    
    if (!isAsync) LoRa.receive();
    return true;
}

void LoRaService::setTxPower(int level) { LoRa.setTxPower(level); }
void LoRaService::setSpreadingFactor(int sf) {
    _currentSF = sf;
    LoRa.setSpreadingFactor(sf);
}
bool LoRaService::canTransmitNow(uint32_t payloadSize) {
    uint32_t airTime = calculateTimeOnAir(payloadSize, _currentSF);
    return _dutyCycle.canTransmit(airTime);
}