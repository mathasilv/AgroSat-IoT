/**
 * @file PayloadManager.h
 * @brief Gerenciador de Payload com QoS Priority (CLEANED)
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
    int createSatellitePayload(const TelemetryData& data, uint8_t* buffer);
    
    int createRelayPayload(const TelemetryData& data, 
                           const GroundNodeBuffer& buffer, 
                           uint8_t* outBuffer,
                           std::vector<uint16_t>& includedNodes);
    
    String createTelemetryJSON(const TelemetryData& data, 
                               const GroundNodeBuffer& groundBuffer);

    // === Recepção (RX) ===
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // === Gestão ===
    void update(); 
    
    void markNodesAsForwarded(GroundNodeBuffer& buffer, 
                              const std::vector<uint16_t>& nodeIds, 
                              unsigned long timestamp);
    
    // Método estático para cálculo de prioridade
    static uint8_t calculateNodePriority(const MissionData& node);
    
    void sortNodesByPriority(MissionData* nodes, uint8_t count);
    
    void getPriorityStats(const GroundNodeBuffer& buffer, 
                          uint8_t& critical, uint8_t& high, 
                          uint8_t& normal, uint8_t& low);

private:
    MissionData _lastMissionData;
    
    void _encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset);
    void _encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset);
    
    bool _decodeRawPacket(const String& rawData, MissionData& data);
    bool _decodeHexStringPayload(const String& hex, MissionData& data);
};

#endif
