/**
 * @file WiFiService.h
 * @brief Serviço WiFi (CLEANED)
 */

#ifndef WIFISERVICE_H
#define WIFISERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiService {
public:
    WiFiService();
    bool begin();
    void update();
    bool isConnected() const;

private:
    const char* _ssid;
    const char* _pass;
    bool _connected;
    unsigned long _lastCheck;
    unsigned long _lastReconnectAttempt;
    
    static constexpr unsigned long CHECK_INTERVAL = 5000;
    static constexpr unsigned long RECONNECT_INTERVAL = 30000;
};

#endif
