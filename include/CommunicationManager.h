/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação DUAL MODE (LoRa + WiFi/HTTP)
 * @version 7.2.0
 * @date 2025-12-01
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

#include "config.h"
#include "comm/LoRaService/LoRaService.h"   // novo serviço de rádio
#include "comm/HttpService/HttpService.h" 

class CommunicationManager {
public:
    CommunicationManager();

    // Inicialização
    bool begin();                         // inicializa LoRaService + WiFi
    bool initLoRa();                      // compatibilidade: delega para _lora.begin()
    bool retryLoRaInit(uint8_t maxAttempts = 3);

    // WiFi
    bool   connectWiFi();
    void   disconnectWiFi();
    bool   isConnected();                 // WiFi conectado
    int8_t getRSSI();
    String getIPAddress();
    bool   testConnection();

    // LoRa - Transmissão (alto nível)
    bool sendLoRa(const String& data);    // delega para _lora.send()
    bool sendTelemetry(const TelemetryData& data,
                       const GroundNodeBuffer& groundBuffer);

    // LoRa - Recepção
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    bool processLoRaPacket(const String& packet, MissionData& data);

    // LoRa - Status
    bool  isLoRaOnline();
    int   getLoRaRSSI();
    float getLoRaSNR();
    void  getLoRaStatistics(uint16_t& sent, uint16_t& failed);

    // Controle
    void enableLoRa(bool enable);         // apenas chama _lora.enable()
    void enableHTTP(bool enable);
    void reconfigureLoRa(OperationMode mode); // delega para _lora.reconfigure()

    // Gestão de nós terrestres
    MissionData getLastMissionData();
    void markNodesAsForwarded(GroundNodeBuffer& buffer,
                              const std::vector<uint16_t>& nodeIds);
    uint8_t calculatePriority(const MissionData& node);

    // Estatísticas globais (HTTP)
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);

    // Atualização periódica (WiFi)
    void update();

private:
    // ========================================
    // Serviços internos
    // ========================================
    LoRaService _lora;   // novo serviço de rádio

    // ========================================
    // Decodificação Payload AgriNode
    // ========================================
    bool _decodeAgriNodePayload(const String& hexPayload, MissionData& data);

    // ========================================
    // Criação de Payloads LoRa (nível de protocolo)
    // ========================================
    String _createSatelliteTelemetryPayload(const TelemetryData& data);
    String _createForwardPayload(const GroundNodeBuffer& buffer,
                                 std::vector<uint16_t>& includedNodes);
    String _createBinaryLoRaPayloadRelay(const TelemetryData& data,
                                         const GroundNodeBuffer& buffer,
                                         std::vector<uint16_t>& includedNodes);

    // ========================================
    // Fallback - Formato Antigo (ASCII)
    // ========================================
    bool _parseAgroPacket(const String& packetData, MissionData& data);
    bool _validateChecksum(const String& packet);

    // ========================================
    // HTTP/JSON
    // ========================================
    String _createTelemetryJSON(const TelemetryData& data,
                                const GroundNodeBuffer& groundBuffer);
    // ========================================
    // Utilidades
    // ========================================
    int _findNodeIndex(uint16_t nodeId);

    // ========================================
    // Variáveis de Estado
    // ========================================

    // WiFi / HTTP
    bool          _connected;
    int8_t        _rssi;
    String        _ipAddress;
    uint16_t      _packetsSent;          // HTTP OK
    uint16_t      _packetsFailed;        // HTTP falhas
    uint16_t      _totalRetries;
    unsigned long _lastConnectionAttempt;

    // Controle de features
    bool          _httpEnabled;

    // Buffer / nós terrestres
    MissionData   _lastMissionData;
    uint16_t      _expectedSeqNum[MAX_GROUND_NODES];
    uint16_t      _seqNodeId[MAX_GROUND_NODES];

    HttpService _http;
};

#endif // COMMUNICATION_MANAGER_H
