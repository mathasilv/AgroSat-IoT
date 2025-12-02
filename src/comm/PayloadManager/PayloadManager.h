/**
 * @file PayloadManager.h
 * @brief Gerenciador de payloads da missão (LoRa + HTTP)
 * @version 2.0.0
 * @date 2025-12-01
 * 
 * Responsável por:
 * - Construção de payloads LoRa (binário/hex)
 * - Construção de payloads HTTP (JSON)
 * - Decodificação de pacotes recebidos
 * - Gerenciamento de estado da missão
 */

#ifndef PAYLOAD_MANAGER_H
#define PAYLOAD_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"
#include "comm/LoRaService/LoRaService.h"  // ← novo serviço LoRa

class PayloadManager {
public:
    PayloadManager();

    // ========================================
    // CONSTRUÇÃO - LoRa
    // ========================================
    
    /**
     * @brief Cria payload de telemetria do satélite
     */
    String createSatellitePayload(const TelemetryData& data);

    /**
     * @brief Cria payload RELAY (satélite + nós terrestres)
     * @param includedNodes [OUT] Nós incluídos no payload
     */
    String createRelayPayload(const TelemetryData& data,
                            const GroundNodeBuffer& buffer,     // ← const
                            std::vector<uint16_t>& includedNodes);  // ← OUT parâmetro


    /**
     * @brief Cria payload FORWARD (apenas nós terrestres)
     */
    String createForwardPayload(const GroundNodeBuffer& buffer,
                                std::vector<uint16_t>& includedNodes);

    // ========================================
    // CONSTRUÇÃO - HTTP/JSON
    // ========================================
    
    /**
     * @brief Cria JSON para envio HTTP
     */
    String createTelemetryJSON(const TelemetryData& data,
                              const GroundNodeBuffer& groundBuffer);

    // ========================================
    // DECODIFICAÇÃO
    // ========================================
    
    /**
     * @brief Decodifica pacote LoRa (binário ou ASCII)
     * @return true se decodificou com sucesso
     */
    bool processLoRaPacket(const String& packet, MissionData& data);

    // ========================================
    // GERENCIAMENTO DA MISSÃO
    // ========================================
    
    /**
     * @brief Atualiza estado da missão (timeout, sequência)
     */
    void update();

    /**
     * @brief Marca nós como retransmitidos
     */
    void markNodesAsForwarded(GroundNodeBuffer& buffer,              // ← não-const (modifica)
                            const std::vector<uint16_t>& nodeIds);  // ← const


    /**
     * @brief Calcula prioridade de um nó
     */
    uint8_t calculateNodePriority(const MissionData& node);

    /**
     * @brief Retorna dados da última missão
     */
    MissionData getLastMissionData() const;

    /**
     * @brief Estatísticas da missão
     */
    void getMissionStatistics(uint16_t& received, uint16_t& lost) const;

    /**
     * @brief Encontra índice de nó no buffer
     */
    int findNodeIndex(uint16_t nodeId);

private:
    // ========================================
    // DECODIFICAÇÃO PRIVADA
    // ========================================
    bool _decodeBinaryPayload(const String& hexPayload, MissionData& data);
    bool _decodeAsciiPayload(const String& packet, MissionData& data);
    bool _validateAsciiChecksum(const String& packet);

    // ========================================
    // CODIFICAÇÃO PRIVADA
    // ========================================
    void _encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset);
    void _encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset);
    String _binaryToHex(const uint8_t* buffer, size_t length);

    // ========================================
    // JSON PRIVADO
    // ========================================
    void _addSatelliteDataToJson(JsonDocument& doc, const TelemetryData& data);
    void _addGroundNodesToJson(JsonObject& payload, const GroundNodeBuffer& buffer);

    // ========================================
    // ESTADO DA MISSÃO
    // ========================================
    MissionData   _lastMissionData;
    uint16_t      _expectedSeqNum[MAX_GROUND_NODES];
    uint16_t      _seqNodeId[MAX_GROUND_NODES];
    uint16_t      _packetsReceived;
    uint16_t      _packetsLost;
    unsigned long _lastPacketTime;
    uint32_t      _packetTimeout;
};

#endif // PAYLOAD_MANAGER_H
