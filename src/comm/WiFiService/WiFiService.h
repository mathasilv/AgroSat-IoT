/**
 * @file WiFiService.h
 * @brief Gerenciamento de conexão WiFi
 * @version 1.0.0
 * @date 2025-12-01
 */

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiService {
public:
    WiFiService();

    // Inicialização e conexão
    bool begin();
    bool connect();
    void disconnect();

    // Reconexão automática
    bool reconnect();
    bool retryConnect(uint8_t maxAttempts = 3);

    // Status
    bool    isConnected() const;
    int8_t  getRSSI() const;
    String  getIPAddress() const;
    uint8_t getSignalQuality() const; // 0-100%

    // Monitoramento (chamar no loop)
    void update();

    // Configuração
    void setCredentials(const char* ssid, const char* password);
    void setTimeout(uint32_t timeoutMs);

    // Estatísticas
    uint16_t getConnectionAttempts() const;
    uint16_t getSuccessfulConnections() const;
    uint16_t getDisconnections() const;
    uint32_t getUptimeSeconds() const;

private:
    // Credenciais
    const char* _ssid;
    const char* _password;

    // Estado
    bool          _connected;
    int8_t        _rssi;
    String        _ipAddress;
    unsigned long _lastConnectionAttempt;
    unsigned long _connectionStartTime;
    uint32_t      _timeoutMs;

    // Estatísticas
    uint16_t _connectionAttempts;
    uint16_t _successfulConnections;
    uint16_t _disconnections;

    // Métodos auxiliares
    void _updateStatus();
    void _onConnected();
    void _onDisconnected();
};

#endif // WIFI_SERVICE_H
