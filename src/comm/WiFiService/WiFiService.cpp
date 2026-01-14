/**
 * @file WiFiService.cpp
 * @brief Implementação WiFi (CLEANED)
 */

#include "WiFiService.h"

WiFiService::WiFiService() 
    : _ssid(WIFI_SSID), _pass(WIFI_PASSWORD), 
      _connected(false), _lastCheck(0), _lastReconnectAttempt(0) 
{}

bool WiFiService::begin() {
    DEBUG_PRINTLN("[WiFi] Inicializando...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _pass);
    
    unsigned long start = millis();
    while (millis() - start < WIFI_TIMEOUT_MS) {
        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            DEBUG_PRINTF("[WiFi] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(100);
    }
    
    DEBUG_PRINTLN("[WiFi] Aviso: Nao conectou no boot. Tentara em background.");
    return false;
}

void WiFiService::update() {
    unsigned long now = millis();

    if (now - _lastCheck >= CHECK_INTERVAL) {
        _lastCheck = now;
        bool currentStatus = (WiFi.status() == WL_CONNECTED);

        if (currentStatus && !_connected) {
            _connected = true;
            DEBUG_PRINTF("[WiFi] Reconectado! IP: %s\n", WiFi.localIP().toString().c_str());
        } 
        else if (!currentStatus && _connected) {
            _connected = false;
            DEBUG_PRINTLN("[WiFi] Caiu a conexao.");
        }
    }

    if (!_connected && (now - _lastReconnectAttempt >= RECONNECT_INTERVAL)) {
        _lastReconnectAttempt = now;
        DEBUG_PRINTLN("[WiFi] Tentando reconexao...");
        WiFi.disconnect(true, false);
        delay(100);
        WiFi.begin(_ssid, _pass);
    }
}

bool WiFiService::isConnected() const { return _connected; }
