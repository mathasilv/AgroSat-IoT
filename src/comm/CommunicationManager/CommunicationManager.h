#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "config.h"

// Inclusão dos Serviços
#include "comm/LoRaService/LoRaService.h"
#include "comm/WiFiService/WiFiService.h"
#include "comm/HttpService/HttpService.h"

// Inclusão CRÍTICA do PayloadManager
// Certifique-se de que o arquivo existe em src/comm/PayloadManager/PayloadManager.h
#include "comm/PayloadManager/PayloadManager.h"

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
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    
    // Missão
    bool processLoRaPacket(const String& packet, MissionData& data);
    bool sendTelemetry(const TelemetryData& tData, const GroundNodeBuffer& gBuffer);
    
    // Helpers
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    uint8_t calculatePriority(const MissionData& node);

private:
    LoRaService _lora;
    WiFiService _wifi;
    HttpService _http;
    
    // A classe PayloadManager deve estar definida pelo include acima
    PayloadManager _payload;

    bool _loraEnabled;
    bool _httpEnabled;
};

#endif