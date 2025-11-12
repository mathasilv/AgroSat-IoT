/**
 * @file CommunicationManager.h
 * @brief Sistema de comunicação dual com processamento de payload integrado
 * @version 4.1.0
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
    
    // WiFi
    bool connectWiFi();
    void disconnectWiFi();
    bool isConnected();
    int8_t getRSSI();
    String getIPAddress();
    bool testConnection();
    
    // Telemetria
    bool sendTelemetry(const TelemetryData& data);
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    
    // LoRa
    bool initLoRa();
    bool sendLoRa(const String& data);
    bool sendLoRaTelemetry(const TelemetryData& data);
    bool receiveLoRaPacket(String& packet, int& rssi, float& snr);
    bool isLoRaOnline();
    int getLoRaRSSI();
    float getLoRaSNR();
    void getLoRaStatistics(uint16_t& sent, uint16_t& failed);
    
    // ✅ NOVO: Controle de transmissão
    void enableLoRa(bool enable);
    void enableHTTP(bool enable);
    bool isLoRaEnabled() const { return _loraEnabled; }
    bool isHTTPEnabled() const { return _httpEnabled; }
    
    // ✅ NOVO: Processamento de payload (substitui PayloadManager)
    bool processLoRaPacket(const String& packet, MissionData& data);
    MissionData getLastMissionData();
    String generateMissionPayload(const MissionData& data);

private:
    // WiFi/HTTP
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
    
    // Flags de controle
    bool _loraEnabled;
    bool _httpEnabled;
    
    // ✅ NOVO: Dados de missão e controle de sequência
    MissionData _lastMissionData;
    uint16_t _expectedSeqNum;
    
    // Métodos privados
    String _createTelemetryJSON(const TelemetryData& data);
    String _createLoRaPayload(const TelemetryData& data);
    bool _sendHTTPPost(const String& jsonPayload);
    
    // ✅ NOVO: Processamento de payload
    bool _parseAgroPacket(const String& packet, MissionData& data);
    bool _validateChecksum(const String& packet);
};

#endif
