#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LoRa.h>
#include <SPI.h>
#include <vector>
#include <mutex>
#include "config.h"

// ✅ REMOVIDO - Não usar mais CBOR
// #include <CBOR.h>
// #include <CBOR_parsing.h>
// #include <CBOR_streams.h>

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
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    bool testConnection();
    
    // LoRa
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
    
    // Mission Data Processing
    bool processLoRaPacket(const String& packet, MissionData& data);
    MissionData getLastMissionData();
    String generateMissionPayload(const MissionData& data);
    String generateConsolidatedPayload(const GroundNodeBuffer& buffer);
    void markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds);
    uint8_t calculatePriority(const MissionData& node);
    
    // Telemetry
    bool sendTelemetry(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    
    // Control
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);

private:
    // WiFi state
    bool _connected;
    int8_t _rssi;
    String _ipAddress;
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    unsigned long _lastConnectionAttempt;
    
    // LoRa state
    bool _loraInitialized;
    int _loraRSSI;
    float _loraSNR;
    uint16_t _loraPacketsSent;
    uint16_t _loraPacketsFailed;
    unsigned long _lastLoRaTransmission;
    bool _loraEnabled;
    bool _httpEnabled;
    uint8_t _txFailureCount;
    unsigned long _lastTxFailure;
    uint8_t _currentSpreadingFactor;
    
    // Mission data
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];
    
    // Helper methods
    String _createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    String _createLoRaPayload(const TelemetryData& data);
    String _createConsolidatedLoRaPayload(const TelemetryData& data, 
                                          const GroundNodeBuffer& buffer,
                                          std::vector<uint16_t>& includedNodes);
    
    // ✅ RENOMEADO para deixar claro que é binary (não CBOR)
    String _createBinaryLoRaPayload(const TelemetryData& data,
                                     const GroundNodeBuffer& buffer,
                                     std::vector<uint16_t>& includedNodes);
    
    // ✅ Métodos de otimização LoRa
    bool _isChannelFree();
    void _adaptSpreadingFactor(float altitude);
    void _configureLoRaParameters();
    
    bool _sendHTTPPost(const String& jsonPayload);
    bool _parseAgroPacket(const String& packetData, MissionData& data);
    bool _validateChecksum(const String& packet);
    bool _validatePayloadSize(size_t size);
    int _findNodeIndex(uint16_t nodeId);
};

#endif
