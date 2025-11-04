/**
 * @file CommunicationManager.h
 * @brief Gerenciamento de comunicação WiFi e HTTP
 * @version 1.0.0
 * 
 * Responsável por:
 * - Conexão WiFi com o balão
 * - Envio de telemetria via HTTP POST (JSON)
 * - Retry e timeout handling
 * - Monitoramento de qualidade de sinal
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class CommunicationManager {
public:
    /**
     * @brief Construtor
     */
    CommunicationManager();
    
    /**
     * @brief Inicializa WiFi
     * @return true se conectado com sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza status da conexão
     */
    void update();
    
    /**
     * @brief Conecta ao WiFi do balão
     * @return true se conectado
     */
    bool connectWiFi();
    
    /**
     * @brief Desconecta do WiFi
     */
    void disconnectWiFi();
    
    /**
     * @brief Envia dados de telemetria via HTTP POST
     * @param telemetryData Estrutura com dados de telemetria
     * @return true se enviado com sucesso
     */
    bool sendTelemetry(const TelemetryData& data);
    
    /**
     * @brief Retorna status da conexão WiFi
     */
    bool isConnected();
    
    /**
     * @brief Retorna RSSI (força do sinal WiFi)
     */
    int8_t getRSSI();
    
    /**
     * @brief Retorna IP atribuído
     */
    String getIPAddress();
    
    /**
     * @brief Retorna estatísticas de comunicação
     */
    void getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries);
    
    /**
     * @brief Testa conectividade HTTP
     */
    bool testConnection();

private:
    bool _connected;
    int8_t _rssi;
    String _ipAddress;
    
    // Estatísticas
    uint16_t _packetsSent;
    uint16_t _packetsFailed;
    uint16_t _totalRetries;
    uint32_t _lastConnectionAttempt;
    
    /**
     * @brief Cria JSON de telemetria conforme especificação OBSAT
     */
    String _createTelemetryJSON(const TelemetryData& data);
    
    /**
     * @brief Envia requisição HTTP POST
     */
    bool _sendHTTPPost(const String& jsonPayload);
    
    /**
     * @brief Aguarda conexão WiFi com timeout
     */
    bool _waitForConnection(uint32_t timeoutMs);
};

#endif // COMMUNICATION_MANAGER_H
