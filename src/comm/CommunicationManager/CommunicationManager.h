/**
 * @file CommunicationManager.h
 * @brief Gerenciador de Comunicação (FASE 2: Suporte a HTTP Async)
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "config.h"

// Includes dos Serviços
#include "comm/LoRaService/LoRaService.h"
#include "comm/WiFiService/WiFiService.h"
#include "comm/HttpService/HttpService.h"
#include "comm/PayloadManager/PayloadManager.h"
#include "comm/LoRaService/DutyCycleTracker.h"

class CommunicationManager {
public:
    CommunicationManager();

    bool begin();
    void update();

    // WiFi & HTTP
    bool isWiFiConnected();
    void connectWiFi(); 
    
    // LoRa
    bool sendLoRa(const String& data);
    bool sendLoRa(const uint8_t* data, size_t len); 
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    
    // Missão
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // Telemetria (Agora coloca na fila se HTTP estiver ativo)
    bool sendTelemetry(const TelemetryData& tData, const GroundNodeBuffer& gBuffer);

    // NOVO (Fase 2): Processa o pacote da fila (Chamado pela Task)
    void processHttpQueuePacket(const HttpQueueMessage& packet);
    
    // Helpers
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    
    // Controle de Spreading Factor
    void setSpreadingFactor(int sf) { _lora.setSpreadingFactor(sf); }
    int getCurrentSF() const { return _lora.getCurrentSF(); }
    
    DutyCycleTracker& getDutyCycleTracker() { 
        return _lora.getDutyCycleTracker(); 
    }
    
    bool canTransmitNow(uint32_t payloadSize) {
        return _lora.canTransmitNow(payloadSize);
    }
    
    int getLastRSSI() const { return _lora.getLastRSSI(); }
    float getLastSNR() const { return _lora.getLastSNR(); }

private:
    LoRaService _lora;
    WiFiService _wifi;
    HttpService _http;
    PayloadManager _payload;

    bool _loraEnabled;
    bool _httpEnabled;
};

#endif