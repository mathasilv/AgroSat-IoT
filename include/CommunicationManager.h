/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação DUAL MODE: LoRa + WiFi/HTTP
 * @version 4.0.0
 * @date 2025-11-11
 */

#ifndef COMMUNICATIONMANAGER_H
#define COMMUNICATIONMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "config.h"

class CommunicationManager {
public:
    CommunicationManager();
    
    bool begin();
    void update();
    
    // WiFi
    bool connectWiFi();
    void disconnectWiFi();
    bool isConnected();
    int8_t getRSSI();
    String getIPAddress();
    bool testConnection();
    
    // HTTP/OBSAT
    bool sendTelemetry(const TelemetryData& data);
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    
    // LoRa (NOVO - ÚNICO GERENCIADOR)
    bool initLoRa();
    bool sendLoRa(const String& data);
    bool sendLoRaTelemetry(const TelemetryData& data);
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);  // ADICIONADO
    bool isLoRaOnline();
    int getLoRaRSSI();
    float getLoRaSNR();
    void getLoRaStatistics(uint16_t& sent, uint16_t& failed);

private:
    // WiFi/HTTP
    bool _connected;
    int8_t _rssi;
    String _ipAddress;
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    unsigned long _lastConnectionAttempt;
    
    // LoRa
    bool _loraInitialized;
    int _loraRSSI;
    float _loraSNR;
    uint16_t _loraPacketsSent;
    uint16_t _loraPacketsFailed;
    unsigned long _lastLoRaTransmission;
    
    // Métodos privados
    String _createTelemetryJSON(const TelemetryData& data);
    String _createLoRaPayload(const TelemetryData& data);
    bool _sendHTTPPost(const String& jsonPayload);
    bool _waitForConnection(uint32_t timeoutMs);
};

#endif // COMMUNICATIONMANAGER_H
