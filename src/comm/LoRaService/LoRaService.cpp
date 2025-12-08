// ARQUIVO: src/comm/LoRaService/LoRaService.cpp

/**
 * @file LoRaService.cpp
 * @brief Implementação LoRa com SF Estático e Duty Cycle
 */

#include "LoRaService.h"
#include "comm/CryptoManager.h"
#include <SPI.h>

LoRaService::LoRaService() : 
    _online(false), 
    _currentSF(LORA_SPREADING_FACTOR) 
{}

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
    
    DEBUG_PRINTF("[LoRa] Online! Freq=%.1f MHz, SF=%d, BW=%.1f kHz, PWR=%d dBm\n",
                 LORA_FREQUENCY/1e6, LORA_SPREADING_FACTOR, 
                 LORA_SIGNAL_BANDWIDTH/1e3, LORA_TX_POWER);
    
    _online = true;
    _currentSF = LORA_SPREADING_FACTOR;
    
    // Inicia escuta
    LoRa.receive();
    
    return true;
}

// ============================================================================
// ENVIO COM DUTY CYCLE CHECK E CRIPTOGRAFIA
// ============================================================================
bool LoRaService::send(const uint8_t* data, size_t len, bool encrypt) {
    if (!_online) return false;
    
    // Buffer para dados (possivelmente criptografados)
    uint8_t txBuffer[256];
    size_t txLen = len;
    
    if (encrypt && CryptoManager::isEnabled()) {
        // Adicionar padding para completar blocos de 16 bytes
        uint8_t paddedData[272];  // len + até 16 bytes de padding
        txLen = CryptoManager::addPadding(data, len, paddedData);
        
        // Criptografar
        if (!CryptoManager::encrypt(paddedData, txLen, txBuffer)) {
            DEBUG_PRINTLN("[LoRa] ERRO: Falha na criptografia.");
            return false;
        }
        
        DEBUG_PRINTF("[LoRa] Dados criptografados: %d -> %d bytes\n", len, txLen);
    } else {
        // Sem criptografia, copiar dados direto
        memcpy(txBuffer, data, len);
    }
    
    // Calcular ToA e verificar duty cycle
    uint32_t toaMs = calculateToA(txLen, _currentSF);
    
    if (!_dutyCycleTracker.canTransmit(toaMs)) {
        uint32_t waitTime = _dutyCycleTracker.getTimeUntilAvailable(toaMs);
        DEBUG_PRINTF("[LoRa] Duty cycle excedido. Aguardar %lu ms (%.1f min)\n", 
                     waitTime, waitTime / 60000.0f);
        return false;
    }
    
    // Transmitir
    bool success = _transmitter.send(txBuffer, txLen);
    
    if (success) {
        _dutyCycleTracker.recordTransmission(toaMs);
        DEBUG_PRINTF("[LoRa] TX OK: %d bytes, ToA=%lu ms, DC=%.1f%%\n", 
                     txLen, toaMs, _dutyCycleTracker.getDutyCyclePercent());
    }
    
    return success;
}

// Envio String (legado)
bool LoRaService::send(const String& data) {
    return send((const uint8_t*)data.c_str(), data.length(), false);
}

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    if (!_online) return false;
    return _receiver.receive(packet, rssi, snr);
}

void LoRaService::setSpreadingFactor(int sf) {
    if (_online && sf >= 7 && sf <= 12) {
        _transmitter.setSpreadingFactor(sf);
        _currentSF = sf;
        
        DEBUG_PRINTF("[LoRa] SF alterado para %d", sf);
        if (sf >= 11) {
            DEBUG_PRINTF(" (LDRO auto-habilitado)");
        }
        DEBUG_PRINTLN("");
    }
}

void LoRaService::setTxPower(int level) {
    if (_online) {
        int pwr = constrain(level, 2, 20);
        LoRa.setTxPower(pwr);
        DEBUG_PRINTF("[LoRa] Potência: %d dBm\n", pwr);
    }
}

// ============================================================================
// DUTY CYCLE HELPERS
// ============================================================================
bool LoRaService::canTransmitNow(uint32_t payloadSize) {
    uint32_t toaMs = calculateToA(payloadSize, _currentSF);
    return _dutyCycleTracker.canTransmit(toaMs);
}

uint32_t LoRaService::calculateToA(uint32_t payloadSize, int sf) {
    // Usar SF atual se não especificado
    if (sf < 0) sf = _currentSF;
    
    // Fórmula simplificada de Time on Air (ToA)
    uint32_t symbolDuration = (1 << sf);  // 2^SF
    uint32_t bwKhz = LORA_SIGNAL_BANDWIDTH / 1000;
    
    // ToA básico (ms)
    uint32_t toaMs = (payloadSize * 8 * symbolDuration) / bwKhz;
    
    // Adicionar overhead de preamble (~8 símbolos)
    uint32_t preambleMs = (LORA_PREAMBLE_LENGTH * symbolDuration * 1000) / LORA_SIGNAL_BANDWIDTH;
    
    uint32_t totalToA = toaMs + preambleMs;
    
    // Margem de segurança de 10%
    totalToA = (totalToA * 110) / 100;
    
    return totalToA;
}