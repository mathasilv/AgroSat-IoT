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

    // ========== CAMPOS OBRIGATÓRIOS OBSAT ==========
    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    
    float temp = isnan(data.temperature) ? 0.0 : data.temperature;
    if (temp < TEMP_MIN_VALID || temp > TEMP_MAX_VALID) temp = 0.0;
    doc["temperatura"] = temp;
    
    float press = isnan(data.pressure) ? 0.0 : data.pressure;
    if (press < PRESSURE_MIN_VALID || press > PRESSURE_MAX_VALID) press = 0.0;
    doc["pressao"] = press;

    JsonArray giroscopio = doc.createNestedArray("giroscopio");
    giroscopio.add(isnan(data.gyroX) ? 0.0 : data.gyroX);
    giroscopio.add(isnan(data.gyroY) ? 0.0 : data.gyroY);
    giroscopio.add(isnan(data.gyroZ) ? 0.0 : data.gyroZ);

    JsonArray acelerometro = doc.createNestedArray("acelerometro");
    acelerometro.add(isnan(data.accelX) ? 0.0 : data.accelX);
    acelerometro.add(isnan(data.accelY) ? 0.0 : data.accelY);
    acelerometro.add(isnan(data.accelZ) ? 0.0 : data.accelZ);

    // ========== PAYLOAD COM DEBUG ==========
    JsonObject payload = doc.createNestedObject("payload");
    
    // DEBUG: Mostrar TODOS os valores recebidos
    DEBUG_PRINTLN("[CommunicationManager] === DEBUG SENSORES ===");
    DEBUG_PRINTF("  Altitude: %.2f (isNaN: %d)\n", data.altitude, isnan(data.altitude));
    DEBUG_PRINTF("  Humidity: %.2f (isNaN: %d)\n", data.humidity, isnan(data.humidity));
    DEBUG_PRINTF("  CO2: %.2f (isNaN: %d)\n", data.co2, isnan(data.co2));
    DEBUG_PRINTF("  TVOC: %.2f (isNaN: %d)\n", data.tvoc, isnan(data.tvoc));
    DEBUG_PRINTF("  MagX: %.2f (isNaN: %d)\n", data.magX, isnan(data.magX));
    DEBUG_PRINTF("  MagY: %.2f (isNaN: %d)\n", data.magY, isnan(data.magY));
    DEBUG_PRINTF("  MagZ: %.2f (isNaN: %d)\n", data.magZ, isnan(data.magZ));
    DEBUG_PRINTLN("[CommunicationManager] ========================");
    
    // Altitude
    if (!isnan(data.altitude) && data.altitude >= 0) {
        payload["alt"] = round(data.altitude * 10) / 10;
        DEBUG_PRINTLN("  ✓ Altitude adicionada ao payload");
    } else {
        DEBUG_PRINTLN("  ✗ Altitude NÃO adicionada (NaN ou inválida)");
    }
    
    // Umidade
    if (!isnan(data.humidity) && data.humidity >= HUMIDITY_MIN_VALID && data.humidity <= HUMIDITY_MAX_VALID) {
        payload["hum"] = (int)data.humidity;
        DEBUG_PRINTLN("  ✓ Umidade adicionada ao payload");
    } else {
        DEBUG_PRINTLN("  ✗ Umidade NÃO adicionada (NaN ou fora do range)");
    }
    
    // CO2
    if (!isnan(data.co2) && data.co2 >= CO2_MIN_VALID && data.co2 <= CO2_MAX_VALID) {
        payload["co2"] = (int)data.co2;
        DEBUG_PRINTLN("  ✓ CO2 adicionado ao payload");
    } else {
        DEBUG_PRINTLN("  ✗ CO2 NÃO adicionado (NaN ou fora do range)");
    }
    
    // TVOC
    if (!isnan(data.tvoc) && data.tvoc >= TVOC_MIN_VALID && data.tvoc <= TVOC_MAX_VALID) {
        payload["tvoc"] = (int)data.tvoc;
        DEBUG_PRINTLN("  ✓ TVOC adicionado ao payload");
    } else {
        DEBUG_PRINTLN("  ✗ TVOC NÃO adicionado (NaN ou fora do range)");
    }
    
    // Magnetômetro
    if (!isnan(data.magX) && !isnan(data.magY) && !isnan(data.magZ)) {
        if (data.magX != 0.0 || data.magY != 0.0 || data.magZ != 0.0) {
            payload["magX"] = round(data.magX * 100) / 100;
            payload["magY"] = round(data.magY * 100) / 100;
            payload["magZ"] = round(data.magZ * 100) / 100;
            DEBUG_PRINTLN("  ✓ Magnetômetro adicionado ao payload");
        } else {
            DEBUG_PRINTLN("  ✗ Magnetômetro todos zero (sensor não conectado?)");
        }
    } else {
        DEBUG_PRINTLN("  ✗ Magnetômetro NÃO adicionado (NaN)");
    }
    
    // Status e tempo
    payload["stat"] = (data.systemStatus == STATUS_OK) ? "ok" : "err";
    payload["time"] = data.missionTime / 1000;

    // Serializar
    String output;
    serializeJson(doc, output);
    
    DEBUG_PRINTF("[CommunicationManager] Payload size: %d bytes\n", measureJson(payload));
    
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
