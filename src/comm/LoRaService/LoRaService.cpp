/**
 * @file LoRaService.cpp
 * @brief Implementação do serviço LoRa com proteção contra race conditions
 * 
 * @details Utiliza spinlock (portMUX) para proteção de variáveis voláteis
 *          compartilhadas entre ISR e tasks. Implementa recepção por
 *          interrupção com semáforo FreeRTOS para sincronização.
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 */

#include "LoRaService.h"
#include "Globals.h" 

//=============================================================================
// VARIÁVEIS ESTÁTICAS
//=============================================================================

volatile int LoRaService::_rxPacketSize = 0;

/// Spinlock para proteção da variável volátil entre ISR e task
static portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

//=============================================================================
// FUNÇÕES AUXILIARES
//=============================================================================

/**
 * @brief Calcula Time-on-Air para um pacote LoRa
 * 
 * @param bytes Tamanho do payload em bytes
 * @param sf Spreading Factor (7-12)
 * @return Tempo de transmissão em milissegundos
 * 
 * @details Fórmula baseada na especificação LoRa:
 *          - Tempo de símbolo: Ts = 2^SF / BW
 *          - Tempo de preâmbulo: (preambleLen + 4.25) * Ts
 *          - Símbolos de payload: 8 + max(ceil((8*PL - 4*SF + 28 + 16) / (4*SF)) * CR, 0)
 * 
 * @note Crítico para cálculo de duty cycle (limite 1%)
 */
uint32_t calculateTimeOnAir(int bytes, int sf) {
    float ts = pow(2, sf) / (LORA_SIGNAL_BANDWIDTH); 
    float tPreamble = (LORA_PREAMBLE_LENGTH + 4.25f) * ts;
    float payloadSymb = 8 + max(ceil((8.0f*bytes - 4.0f*sf + 28.0f + 16.0f)/(4.0f*sf)) * (LORA_CODING_RATE), 0.0f);
    float tPayload = payloadSymb * ts;
    return (uint32_t)((tPreamble + tPayload) * 1000);
}

//=============================================================================
// CONSTRUTOR E INICIALIZAÇÃO
//=============================================================================

LoRaService::LoRaService() 
    : _currentSF(LORA_SPREADING_FACTOR)
    , _lastRSSI(0)
    , _lastSNR(0) 
{}

bool LoRaService::begin() {
    DEBUG_PRINTLN("[LoRa] Inicializando Hardware (Interrupt Mode)...");
    
    // Configurar pinos SPI do módulo LoRa
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

//=============================================================================
// INTERRUPÇÃO DE RECEPÇÃO
//=============================================================================

/**
 * @brief ISR chamada quando DIO0 sinaliza pacote recebido
 * @note IRAM_ATTR garante execução da RAM para menor latência
 */
void IRAM_ATTR LoRaService::onDio0Rise(int packetSize) {
    // Proteção crítica para variável compartilhada
    portENTER_CRITICAL_ISR(&rxMux);
    _rxPacketSize = packetSize;
    portEXIT_CRITICAL_ISR(&rxMux);
    
    // Sinaliza task via semáforo (FreeRTOS safe)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xLoRaRxSemaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

//=============================================================================
// RECEPÇÃO
//=============================================================================

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    // Tenta adquirir semáforo (non-blocking)
    if (xSemaphoreTake(xLoRaRxSemaphore, 0) == pdTRUE) {
        // Copia atomicamente o tamanho do pacote (proteção contra ISR)
        portENTER_CRITICAL(&rxMux);
        int packetSize = _rxPacketSize;
        _rxPacketSize = 0;
        portEXIT_CRITICAL(&rxMux);
        
        if (packetSize == 0) return false; 
        
        packet = "";
        while (LoRa.available()) packet += (char)LoRa.read();
        rssi = LoRa.packetRssi();
        snr = LoRa.packetSnr();
        _lastRSSI = rssi;
        _lastSNR = snr;
        return true;
    }
    return false;
}

//=============================================================================
// TRANSMISSÃO
//=============================================================================

bool LoRaService::send(const uint8_t* data, size_t len, bool isAsync) {
    LoRa.beginPacket();
    LoRa.write(data, len);
    LoRa.endPacket(isAsync); 
    
    uint32_t airTime = calculateTimeOnAir(len, _currentSF);
    _dutyCycle.recordTransmission(airTime);
    
    // Retorna ao modo recepção após TX síncrono
    if (!isAsync) LoRa.receive();
    return true;
}

//=============================================================================
// CONFIGURAÇÃO
//=============================================================================

void LoRaService::setTxPower(int level) { LoRa.setTxPower(level); }

//=============================================================================
// CONTROLE DE DUTY CYCLE
//=============================================================================

bool LoRaService::canTransmitNow(uint32_t payloadSize) {
    uint32_t airTime = calculateTimeOnAir(payloadSize, _currentSF);
    return _dutyCycle.canTransmit(airTime);
}
