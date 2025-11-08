/**
 * @file CommunicationManager.cpp
 * @brief Implementação do gerenciador de comunicação com JSON + DEBUG
 */

#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() :
    _connected(false),
    _rssi(0),
    _ipAddress(""),
    _packetsSent(0),
    _packetsFailed(0),
    _totalRetries(0),
    _lastConnectionAttempt(0)  // inicializa com 0
{
}

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] Inicializando WiFi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(false);
    
    return connectWiFi();
}

void CommunicationManager::update() {
    wl_status_t wifiStatus = WiFi.status();

    if (wifiStatus == WL_CONNECTED) {
        if (!_connected) {
            _connected = true;
            _rssi = WiFi.RSSI();
            _ipAddress = WiFi.localIP().toString();
            DEBUG_PRINTF("[CommunicationManager] WiFi conectado! IP: %s, RSSI: %d dBm\n", 
                        _ipAddress.c_str(), _rssi);
        } else {
            // Atualiza RSSI só se conectado e passa maior intervalo para evitar chamadas contínuas
            static uint32_t lastRssiUpdate = 0;
            uint32_t currentTime = millis();
            if (currentTime - lastRssiUpdate > 10000) { // A cada 10s
                _rssi = WiFi.RSSI();
                lastRssiUpdate = currentTime;
            }
        }
    } else {
        if (_connected) {
            _connected = false;
            DEBUG_PRINTLN("[CommunicationManager] WiFi desconectado!");
        }
    }
}

bool CommunicationManager::connectWiFi() {
    if (_connected) return true;

    uint32_t currentTime = millis();

    // Permitir tentativa imediata se for a primeira vez
    if (_lastConnectionAttempt != 0 && currentTime - _lastConnectionAttempt < 5000) {
        DEBUG_PRINTLN("[CommunicationManager] Ignorando tentativa de conexão (recent)");
        return false;
    }

    _lastConnectionAttempt = currentTime;

    DEBUG_PRINTF("[CommunicationManager] Tentando conectar ao WiFi '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (!_waitForConnection(WIFI_TIMEOUT_MS)) {
        DEBUG_PRINTLN("[CommunicationManager] Timeout de conexão WiFi");
        return false;
    }

    _connected = true;
    _rssi = WiFi.RSSI();
    _ipAddress = WiFi.localIP().toString();
    DEBUG_PRINTF("[CommunicationManager] Conectado! IP: %s, RSSI: %d dBm\n", _ipAddress.c_str(), _rssi);
    return true;
}

void CommunicationManager::disconnectWiFi() {
    WiFi.disconnect();
    _connected = false;
    DEBUG_PRINTLN("[CommunicationManager] WiFi desconectado");
}

bool CommunicationManager::sendTelemetry(const TelemetryData& data) {
    if (!_connected) {
        DEBUG_PRINTLN("[CommunicationManager] Sem conexão WiFi para enviar telemetria");
        _packetsFailed++;
        return false;
    }
    
    String jsonPayload = _createTelemetryJSON(data);
    
    DEBUG_PRINTLN("[CommunicationManager] Enviando telemetria...");
    DEBUG_PRINTF("[CommunicationManager] JSON size: %d bytes\n", jsonPayload.length());
    
    DEBUG_PRINTLN("[CommunicationManager] ========== DEBUG HTTP ==========");
    DEBUG_PRINTF("[CommunicationManager] HTTP_SERVER: %s\n", HTTP_SERVER);
    DEBUG_PRINTF("[CommunicationManager] HTTP_PORT: %d\n", HTTP_PORT);
    DEBUG_PRINTF("[CommunicationManager] HTTP_ENDPOINT: %s\n", HTTP_ENDPOINT);
    String debugUrl = String("http://") + HTTP_SERVER;
    if (HTTP_PORT != 80) {
        debugUrl += ":" + String(HTTP_PORT);
    }
    debugUrl += HTTP_ENDPOINT;
    DEBUG_PRINTF("[CommunicationManager] URL Completa: %s\n", debugUrl.c_str());
    DEBUG_PRINTLN("[CommunicationManager] ================================");
    
    for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            _totalRetries++;
            DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d...\n", 
                          attempt + 1, WIFI_RETRY_ATTEMPTS);
            delay(1000);
        }
        
        if (_sendHTTPPost(jsonPayload)) {
            _packetsSent++;
            DEBUG_PRINTLN("[CommunicationManager] Telemetria enviada com sucesso!");
            return true;
        }
    }
    
    _packetsFailed++;
    DEBUG_PRINTLN("[CommunicationManager] Falha ao enviar telemetria após retries");
    return false;
}

