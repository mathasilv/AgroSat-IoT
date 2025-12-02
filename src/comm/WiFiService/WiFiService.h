/**
 * @file WiFiService.h
 * @brief Gerenciador de WiFi Não-Bloqueante
 */

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiService {
public:
    WiFiService();

    bool begin();
    void update(); // Chama no loop para reconexão automática

    bool isConnected() const;
    int8_t getRSSI() const;
    String getIP() const;

private:
    const char* _ssid;
    const char* _pass;
    bool _connected;
    
    // Timers para não travar o loop
    unsigned long _lastCheck;
    unsigned long _lastReconnectAttempt;
    
    static constexpr unsigned long CHECK_INTERVAL = 5000;      // Verificar status a cada 5s
    static constexpr unsigned long RECONNECT_INTERVAL = 30000; // Tentar reconectar a cada 30s
};

#endif