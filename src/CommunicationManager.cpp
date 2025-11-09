/**
 * @file CommunicationManager.cpp
 * @brief Implementação do gerenciador de comunicação com JSON + DEBUG
 * @note VERSÃO FINAL - Payload como objeto JSON (formato oficial OBSAT)
 * @version 2.3.2
 */

#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() :
    _connected(false),
    _rssi(0),
    _ipAddress(""),
    _packetsSent(0),
    _packetsFailed(0),
    _totalRetries(0),
    _lastConnectionAttempt(0)
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
        DEBUG_PRINT(".");
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
    DEBUG_PRINTLN("[CommunicationManager] Enviando telemetria OBSAT...");
    DEBUG_PRINTF("[CommunicationManager] JSON size: %d bytes\n", jsonPayload.length());
    DEBUG_PRINTF("[CommunicationManager] JSON: %s\n", jsonPayload.c_str());

    if (jsonPayload.length() > JSON_MAX_SIZE) {
        DEBUG_PRINTF("[CommunicationManager] ERRO: JSON muito grande (%d > %d bytes)\n", 
                    jsonPayload.length(), JSON_MAX_SIZE);
        _packetsFailed++;
        return false;
    }

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
    DEBUG_PRINTLN("[CommunicationManager] Falha ao enviar telemetria após todas as tentativas");
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
    String url = "https://obsat.org.br/teste_post/index.php";
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    return (httpCode == 200);
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

String CommunicationManager::_createTelemetryJSON(const TelemetryData& data) {
    StaticJsonDocument<JSON_MAX_SIZE> doc;

    // Campo "equipe" como NÚMERO
    doc["equipe"] = TEAM_ID;

    // Campo "bateria" como número
    doc["bateria"] = (int)data.batteryPercentage;

    // Campo "temperatura" (validação)
    float temp = isnan(data.temperature) ? 0.0 : data.temperature;
    if (temp < TEMP_MIN_VALID || temp > TEMP_MAX_VALID) temp = 0.0;
    doc["temperatura"] = temp;

    // Campo "pressao" (validação)
    float press = isnan(data.pressure) ? 0.0 : data.pressure;
    if (press < PRESSURE_MIN_VALID || press > PRESSURE_MAX_VALID) press = 0.0;
    doc["pressao"] = press;

    // "giroscopio" como ARRAY [x, y, z]
    JsonArray giroscopio = doc.createNestedArray("giroscopio");
    giroscopio.add(isnan(data.gyroX) ? 0.0 : data.gyroX);
    giroscopio.add(isnan(data.gyroY) ? 0.0 : data.gyroY);
    giroscopio.add(isnan(data.gyroZ) ? 0.0 : data.gyroZ);

    // "acelerometro" como ARRAY [x, y, z]
    JsonArray acelerometro = doc.createNestedArray("acelerometro");
    acelerometro.add(isnan(data.accelX) ? 0.0 : data.accelX);
    acelerometro.add(isnan(data.accelY) ? 0.0 : data.accelY);
    acelerometro.add(isnan(data.accelZ) ? 0.0 : data.accelZ);

    // "payload" como OBJETO JSON (formato oficial OBSAT)
    JsonObject payload = doc.createNestedObject("payload");
    
    // Adicionar apenas campos com dados válidos
    if (!isnan(data.altitude) && data.altitude > 0) {
        payload["alt"] = (int)data.altitude;
    }
    
    if (!isnan(data.humidity) && data.humidity >= 0 && data.humidity <= 100) {
        payload["hum"] = (int)data.humidity;
    }
    
    if (!isnan(data.co2) && data.co2 > 0 && data.co2 < 5000) {
        payload["co2"] = (int)data.co2;
    }
    
    if (!isnan(data.tvoc) && data.tvoc > 0) {
        payload["tvoc"] = (int)data.tvoc;
    }
    
    // Status do sistema
    payload["stat"] = (data.systemStatus == STATUS_OK) ? "ok" : "err";

    // Serializar
    String output;
    serializeJson(doc, output);
    
    return output;
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    HTTPClient http;
    
    String url = String("https://") + HTTP_SERVER + HTTP_ENDPOINT;
    
    DEBUG_PRINTF("[CommunicationManager] URL: %s\n", url.c_str());
    
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
            
            // Verificar se resposta contém "sucesso"
            if (response.indexOf("sucesso") >= 0 || response.indexOf("Sucesso") >= 0) {
                success = true;
            } else {
                // Tentar parsear como JSON
                StaticJsonDocument<256> responseDoc;
                DeserializationError error = deserializeJson(responseDoc, response);
                
                if (!error) {
                    const char* status = responseDoc["Status"];
                    if (status != nullptr) {
                        DEBUG_PRINTF("[CommunicationManager] Status OBSAT: %s\n", status);
                        String statusStr = String(status);
                        if (statusStr.indexOf("Sucesso") >= 0 || statusStr.indexOf("sucesso") >= 0) {
                            success = true;
                        }
                    }
                } else {
                    // Se não é JSON mas HTTP foi 200, considerar sucesso
                    success = true;
                }
            }
            
        } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
            String location = http.getLocation();
            DEBUG_PRINTF("[CommunicationManager] Redirect detectado para: %s\n", location.c_str());
        } else if (httpCode >= 400 && httpCode < 500) {
            DEBUG_PRINTF("[CommunicationManager] Erro cliente HTTP: %d\n", httpCode);
        } else if (httpCode >= 500) {
            DEBUG_PRINTF("[CommunicationManager] Erro servidor HTTP: %d\n", httpCode);
        }
    } else {
        DEBUG_PRINTF("[CommunicationManager] Erro de conexão: %s\n", http.errorToString(httpCode).c_str());
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
