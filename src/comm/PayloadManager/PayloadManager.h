/**
 * @file PayloadManager.h
 * @brief Gerenciador de Payload Otimizado (Binário Puro)
 */

#ifndef PAYLOAD_MANAGER_H
#define PAYLOAD_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h" 
#include "esp_sntp.h"

class PayloadManager {
public:
    PayloadManager();

    // === Transmissão (TX) - Binário ===
    
    // Preenche buffer com dados do satélite. Retorna tamanho em bytes.
    int createSatellitePayload(const TelemetryData& data, uint8_t* buffer);
    
    // Preenche buffer com dados de relay. Retorna tamanho em bytes.
    int createRelayPayload(const TelemetryData& data, 
                           const GroundNodeBuffer& buffer, 
                           uint8_t* outBuffer,
                           std::vector<uint16_t>& includedNodes);
    
    // JSON mantém-se para WiFi/Debug
    String createTelemetryJSON(const TelemetryData& data, 
                               const GroundNodeBuffer& groundBuffer);

    // === Recepção (RX) ===
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // === Gestão ===
    void update(); 
    void markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds, unsigned long timestamp);
    uint8_t calculateNodePriority(const MissionData& node);
    MissionData getLastMissionData() const { return _lastMissionData; }
    int findNodeIndex(uint16_t nodeId);

private:
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];
    uint16_t _seqNodeId[MAX_GROUND_NODES];
    uint16_t _packetsReceived;
    uint16_t _packetsLost;
    
    // Encoders (Binário)
    void _encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset);
    void _encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset);
    
    // Decoders
    bool _decodeBinaryPayload(const String& hex, MissionData& data);
    bool _decodeAsciiPayload(const String& packet, MissionData& data);
    bool _validateAsciiChecksum(const String& packet);
};

#endif