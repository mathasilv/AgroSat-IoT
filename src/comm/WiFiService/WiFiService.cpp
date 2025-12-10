/**
 * @file WiFiService.cpp
 * @brief Implementação WiFi Robusta (FIX: Timeout correto)
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
    
    // CORRIGIDO: Usa o timeout definido na configuração global
    unsigned long start = millis();
    while (millis() - start < WIFI_TIMEOUT_MS) {
        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            DEBUG_PRINTF("[WiFi] Conectado! IP: %s (%d dBm)\n", 
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
        delay(100);
    }
    
    DEBUG_PRINTLN("[WiFi] Aviso: Não conectou no boot. Tentará em background.");
    return false;
}

void WiFiService::update() {
    unsigned long now = millis();

    // 1. Verificar status periodicamente
    if (now - _lastCheck >= CHECK_INTERVAL) {
        _lastCheck = now;
        bool currentStatus = (WiFi.status() == WL_CONNECTED);

        if (currentStatus && !_connected) {
            _connected = true;
            DEBUG_PRINTF("[WiFi] Reconectado! IP: %s\n", WiFi.localIP().toString().c_str());
        } 
        else if (!currentStatus && _connected) {
            _connected = false;
            DEBUG_PRINTLN("[WiFi] Caiu a conexão.");
        }
    }

    // 2. Tentar reconectar se necessário (sem travar)
    if (!_connected && (now - _lastReconnectAttempt >= RECONNECT_INTERVAL)) {
        _lastReconnectAttempt = now;
        DEBUG_PRINTLN("[WiFi] Tentando reconexão em background...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
}

bool WiFiService::isConnected() const { return _connected; }

int8_t WiFiService::getRSSI() const { return WiFi.RSSI(); }

String WiFiService::getIP() const { return WiFi.localIP().toString(); }