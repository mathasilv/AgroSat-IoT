/**
 * @file CommunicationManager.h
 * @brief Sistema de comunicação dual com Store-and-Forward LEO
 * @version 6.0.0
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
    
    bool connectWiFi();
    void disconnectWiFi();
    bool isConnected();
    int8_t getRSSI();
    String getIPAddress();
    bool testConnection();
    
    bool sendTelemetry(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    
    bool initLoRa();
    bool retryLoRaInit(uint8_t maxAttempts = 3);
    bool sendLoRa(const String& data);
    bool sendLoRaTelemetry(const TelemetryData& data);
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    bool isLoRaOnline();
    int getLoRaRSSI();
    float getLoRaSNR();
    void getLoRaStatistics(uint16_t& sent, uint16_t& failed);
    void reconfigureLoRa(OperationMode mode);
    
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    bool isLoRaEnabled() const { return _loraEnabled; }
    bool isHTTPEnabled() const { return _httpEnabled; }
    
    bool processLoRaPacket(const String& packet, MissionData& data);
    MissionData getLastMissionData();
    String generateMissionPayload(const MissionData& data);
    String generateConsolidatedPayload(const GroundNodeBuffer& buffer);
    void markNodesAsForwarded(GroundNodeBuffer& buffer);

private:
    bool _connected;
    int8_t _rssi;
    String _ipAddress;
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    unsigned long _lastConnectionAttempt;
    
    bool _loraInitialized;
    int _loraRSSI;
    float _loraSNR;
    uint16_t _loraPacketsSent;
    uint16_t _loraPacketsFailed;
    unsigned long _lastLoRaTransmission;
    
    bool _loraEnabled;
    bool _httpEnabled;
    
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];
    
    String _createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    String _createLoRaPayload(const TelemetryData& data);
    String _createConsolidatedLoRaPayload(const TelemetryData& data, const GroundNodeBuffer& buffer);
    bool _sendHTTPPost(const String& jsonPayload);
    
    bool _parseAgroPacket(const String& packet, MissionData& data);
    bool _validateChecksum(const String& packet);
    bool _validatePayloadSize(size_t size);
    int _findNodeIndex(uint16_t nodeId);
};

#endif
