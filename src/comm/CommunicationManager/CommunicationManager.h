/**
 * @file CommunicationManager.h
 * @brief Gerenciador de comunicação DUAL MODE (LoRa + WiFi/HTTP)
 * @version 8.0.0
 * @date 2025-12-01
 * 
 * CHANGELOG v8.0.0:
 * - [REFACTOR] Integração com PayloadManager (separação de responsabilidades)
 * - [REMOVED] Métodos privados de payload movidos para PayloadManager
 * - [CLEAN] Redução de ~550 linhas de código duplicado
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

#include "config.h"
#include "comm/LoRaService/LoRaService.h"
#include "comm/HttpService/HttpService.h" 
#include "comm/WiFiService/WiFiService.h" 
#include "comm/PayloadManager/PayloadManager.h"

class CommunicationManager {
public:
    CommunicationManager();

    // ========================================
    // Inicialização
    // ========================================
    bool begin();
    bool initLoRa();
    bool retryLoRaInit(uint8_t maxAttempts = 3);

    // ========================================
    // WiFi
    // ========================================
    bool   connectWiFi();
    void   disconnectWiFi();
    bool   isConnected();
    int8_t getRSSI();
    String getIPAddress();
    bool   testConnection();

    // ========================================
    // LoRa - Transmissão
    // ========================================
    bool sendLoRa(const String& data);
    bool sendTelemetry(const TelemetryData& data,
                       const GroundNodeBuffer& groundBuffer);

    // ========================================
    // LoRa - Recepção
    // ========================================
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    bool processLoRaPacket(const String& packet, MissionData& data);

    // ========================================
    // LoRa - Status
    // ========================================
    bool  isLoRaOnline();
    int   getLoRaRSSI();
    float getLoRaSNR();
    void  getLoRaStatistics(uint16_t& sent, uint16_t& failed);

    // ========================================
    // Controle
    // ========================================
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    void reconfigureLoRa(OperationMode mode);

    // ========================================
    // Gestão de Missão
    // ========================================
    MissionData getLastMissionData();
    void markNodesAsForwarded(GroundNodeBuffer& buffer,
                              const std::vector<uint16_t>& nodeIds);
    uint8_t calculatePriority(const MissionData& node);

    // ========================================
    // Estatísticas
    // ========================================
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);

    // ========================================
    // Atualização
    // ========================================
    void update();

private:
    // ========================================
    // Serviços Internos
    // ========================================
    LoRaService    _lora;
    HttpService    _http;
    WiFiService    _wifi;
    PayloadManager _payloadManager;  // ← Gerencia TODOS os payloads

    // ========================================
    // Variáveis de Estado (HTTP)
    // ========================================
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    bool     _httpEnabled;
};

#endif // COMMUNICATION_MANAGER_H
