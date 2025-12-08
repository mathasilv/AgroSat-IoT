/**
 * @file PayloadManager.h
 * @brief Gerenciador de Payload com QoS Priority
 * @version 3.0.0 (5.3 QoS Priority Implementado)
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
    /**
     * @brief Cria payload de telemetria do satélite
     */
    int createSatellitePayload(const TelemetryData& data, uint8_t* buffer);
    
    /**
     * @brief Cria payload de relay (Store-and-Forward) com priorização QoS
     * @param data Telemetria do satélite
     * @param buffer Buffer de nós terrestres
     * @param outBuffer Buffer de saída (payload binário)
     * @param includedNodes IDs dos nós incluídos no payload (saída)
     * @return Tamanho do payload em bytes
     */
    int createRelayPayload(const TelemetryData& data, 
                           const GroundNodeBuffer& buffer, 
                           uint8_t* outBuffer,
                           std::vector<uint16_t>& includedNodes);
    
    /**
     * @brief Cria JSON de telemetria (backup HTTP)
     */
    String createTelemetryJSON(const TelemetryData& data, 
                               const GroundNodeBuffer& groundBuffer);

    // === Recepção (RX) ===
    /**
     * @brief Processa pacote LoRa recebido (binário, hex ou ASCII)
     */
    bool processLoRaPacket(const String& packet, MissionData& data);
    
    // === Gestão ===
    void update(); 
    
    /**
     * @brief Marca nós como retransmitidos
     */
    void markNodesAsForwarded(GroundNodeBuffer& buffer, 
                              const std::vector<uint16_t>& nodeIds, 
                              unsigned long timestamp);
    
    // === NOVO 5.3: QoS PRIORITY ===
    /**
     * @brief Calcula prioridade QoS do nó baseado em regras inteligentes
     * @param node Dados do nó terrestre
     * @return Prioridade (0=CRITICAL, 1=HIGH, 2=NORMAL, 3=LOW)
     */
    uint8_t calculateNodePriority(const MissionData& node);
    
    /**
     * @brief Ordena nós por prioridade (menor valor = maior prioridade)
     * @param nodes Array de nós
     * @param count Quantidade de nós
     */
    void sortNodesByPriority(MissionData* nodes, uint8_t count);
    
    /**
     * @brief Retorna estatísticas de priorização
     */
    void getPriorityStats(const GroundNodeBuffer& buffer, 
                          uint8_t& critical, uint8_t& high, 
                          uint8_t& normal, uint8_t& low);
    
    // === Outros ===
    MissionData getLastMissionData() const { return _lastMissionData; }
    int findNodeIndex(uint16_t nodeId);

private:
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];
    uint16_t _seqNodeId[MAX_GROUND_NODES];
    uint16_t _packetsReceived;
    uint16_t _packetsLost;
    
    // Encoders (TX)
    void _encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset);
    void _encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset);
    
    // Decoders (RX)
    bool _decodeRawPacket(const String& rawData, MissionData& data);
    bool _decodeHexStringPayload(const String& hex, MissionData& data);
    bool _decodeAsciiPayload(const String& packet, MissionData& data);
    bool _validateAsciiChecksum(const String& packet);
};

#endif