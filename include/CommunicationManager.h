/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação DUAL: LoRa + WiFi/HTTP
 * @version 3.1.0
 * @date 2025-11-10
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LoRa.h>
#include <SPI.h>
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
    
    // LoRa
    bool initLoRa();
    bool sendLoRa(const String& data);
    bool sendLoRaTelemetry(const TelemetryData& data);
    int getLoRaRSSI();
    float getLoRaSNR();
    bool isLoRaOnline();
    
    // Telemetria (WiFi HTTP)
    bool sendTelemetry(const TelemetryData& data);
    bool testConnection();
    
    // Estatísticas
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    void getLoRaStatistics(uint16_t& sent, uint16_t& failed);

private:
    // WiFi
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
    
    // Métodos privados WiFi
    String _createTelemetryJSON(const TelemetryData& data);
    bool _sendHTTPPost(const String& jsonPayload);
    bool _waitForConnection(uint32_t timeoutMs);
    
    // Métodos privados LoRa
    String _createLoRaPayload(const TelemetryData& data);
    bool _transmitLoRaPacket(const uint8_t* data, size_t length);
};

#endif // COMMUNICATION_MANAGER_H
