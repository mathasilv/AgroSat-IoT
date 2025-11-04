/**
 * @file CommunicationManager.cpp
 * @brief Implementação do gerenciador de comunicação
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
    
    // Configurar modo WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(false);
    
    // Tentar conexão inicial
    return connectWiFi();
}

void CommunicationManager::update() {
    // Atualizar status da conexão
    if (WiFi.status() == WL_CONNECTED) {
        if (!_connected) {
            _connected = true;
            _rssi = WiFi.RSSI();
            _ipAddress = WiFi.localIP().toString();
            DEBUG_PRINTF("[CommunicationManager] WiFi conectado! IP: %s, RSSI: %d dBm\n", 
                        _ipAddress.c_str(), _rssi);
        }
        
        // Atualizar RSSI
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
    
    // Verificar intervalo mínimo entre tentativas
    uint32_t currentTime = millis();
    if (currentTime - _lastConnectionAttempt < 5000) {
        return false;  // Aguardar 5 segundos entre tentativas
    }
    
    _lastConnectionAttempt = currentTime;
    
    DEBUG_PRINTF("[CommunicationManager] Conectando ao WiFi '%s'...\n", WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Aguardar conexão com timeout
    if (_waitForConnection(WIFI_TIMEOUT_MS)) {
        _connected = true;
        _rssi = WiFi.RSSI();
        _ipAddress = WiFi.localIP().toString();
        
        DEBUG_PRINTF("[CommunicationManager] Conectado! IP: %s, RSSI: %d dBm\n", 
                    _ipAddress.c_str(), _rssi);
        return true;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] Falha na conexão WiFi!");
    return false;
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
    
    // Criar JSON de telemetria
    String jsonPayload = _createTelemetryJSON(data);
    
    DEBUG_PRINTLN("[CommunicationManager] Enviando telemetria...");
    DEBUG_PRINTLN(jsonPayload);
    
    // Tentar envio com retries
    for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            _totalRetries++;
            DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d...\n", 
                        attempt + 1, WIFI_RETRY_ATTEMPTS);
            delay(1000);  // Aguardar entre tentativas
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
    // Criar documento JSON conforme especificação OBSAT
    // Capacidade: JSON_MAX_SIZE bytes
    StaticJsonDocument<JSON_MAX_SIZE> doc;
    
    // Identificação da equipe
    doc["team"] = TEAM_NAME;
    doc["category"] = TEAM_CATEGORY;
    doc["mission"] = MISSION_NAME;
    doc["firmware"] = FIRMWARE_VERSION;
    
    // Timestamp
    doc["timestamp"] = data.timestamp;
    doc["missionTime"] = data.missionTime;
    
    // Bateria (obrigatório)
    doc["battery"]["voltage"] = serialized(String(data.batteryVoltage, 2));
    doc["battery"]["percentage"] = serialized(String(data.batteryPercentage, 1));
    
    // Temperatura (obrigatório)
    doc["temperature"] = serialized(String(data.temperature, 2));
    
    // Pressão (obrigatório)
    doc["pressure"] = serialized(String(data.pressure, 2));
    doc["altitude"] = serialized(String(data.altitude, 1));
    
    // Giroscópio (obrigatório - 3 eixos em rad/s)
    JsonObject gyro = doc.createNestedObject("gyroscope");
    gyro["x"] = serialized(String(data.gyroX, 4));
    gyro["y"] = serialized(String(data.gyroY, 4));
    gyro["z"] = serialized(String(data.gyroZ, 4));
    
    // Acelerômetro (obrigatório - 3 eixos em m/s²)
    JsonObject accel = doc.createNestedObject("accelerometer");
    accel["x"] = serialized(String(data.accelX, 4));
    accel["y"] = serialized(String(data.accelY, 4));
    accel["z"] = serialized(String(data.accelZ, 4));
    
    // Status do sistema
    doc["status"] = data.systemStatus;
    doc["errors"] = data.errorCount;
    
    // Payload customizado (máximo 90 bytes)
    // Dados da missão AgroSat-IoT (LoRa)
    if (strlen(data.payload) > 0) {
        doc["payload"] = data.payload;
    }
    
    // Serializar JSON
    String output;
    serializeJson(doc, output);
    
    return output;
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    HTTPClient http;
    
    // Construir URL completa
    String url = String("http://") + HTTP_SERVER + HTTP_ENDPOINT;
    
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    
    // Enviar POST
    int httpCode = http.POST(jsonPayload);
    
    bool success = false;
    if (httpCode > 0) {
        DEBUG_PRINTF("[CommunicationManager] HTTP Response: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            DEBUG_PRINTF("[CommunicationManager] Response: %s\n", response.c_str());
            success = true;
        }
    } else {
        DEBUG_PRINTF("[CommunicationManager] HTTP Error: %s\n", 
                    http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}

bool CommunicationManager::_waitForConnection(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            return false;  // Timeout
        }
        delay(100);
    }
    
    return true;
}