bool CommunicationManager::isConnected() {
    return _connected;
}

int8_t CommunicationManager::getRSSI() {
    return _rssi;
}

String CommunicationManager::getIPAddress() {
    return _ipAddress;
}

void CommunicationManager::getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries) {
    sent = _packetsSent;
    failed = _packetsFailed;
    retries = _totalRetries;
}

bool CommunicationManager::testConnection() {
    if (!_connected) return false;
    
    HTTPClient http;
    String url = String("http://") + HTTP_SERVER + "/";
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    return (httpCode > 0);
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

String CommunicationManager::_createTelemetryJSON(const TelemetryData& data) {
    StaticJsonDocument<JSON_MAX_SIZE> doc;
    
    doc["team"] = TEAM_NAME;
    doc["category"] = TEAM_CATEGORY;
    doc["mission"] = MISSION_NAME;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["timestamp"] = data.timestamp;
    doc["missionTime"] = data.missionTime;
    doc["batteryVoltage"] = serialized(String(data.batteryVoltage, 2));
    doc["batteryPercentage"] = serialized(String(data.batteryPercentage, 1));
    if (!isnan(data.temperature))        doc["temperature"] = serialized(String(data.temperature, 2));
    if (!isnan(data.pressure))           doc["pressure"] = serialized(String(data.pressure, 2));
    if (!isnan(data.altitude))           doc["altitude"] = serialized(String(data.altitude, 1));
    doc["gyroX"] = serialized(String(data.gyroX, 4));
    doc["gyroY"] = serialized(String(data.gyroY, 4));
    doc["gyroZ"] = serialized(String(data.gyroZ, 4));
    doc["accelX"] = serialized(String(data.accelX, 4));
    doc["accelY"] = serialized(String(data.accelY, 4));
    doc["accelZ"] = serialized(String(data.accelZ, 4));
    if (!isnan(data.humidity))           doc["humidity"] = serialized(String(data.humidity, 1));
    if (!isnan(data.co2) && data.co2 > 0) doc["co2"] = serialized(String(data.co2, 0));
    if (!isnan(data.tvoc) && data.tvoc > 0) doc["tvoc"] = serialized(String(data.tvoc, 0));
    if (!isnan(data.magX) && !isnan(data.magY) && !isnan(data.magZ)) {
        doc["magX"] = serialized(String(data.magX, 2));
        doc["magY"] = serialized(String(data.magY, 2));
        doc["magZ"] = serialized(String(data.magZ, 2));
    }
    doc["systemStatus"] = data.systemStatus;
    doc["errorCount"] = data.errorCount;
    if (strlen(data.payload) > 0)         doc["payload"] = data.payload;

    String output;
    serializeJson(doc, output);

    if (output.length() > JSON_MAX_SIZE - 50) {
        DEBUG_PRINTF("[CommunicationManager] AVISO: JSON próximo do limite: %d bytes\n", output.length());
    }

    return output;
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    HTTPClient http;

    String url = String("http://") + HTTP_SERVER;
    if (HTTP_PORT != 80) {
        url += ":" + String(HTTP_PORT);
    }
    url += HTTP_ENDPOINT;

    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);

    bool success = false;
    if (httpCode > 0) {
        DEBUG_PRINTF("[CommunicationManager] HTTP Code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            DEBUG_PRINTF("[CommunicationManager] Resposta: %s\n", response.c_str());
            success = true;
        }
    } else {
        DEBUG_PRINTF("[CommunicationManager] HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
}

bool CommunicationManager::_waitForConnection(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            return false;
        }
        delay(100);
    }
    return true;
}
