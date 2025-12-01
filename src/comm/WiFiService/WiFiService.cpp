/**
 * @file WiFiService.cpp
 * @brief Implementação do gerenciamento de WiFi
 * @version 1.0.0
 * @date 2025-12-01
 */

#include "WiFiService.h"

WiFiService::WiFiService() :
    _ssid(WIFI_SSID),
    _password(WIFI_PASSWORD),
    _connected(false),
    _rssi(0),
    _ipAddress(""),
    _lastConnectionAttempt(0),
    _connectionStartTime(0),
    _timeoutMs(WIFI_TIMEOUT_MS),
    _connectionAttempts(0),
    _successfulConnections(0),
    _disconnections(0)
{
}

bool WiFiService::begin() {
    DEBUG_PRINTLN("[WiFiService] Inicializando...");
    
    // Configurar modo WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // controle manual
    
    DEBUG_PRINTF("[WiFiService] SSID: %s\n", _ssid);
    
    return connect();
}

bool WiFiService::connect() {
    // Evitar tentativas muito frequentes
    uint32_t now = millis();
    if (_lastConnectionAttempt != 0 && (now - _lastConnectionAttempt) < 5000) {
        DEBUG_PRINTLN("[WiFiService] Aguardando intervalo mínimo entre tentativas");
        return false;
    }

    // Se já conectado, retornar OK
    if (_connected && WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("[WiFiService] Já conectado");
        return true;
    }

    _lastConnectionAttempt = now;
    _connectionAttempts++;

    DEBUG_PRINTF("[WiFiService] Tentativa %d: Conectando '%s'...\n", 
                 _connectionAttempts, _ssid);

    // Iniciar conexão
    WiFi.begin(_ssid, _password);
    _connectionStartTime = millis();

    // Aguardar conexão
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - _connectionStartTime > _timeoutMs) {
            DEBUG_PRINTLN("[WiFiService] Timeout!");
            return false;
        }
        DEBUG_PRINT(".");
        delay(500);
    }

    DEBUG_PRINTLN();
    _onConnected();
    return true;
}

void WiFiService::disconnect() {
    DEBUG_PRINTLN("[WiFiService] Desconectando...");
    WiFi.disconnect();
    _connected = false;
    _rssi = 0;
    _ipAddress = "";
}

bool WiFiService::reconnect() {
    DEBUG_PRINTLN("[WiFiService] Reconectando...");
    disconnect();
    delay(1000);
    return connect();
}

bool WiFiService::retryConnect(uint8_t maxAttempts) {
    DEBUG_PRINTF("[WiFiService] Retry com máx %d tentativas\n", maxAttempts);

    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
        DEBUG_PRINTF("[WiFiService] Tentativa %d/%d\n", attempt, maxAttempts);

        if (connect()) {
            DEBUG_PRINTLN("[WiFiService] Conectado com sucesso!");
            return true;
        }

        if (attempt < maxAttempts) {
            uint32_t backoff = 2000 * attempt; // backoff exponencial
            DEBUG_PRINTF("[WiFiService] Aguardando %lu ms...\n", backoff);
            delay(backoff);
        }
    }

    DEBUG_PRINTLN("[WiFiService] Falha após todas tentativas");
    return false;
}

void WiFiService::update() {
    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        if (!_connected) {
            _onConnected();
        } else {
            // Atualizar RSSI periodicamente
            _rssi = WiFi.RSSI();
        }
    } else {
        if (_connected) {
            _onDisconnected();
        }
    }
}

bool WiFiService::isConnected() const {
    return _connected && (WiFi.status() == WL_CONNECTED);
}

int8_t WiFiService::getRSSI() const {
    return _rssi;
}

String WiFiService::getIPAddress() const {
    return _ipAddress;
}

uint8_t WiFiService::getSignalQuality() const {
    if (!_connected) return 0;

    // Converter RSSI (-100 a -50 dBm) para qualidade (0-100%)
    int8_t rssi = _rssi;
    
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    
    return (uint8_t)(2 * (rssi + 100));
}

void WiFiService::setCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    DEBUG_PRINTF("[WiFiService] Credenciais atualizadas: %s\n", _ssid);
}

void WiFiService::setTimeout(uint32_t timeoutMs) {
    _timeoutMs = timeoutMs;
    DEBUG_PRINTF("[WiFiService] Timeout configurado: %lu ms\n", _timeoutMs);
}

uint16_t WiFiService::getConnectionAttempts() const {
    return _connectionAttempts;
}

uint16_t WiFiService::getSuccessfulConnections() const {
    return _successfulConnections;
}

uint16_t WiFiService::getDisconnections() const {
    return _disconnections;
}

uint32_t WiFiService::getUptimeSeconds() const {
    if (!_connected || _connectionStartTime == 0) return 0;
    return (millis() - _connectionStartTime) / 1000;
}

// ========================================
// MÉTODOS PRIVADOS
// ========================================

void WiFiService::_updateStatus() {
    _rssi = WiFi.RSSI();
    _ipAddress = WiFi.localIP().toString();
}

void WiFiService::_onConnected() {
    _connected = true;
    _successfulConnections++;
    _connectionStartTime = millis();
    
    _updateStatus();

    DEBUG_PRINTLN("[WiFiService] ━━━━━ CONECTADO ━━━━━");
    DEBUG_PRINTF("[WiFiService] IP: %s\n", _ipAddress.c_str());
    DEBUG_PRINTF("[WiFiService] RSSI: %d dBm (%d%%)\n", 
                 _rssi, getSignalQuality());
    DEBUG_PRINTF("[WiFiService] Tentativas: %d\n", _connectionAttempts);
    DEBUG_PRINTLN("[WiFiService] ━━━━━━━━━━━━━━━━━━━━");
}

void WiFiService::_onDisconnected() {
    _connected = false;
    _disconnections++;
    
    DEBUG_PRINTLN("[WiFiService] ⚠️  DESCONECTADO!");
    DEBUG_PRINTF("[WiFiService] Total desconexões: %d\n", _disconnections);
}
