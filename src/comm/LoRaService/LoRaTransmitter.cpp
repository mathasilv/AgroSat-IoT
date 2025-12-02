/**
 * @file LoRaTransmitter.cpp
 * @brief Implementação da transmissão LoRa
 */

#include "LoRaTransmitter.h"

LoRaTransmitter::LoRaTransmitter() :
    _packetsSent(0),
    _packetsFailed(0),
    _lastTx(0),
    _txFailureCount(0),
    _lastTxFailure(0)
{}

bool LoRaTransmitter::send(const String& data, uint8_t currentSF) {
    // Validar tamanho
    if (!_validatePayloadSize(data.length())) {
        DEBUG_PRINTF("[LoRaTransmitter] Payload muito grande: %d bytes (max %d)\n",
                     data.length(), LORA_MAX_PAYLOAD_SIZE);
        _packetsFailed++;
        return false;
    }

    // Verificar duty cycle
    if (!_checkDutyCycle()) {
        return false;
    }

    // CAD - Channel Activity Detection
    if (!_isChannelFree()) {
        DEBUG_PRINTLN("[LoRaTransmitter] TX adiado: canal ocupado");
        return false;
    }

    // Determinar timeout baseado no SF
    uint32_t txTimeout = (currentSF >= 11) 
        ? LORA_TX_TIMEOUT_MS_SAFE 
        : LORA_TX_TIMEOUT_MS_NORMAL;

    DEBUG_PRINTLN("[LoRaTransmitter] ━━━━━ TRANSMITINDO ━━━━━");
    DEBUG_PRINTF("[LoRaTransmitter] Payload: %s\n", data.c_str());
    DEBUG_PRINTF("[LoRaTransmitter] Tamanho: %d bytes\n", data.length());
    DEBUG_PRINTF("[LoRaTransmitter] SF: %d, Timeout: %lu ms\n", currentSF, txTimeout);

    // Transmitir
    bool success = _transmit(data, txTimeout);

    if (success) {
        _packetsSent++;
        _lastTx = millis();
        _txFailureCount = 0;
        DEBUG_PRINTF("[LoRaTransmitter] ✓ Pacote enviado! Total: %d\n", _packetsSent);
    } else {
        _packetsFailed++;
        _txFailureCount++;
        _lastTxFailure = millis();
        DEBUG_PRINTLN("[LoRaTransmitter] ✗ Falha na transmissão");

        // Backoff exponencial
        uint32_t backoff = min<uint32_t>(1000U * (1U << _txFailureCount), 8000U);
        DEBUG_PRINTF("[LoRaTransmitter] Backoff: %lu ms\n", backoff);
        delay(backoff);
    }

    // Voltar para RX
    delay(10);
    LoRa.receive();
    DEBUG_PRINTLN("[LoRaTransmitter] LoRa voltou ao modo RX");

    return success;
}

void LoRaTransmitter::adaptSpreadingFactor(float altitude, uint8_t& currentSF) {
    if (isnan(altitude)) {
        return;
    }

    uint8_t newSF = currentSF;

    // Lógica de adaptação
    if (altitude < 10000.0f)       newSF = 7;
    else if (altitude < 20000.0f)  newSF = 8;
    else if (altitude < 30000.0f)  newSF = 9;
    else                           newSF = 10;

    if (newSF != currentSF) {
        DEBUG_PRINTF("[LoRaTransmitter] Ajustando SF: %d → %d (alt=%.0fm)\n",
                     currentSF, newSF, altitude);
        
        LoRa.setSpreadingFactor(newSF);
        currentSF = newSF;
        
        delay(10);
        LoRa.receive();
    }
}

void LoRaTransmitter::getStatistics(uint16_t& sent, uint16_t& failed) const {
    sent = _packetsSent;
    failed = _packetsFailed;
}

uint8_t LoRaTransmitter::getFailureCount() const {
    return _txFailureCount;
}

unsigned long LoRaTransmitter::getLastTxTime() const {
    return _lastTx;
}

// ========================================
// MÉTODOS PRIVADOS
// ========================================

bool LoRaTransmitter::_validatePayloadSize(size_t size) const {
    return size > 0 && size <= LORA_MAX_PAYLOAD_SIZE;
}

bool LoRaTransmitter::_isChannelFree() const {
    const int RSSI_THRESHOLD = -90;
    const uint8_t CAD_CHECKS = 3;

    for (uint8_t i = 0; i < CAD_CHECKS; i++) {
        int rssi = LoRa.rssi();
        
        DEBUG_PRINTF("[LoRaTransmitter] CAD %d: RSSI=%d dBm\n", i + 1, rssi);
        
        if (rssi > RSSI_THRESHOLD) {
            DEBUG_PRINTF("[LoRaTransmitter] Canal ocupado (RSSI=%d dBm)\n", rssi);
            delay(random(50, 200)); // backoff aleatório
            return false;
        }
        delay(10);
    }
    
    DEBUG_PRINTLN("[LoRaTransmitter] Canal LIVRE!");
    return true;
}

bool LoRaTransmitter::_checkDutyCycle() {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastTx;

    if (elapsed < LORA_MIN_INTERVAL_MS) {
        uint32_t waitTime = LORA_MIN_INTERVAL_MS - elapsed;
        
        DEBUG_PRINTF("[LoRaTransmitter] Aguardando duty cycle: %lu ms\n", waitTime);
        
        // Proteção contra espera absurda
        if (waitTime > 30000) {
            DEBUG_PRINTLN("[LoRaTransmitter] ERRO: Duty cycle muito longo");
            return false;
        }
        
        delay(waitTime);
    }
    
    return true;
}

bool LoRaTransmitter::_transmit(const String& data, uint32_t timeout) {
    unsigned long txStart = millis();
    
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = (LoRa.endPacket(true) == 1);
    
    unsigned long txDuration = millis() - txStart;

    // Verificar timeout
    if (txDuration > timeout) {
        DEBUG_PRINTF("[LoRaTransmitter] TIMEOUT: %lu ms > %lu ms\n",
                     txDuration, timeout);
        LoRa.receive();
        return false;
    }

    DEBUG_PRINTF("[LoRaTransmitter] TX duração: %lu ms\n", txDuration);
    
    return success;
}
