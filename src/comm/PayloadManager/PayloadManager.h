/**
 * @file PayloadManager.h
 * @brief Gerenciador de Payload (Com Suporte a GPS)
 */

#ifndef PAYLOAD_MANAGER_H
#define PAYLOAD_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h" 

class PayloadManager {
public:
    PayloadManager();

    // === Transmissão (TX) ===
    
    // Cria pacote LoRa Binário (Header + Dados + IMU + GPS)
    String createSatellitePayload(const TelemetryData& data);
    
    // Cria pacote Relay (Header + Dados + Nós)
    String createRelayPayload(const TelemetryData& data, 
                              const GroundNodeBuffer& buffer, 
                              std::vector<uint16_t>& includedNodes);
    
    // Cria JSON para HTTP (Formato OBSAT Rigoroso + GPS)
    String createTelemetryJSON(const TelemetryData& data, 
                               const GroundNodeBuffer& groundBuffer);

    // === Recepção (RX) ===
    
    // Processa pacotes recebidos
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // === Gestão ===
    
    void update(); 
    void markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds);
    uint8_t calculateNodePriority(const MissionData& node);
    MissionData getLastMissionData() const { return _lastMissionData; }

    int findNodeIndex(uint16_t nodeId);

private:
    MissionData _lastMissionData;
    
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];
    uint16_t _seqNodeId[MAX_GROUND_NODES];
    uint16_t _packetsReceived;
    uint16_t _packetsLost;
    
    // === Encoders (TX) ===
    void _encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset);
    void _encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset);
    String _binaryToHex(const uint8_t* buffer, size_t length);
    
    // === Decoders (RX) ===
    bool _decodeBinaryPayload(const String& hex, MissionData& data);
    bool _decodeAsciiPayload(const String& packet, MissionData& data);
    bool _validateAsciiChecksum(const String& packet);
};

#endif