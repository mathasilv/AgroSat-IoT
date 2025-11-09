/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação WiFi/HTTP para OBSAT
 * @version 2.3.0
 */

#ifndef COMMUNICATIONMANAGER_H
#define COMMUNICATIONMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class CommunicationManager {
public:
    CommunicationManager();
    
    bool begin();
    void update();
    bool connectWiFi();
    void disconnectWiFi();
    bool sendTelemetry(const TelemetryData& data);
    bool isConnected();
    int8_t getRSSI();
    String getIPAddress();
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    bool testConnection();

private:
    bool _connected;
    int8_t _rssi;
    String _ipAddress;
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    uint32_t _lastConnectionAttempt;

    String _createTelemetryJSON(const TelemetryData& data);
    String _createPayloadString(const TelemetryData& data);
    bool _sendHTTPPost(const String& jsonPayload);
    bool _waitForConnection(uint32_t timeoutMs);
};

#endif
