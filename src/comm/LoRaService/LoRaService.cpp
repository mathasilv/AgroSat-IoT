// ARQUIVO: src/comm/LoRaService/LoRaService.cpp

/**
 * @file LoRaService.cpp
 * @brief Implementação LoRa com Adaptive SF e Duty Cycle
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
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);  // LDRO gerenciado automaticamente aqui
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    if (LORA_CRC_ENABLED) LoRa.enableCrc();
    
    // NOTA: LDRO é habilitado automaticamente pela biblioteca quando SF >= 11
    // Não é necessário chamar setLdoFlag() manualmente (método privado)

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
// ENVIO COM DUTY CYCLE CHECK E CRIPTOGRAFIA (4.1 + 4.8)
// ============================================================================
bool LoRaService::send(const uint8_t* data, size_t len, bool encrypt) {
    if (!_online) return false;
    
    // Buffer para dados (possivelmente criptografados)
    uint8_t txBuffer[256];
    size_t txLen = len;
    
    // NOVO 4.1: Criptografar se solicitado
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
    
    // NOVO 4.8: Calcular ToA e verificar duty cycle
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
        
        // LDRO é gerenciado automaticamente pela biblioteca ao definir SF
        // Não é necessário chamar setLdoFlag() (método privado)
        
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
// NOVO 5.2: ADAPTIVE SPREADING FACTOR
// ============================================================================
void LoRaService::adjustSFBasedOnLinkQuality(int rssi, float snr) {
    int newSF = _currentSF;
    
    // Tabela de decisão baseada em RSSI e SNR
    if (rssi < -120 || snr < -10) {
        newSF = LORA_SPREADING_FACTOR_SAFE;  // SF12 (mais confiável)
        DEBUG_PRINTF("[LoRa] Link CRÍTICO (RSSI=%d, SNR=%.1f) -> SF12\n", rssi, snr);
    } 
    else if (rssi < -115 || snr < -5) {
        newSF = 11;
        DEBUG_PRINTF("[LoRa] Link RUIM (RSSI=%d, SNR=%.1f) -> SF11\n", rssi, snr);
    } 
    else if (rssi < -110 || snr < 0) {
        newSF = 10;
        DEBUG_PRINTF("[LoRa] Link MODERADO (RSSI=%d, SNR=%.1f) -> SF10\n", rssi, snr);
    } 
    else if (rssi < -105 && snr < 5) {
        newSF = 9;
        DEBUG_PRINTF("[LoRa] Link BOM (RSSI=%d, SNR=%.1f) -> SF9\n", rssi, snr);
    } 
    else if (rssi > -100 && snr > 5) {
        newSF = 7;  // SF7 (mais rápido, menos ToA)
        DEBUG_PRINTF("[LoRa] Link EXCELENTE (RSSI=%d, SNR=%.1f) -> SF7\n", rssi, snr);
    }
    
    // Aplicar mudança apenas se diferente do atual
    if (newSF != _currentSF) {
        setSpreadingFactor(newSF);
    }
}

void LoRaService::adjustSFBasedOnDistance(float distanceKm) {
    int newSF = 7;
    
    // Tabela empírica baseada em testes LoRa-Satélite
    if (distanceKm < 500.0f) {
        newSF = 7;
    } else if (distanceKm < 800.0f) {
        newSF = 8;
    } else if (distanceKm < 1100.0f) {
        newSF = 9;
    } else if (distanceKm < 1400.0f) {
        newSF = 10;
    } else if (distanceKm < 1800.0f) {
        newSF = 11;
    } else {
        newSF = 12;
    }
    
    DEBUG_PRINTF("[LoRa] Distância %.1f km -> SF%d\n", distanceKm, newSF);
    
    if (newSF != _currentSF) {
        setSpreadingFactor(newSF);
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
    // ToA ≈ (Preamble + Payload) * (2^SF / BW)
    // Simplificação: ToA_ms ≈ payloadBytes * 8 * (2^SF) / (BW/1000)
    
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
