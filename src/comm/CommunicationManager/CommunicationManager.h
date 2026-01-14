/**
 * @file CommunicationManager.h
 * @brief Gerenciador de Comunicação (CLEANED)
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "config.h"
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
    bool sendLoRa(const uint8_t* data, size_t len); 
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    
    // Missão
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // Telemetria
    bool sendTelemetry(const TelemetryData& tData, GroundNodeBuffer& gBuffer);

    // Processa o pacote da fila
    void processHttpQueuePacket(const HttpQueueMessage& packet);
    
    // Helpers
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    
    // Duty Cycle (usado pelo comando DUTY_CYCLE)
    DutyCycleTracker& getDutyCycleTracker() { 
        return _lora.getDutyCycleTracker(); 
    }

private:
    LoRaService _lora;
    WiFiService _wifi;
    HttpService _http;
    PayloadManager _payload;

    bool _loraEnabled;
    bool _httpEnabled;
};

#endif
