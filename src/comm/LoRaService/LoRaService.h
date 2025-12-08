/**
 * @file LoRaService.h
 * @brief Serviço LoRa com Adaptive SF, Duty Cycle e Criptografia
 * @version 3.0.0 (5.2 Adaptive SF + 4.8 Duty Cycle + 4.1 Crypto)
 */

#ifndef LORA_SERVICE_H
#define LORA_SERVICE_H

#include <Arduino.h>
#include "LoRaReceiver.h"
#include "LoRaTransmitter.h"
#include "DutyCycleTracker.h"
#include "config.h"

class LoRaService {
public:
    LoRaService();
    
    bool begin();
    
    // --- MÉTODOS DE ENVIO ---
    /**
     * @brief Envia dados binários com verificação de duty cycle
     * @param data Ponteiro para os dados
     * @param len Tamanho dos dados
     * @param encrypt Se true, criptografa antes de enviar (padrão: false)
     * @return true se enviou com sucesso
     */
    bool send(const uint8_t* data, size_t len, bool encrypt = false);
    
    /**
     * @brief Envia string (legado - compatibilidade)
     */
    bool send(const String& data);
    
    // --- RECEPÇÃO ---
    /**
     * @brief Recebe pacote LoRa
     * @param packet String de saída com dados
     * @param rssi RSSI do pacote recebido
     * @param snr SNR do pacote recebido
     * @return true se recebeu pacote
     */
    bool receive(String& packet, int& rssi, float& snr);
    
    // --- CONTROLE DE PARÂMETROS ---
    bool isOnline() const { return _online; }
    
    /**
     * @brief Define Spreading Factor manualmente
     */
    void setSpreadingFactor(int sf);
    
    /**
     * @brief Define potência de transmissão (2-20 dBm)
     */
    void setTxPower(int level);
    
    // --- NOVO 5.2: ADAPTIVE SPREADING FACTOR ---
    /**
     * @brief Ajusta SF dinamicamente baseado na qualidade do link
     * @param rssi RSSI atual (dBm)
     * @param snr SNR atual (dB)
     */
    void adjustSFBasedOnLinkQuality(int rssi, float snr);
    
    /**
     * @brief Ajusta SF baseado na distância calculada
     * @param distanceKm Distância em km
     */
    void adjustSFBasedOnDistance(float distanceKm);
    
    // --- NOVO 4.8: DUTY CYCLE ---
    /**
     * @brief Verifica se pode transmitir agora (sem violar duty cycle)
     * @param payloadSize Tamanho do payload em bytes
     * @return true se pode transmitir
     */
    bool canTransmitNow(uint32_t payloadSize);
    
    /**
     * @brief Retorna referência ao rastreador de duty cycle
     */
    DutyCycleTracker& getDutyCycleTracker() { return _dutyCycleTracker; }
    
    /**
     * @brief Retorna tempo estimado de transmissão (Time on Air)
     * @param payloadSize Tamanho do payload em bytes
     * @param sf Spreading Factor (usa atual se não especificado)
     * @return ToA em milissegundos
     */
    uint32_t calculateToA(uint32_t payloadSize, int sf = -1);
    
    // --- ESTATÍSTICAS ---
    int getCurrentSF() const { return _currentSF; }
    int getLastRSSI() const { return _receiver.getLastRSSI(); }
    float getLastSNR() const { return _receiver.getLastSNR(); }

private:
    LoRaReceiver _receiver;
    LoRaTransmitter _transmitter;
    DutyCycleTracker _dutyCycleTracker;  // NOVO 4.8
    
    bool _online;
    int _currentSF;  // SF atual em uso
    
    // Helpers internos
    bool _sendWithDutyCycleCheck(const uint8_t* data, size_t len);
};

#endif