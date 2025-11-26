/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação DUAL MODE (LoRa + WiFi/HTTP)
 * @version 7.1.0
 * @date 2025-11-25
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LoRa.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"

class CommunicationManager {
public:
    CommunicationManager();
    
    // Inicialização
    bool begin();
    bool initLoRa();
    bool retryLoRaInit(uint8_t maxAttempts = 3);
    
    // WiFi
    bool connectWiFi();
    void disconnectWiFi();
    bool isConnected();
    int8_t getRSSI();
    String getIPAddress();
    bool testConnection();
    
    // LoRa - Transmissão
    bool sendLoRa(const String& data);
    bool sendTelemetry(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    
    // LoRa - Recepção
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    bool processLoRaPacket(const String& packet, MissionData& data);  // ✅ APENAS UMA DECLARAÇÃO
    
    // LoRa - Status
    bool isLoRaOnline();
    int getLoRaRSSI();
    float getLoRaSNR();
    void getLoRaStatistics(uint16_t& sent, uint16_t& failed);
    
    // Controle
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    void reconfigureLoRa(OperationMode mode);
    
    // Gestão de nós terrestres
    MissionData getLastMissionData();
    void markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds);
    uint8_t calculatePriority(const MissionData& node);
    
    // Estatísticas
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    void update();

private:
    // ========================================
    // LoRa - Métodos Internos
    // ========================================
    void _configureLoRaParameters();
    bool _isChannelFree();
    void _adaptSpreadingFactor(float altitude);
    bool _validatePayloadSize(size_t size);
    
    // ========================================
    // Decodificação Payload AgriNode
    // ========================================
    bool _decodeAgriNodePayload(const String& hexPayload, MissionData& data);
    
    // ========================================
    // Criação de Payloads LoRa
    // ========================================
    String _createSatelliteTelemetryPayload(const TelemetryData& data);
    String _createForwardPayload(const GroundNodeBuffer& buffer, 
                                 std::vector<uint16_t>& includedNodes);
    
    // ========================================
    // Fallback - Formato Antigo (ASCII)
    // ========================================
    bool _parseAgroPacket(const String& packetData, MissionData& data);
    bool _validateChecksum(const String& packet);
    
    // ========================================
    // HTTP/JSON
    // ========================================
    String _createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer);
    bool _sendHTTPPost(const String& jsonPayload);
    
    // ========================================
    // Utilidades
    // ========================================
    int _findNodeIndex(uint16_t nodeId);
    
    // ========================================
    // Variáveis de Estado
    // ========================================
    
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
    uint8_t _currentSpreadingFactor;
    
    // Controle
    bool _loraEnabled;
    bool _httpEnabled;
    uint8_t _txFailureCount;
    unsigned long _lastTxFailure;
    
    // Buffer
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum[MAX_GROUND_NODES];

    String _createBinaryLoRaPayloadRelay(
        const TelemetryData& data,
        const GroundNodeBuffer& buffer,
        std::vector<uint16_t>& includedNodes);
};

#endif // COMMUNICATION_MANAGER_H
