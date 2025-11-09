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
    if (WiFi.status() == WL_CONNECTED) {
        if (!_connected) {
            _connected = true;
            _rssi = WiFi.RSSI();
            _ipAddress = WiFi.localIP().toString();
            DEBUG_PRINTF("[CommunicationManager] WiFi conectado! IP: %s, RSSI: %d dBm\n", 
                        _ipAddress.c_str(), _rssi);
        }
        _rssi = WiFi.RSSI();
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

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_TIMEOUT_MS) {
            DEBUG_PRINTLN("[CommunicationManager] Timeout de conexão WiFi");
            return false;
        }
        DEBUG_PRINTF(".");
        delay(500);
    }

    _connected = true;
    _rssi = WiFi.RSSI();
    _ipAddress = WiFi.localIP().toString();
    DEBUG_PRINTF("\n[CommunicationManager] Conectado! IP: %s, RSSI: %d dBm\n", _ipAddress.c_str(), _rssi);
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

    // Montar URL HTTPS globalmente (definida em header ou em const), usar lá dentro de _sendHTTPPost
    // Vou assumir existe uma constante URL_HTTP definida adequadamente
   
    for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            _totalRetries++;
            DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d...\n", attempt + 1, WIFI_RETRY_ATTEMPTS);
            delay(1000);
        }

        if (_sendHTTPPost(jsonPayload)) {
            _packetsSent++;
            DEBUG_PRINTLN("[CommunicationManager] Telemetria enviada com sucesso!");
            return true;
        }
    }

    _packetsFailed++;
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
    String url = String("https://") + HTTP_SERVER + "/";
    
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
    StaticJsonDocument<512> doc;

    doc["equipe"] = TEAM_NAME;

    doc["bateria"] = data.batteryPercentage;  // Valor numérico puro

    doc["temperatura"] = isnan(data.temperature) ? 0.0 : data.temperature;

    doc["pressao"] = isnan(data.pressure) ? 0.0 : data.pressure;

    JsonObject giroscopio = doc.createNestedObject("giroscopio");
    giroscopio["x"] = isnan(data.gyroX) ? 0.0 : data.gyroX;
    giroscopio["y"] = isnan(data.gyroY) ? 0.0 : data.gyroY;
    giroscopio["z"] = isnan(data.gyroZ) ? 0.0 : data.gyroZ;

    JsonObject acelerometro = doc.createNestedObject("acelerometro");
    acelerometro["x"] = isnan(data.accelX) ? 0.0 : data.accelX;
    acelerometro["y"] = isnan(data.accelY) ? 0.0 : data.accelY;
    acelerometro["z"] = isnan(data.accelZ) ? 0.0 : data.accelZ;

    String payloadStr = String(data.payload);
    if (payloadStr.length() > 90) {
        payloadStr = payloadStr.substring(0, 90);
    }
    doc["payload"] = payloadStr;

    String output;
    serializeJson(doc, output);
    return output;
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    HTTPClient http;
    
    // Construir URL da comunicação aqui
    String url = String("https://") + HTTP_SERVER;
    if (HTTP_PORT != 80 && HTTP_PORT != 443) {
        url += ":" + String(HTTP_PORT);
    }
    url += HTTP_ENDPOINT;
    
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(jsonPayload);
    bool success = false;
    if (httpCode > 0) {
        DEBUG_PRINTF("[CommunicationManager] HTTP Code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            DEBUG_PRINTF("[CommunicationManager] Resposta: %s\n", response.c_str());
            success = true;
        } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
            String location = http.getLocation();
            DEBUG_PRINTF("[CommunicationManager] Redirect detectado para: %s\n", location.c_str());
            DEBUG_PRINTLN("[CommunicationManager] ERRO: URL incorreta, ajuste para HTTPS");
        } else {
            DEBUG_PRINTF("[CommunicationManager] HTTP Error: %d\n", httpCode);
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
