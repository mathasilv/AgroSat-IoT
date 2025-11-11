/**
 * @file CommunicationManager.cpp
 * @brief Comunicação DUAL MODE: LoRa + WiFi/HTTP (ÚNICO GERENCIADOR LORA)
 * @version 4.0.0
 * @date 2025-11-11
 * 
 * MUDANÇAS v4.0.0:
 * - LoRa gerenciado EXCLUSIVAMENTE aqui (PayloadManager removido)
 * - Duty cycle ANATEL compliance (200s entre TX)
 * - JSON serialization otimizada (buffer estático)
 */

#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() :
    _connected(false),
    _rssi(0),
    _ipAddress(""),
    _packetsSent(0),
    _packetsFailed(0),
    _totalRetries(0),
    _lastConnectionAttempt(0),
    _loraInitialized(false),
    _loraRSSI(0),
    _loraSNR(0.0),
    _loraPacketsSent(0),
    _loraPacketsFailed(0),
    _lastLoRaTransmission(0),
    _loraEnabled(true),      // ✅ NOVO: Iniciado como habilitado
    _httpEnabled(true)       // ✅ NOVO: Iniciado como habilitado
{
}

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] ===========================================");
    DEBUG_PRINTLN("[CommunicationManager] Inicializando sistema de comunicação DUAL");
    DEBUG_PRINTLN("[CommunicationManager] ===========================================");
    
    bool loraOk = initLoRa();
    bool wifiOk = connectWiFi();
    
    DEBUG_PRINTLN("[CommunicationManager] -------------------------------------------");
    DEBUG_PRINTF("[CommunicationManager] Status Final: LoRa=%s WiFi=%s\n", 
                 loraOk ? "OK" : "FALHOU", wifiOk ? "OK" : "FALHOU");
    DEBUG_PRINTLN("[CommunicationManager] ===========================================");
    
    return (loraOk || wifiOk);
}

// ============================================================================
// LORA - ÚNICA FONTE DE INICIALIZAÇÃO
// ============================================================================
bool CommunicationManager::initLoRa() {
    DEBUG_PRINTLN("[CommunicationManager] ----- INICIALIZANDO LORA -----");
    DEBUG_PRINTF("[CommunicationManager] Pinos: SCK=%d MISO=%d MOSI=%d CS=%d RST=%d DIO0=%d\n",
                 LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTF("[CommunicationManager] Frequência: %.1f MHz\n", LORA_FREQUENCY / 1E6);
    
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    DEBUG_PRINT("[CommunicationManager] Tentando LoRa.begin()... ");
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        _loraInitialized = false;
        return false;
    }
    
    DEBUG_PRINTLN("SUCESSO!");
    
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setPreambleLength(8);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();
    LoRa.setTxPower(LORA_TX_POWER);
    
    DEBUG_PRINTLN("[CommunicationManager] Parâmetros LoRa:");
    DEBUG_PRINTF("[CommunicationManager]   - SF: %d\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("[CommunicationManager]   - BW: %.0f kHz\n", LORA_SIGNAL_BANDWIDTH / 1000);
    DEBUG_PRINTF("[CommunicationManager]   - CR: 4/%d\n", LORA_CODING_RATE);
    DEBUG_PRINTF("[CommunicationManager]   - TX Power: %d dBm\n", LORA_TX_POWER);
    DEBUG_PRINTF("[CommunicationManager]   - Duty Cycle: %.1f%% (min %lus entre TX)\n", 
                 LORA_DUTY_CYCLE_PERCENT, LORA_MIN_INTERVAL_MS / 1000);
    
    _loraInitialized = true;
    
    // Teste inicial
    DEBUG_PRINT("[CommunicationManager] Enviando pacote de teste... ");
    String testMsg = "AGROSAT_BOOT_v4";
    if (sendLoRa(testMsg)) {
        DEBUG_PRINTLN("OK!");
    } else {
        DEBUG_PRINTLN("FALHOU (mas LoRa está funcional)");
    }
    
    LoRa.receive();  // Modo recepção contínua
    return true;
}

bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa desabilitado");
        return false;
    }
    
    if (!_loraInitialized) {
        _loraPacketsFailed++;
        return false;
    }
    
    unsigned long now = millis();
    
    // DUTY CYCLE ANATEL
    if (now - _lastLoRaTransmission < LORA_MIN_INTERVAL_MS) {
        uint32_t waitTime = (LORA_MIN_INTERVAL_MS - (now - _lastLoRaTransmission)) / 1000;
        DEBUG_PRINTF("[CommunicationManager] Duty cycle: aguarde %lu s\n", waitTime);
        return false;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] ----- TRANSMITINDO LORA -----");
    DEBUG_PRINTF("[CommunicationManager] Payload: %s (%d bytes)\n", 
                 data.c_str(), data.length());
    
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = LoRa.endPacket(true);
    
    if (success) {
        _loraPacketsSent++;
        _lastLoRaTransmission = now;
        DEBUG_PRINTLN("[CommunicationManager] ✓ Pacote enviado!");
        DEBUG_PRINTF("[CommunicationManager] Total: %d pacotes\n", _loraPacketsSent);
    } else {
        _loraPacketsFailed++;
        DEBUG_PRINTLN("[CommunicationManager] ✗ Falha na transmissão");
    }
    
    LoRa.receive();
    return success;
}

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraInitialized) return false;
    
    int packetSize = LoRa.parsePacket();
    if (packetSize <= 0) return false;
    
    packet = "";
    while (LoRa.available()) {
        packet += (char)LoRa.read();
    }
    
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();
    
    _loraRSSI = rssi;
    _loraSNR = snr;
    
    DEBUG_PRINTF("[CommunicationManager] LoRa RX: %s (RSSI=%d, SNR=%.1f)\n", 
                 packet.c_str(), rssi, snr);
    
    return true;
}

bool CommunicationManager::sendLoRaTelemetry(const TelemetryData& data) {
    if (!_loraInitialized) return false;
    String payload = _createLoRaPayload(data);
    return sendLoRa(payload);
}

String CommunicationManager::_createLoRaPayload(const TelemetryData& data) {
    // Formato compacto: T<id>,B<bat>,T<temp>,P<press>,A<alt>,G<xyz>,A<xyz>
    char buffer[200];
    snprintf(buffer, sizeof(buffer), 
             "T%d,B%.1f,T%.1f,P%.0f,A%.0f,G%.2f,%.2f,%.2f,A%.2f,%.2f,%.2f",
             TEAM_ID,
             data.batteryPercentage,
             data.temperature,
             data.pressure,
             data.altitude,
             data.gyroX, data.gyroY, data.gyroZ,
             data.accelX, data.accelY, data.accelZ);
    
    return String(buffer);
}

bool CommunicationManager::isLoRaOnline() {
    return _loraInitialized;
}

int CommunicationManager::getLoRaRSSI() {
    return _loraRSSI;
}

float CommunicationManager::getLoRaSNR() {
    return _loraSNR;
}

void CommunicationManager::getLoRaStatistics(uint16_t& sent, uint16_t& failed) {
    sent = _loraPacketsSent;
    failed = _loraPacketsFailed;
}

// ============================================================================
// WIFI/HTTP
// ============================================================================
void CommunicationManager::update() {
    // Verificar conexão WiFi
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
    
    // Processar pacotes LoRa recebidos (se necessário)
    if (_loraInitialized) {
        String packet;
        int rssi;
        float snr;
        if (receiveLoRaPacket(packet, rssi, snr)) {
            // PayloadManager pode consumir via polling
        }
    }
}

bool CommunicationManager::connectWiFi() {
    if (_connected) return true;

    uint32_t currentTime = millis();
    if (_lastConnectionAttempt != 0 && currentTime - _lastConnectionAttempt < 5000) {
        return false;
    }

    _lastConnectionAttempt = currentTime;

    DEBUG_PRINTF("[CommunicationManager] Conectando WiFi '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_TIMEOUT_MS) {
            DEBUG_PRINTLN("[CommunicationManager] Timeout WiFi");
            return false;
        }
        DEBUG_PRINT(".");
        delay(500);
    }

    _connected = true;
    _rssi = WiFi.RSSI();
    _ipAddress = WiFi.localIP().toString();
    DEBUG_PRINTF("\n[CommunicationManager] Conectado! IP: %s, RSSI: %d dBm\n", 
                 _ipAddress.c_str(), _rssi);
    return true;
}

void CommunicationManager::disconnectWiFi() {
    WiFi.disconnect();
    _connected = false;
    DEBUG_PRINTLN("[CommunicationManager] WiFi desconectado");
}

void CommunicationManager::enableLoRa(bool enable) {
    _loraEnabled = enable;
    DEBUG_PRINTF("[CommunicationManager] LoRa %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

void CommunicationManager::enableHTTP(bool enable) {
    _httpEnabled = enable;
    DEBUG_PRINTF("[CommunicationManager] HTTP %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

bool CommunicationManager::sendTelemetry(const TelemetryData& data) {
    bool loraSuccess = false;
    bool httpSuccess = false;
    
    // 1. ENVIAR VIA LORA (se habilitado)
    if (_loraEnabled && _loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] >>> Enviando via LoRa...");
        loraSuccess = sendLoRaTelemetry(data);
    } else if (!_loraEnabled) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa desabilitado (pulando)");
    }
    
    // 2. ENVIAR VIA HTTP (se habilitado)
    if (_httpEnabled && _connected) {
        String jsonPayload = _createTelemetryJSON(data);
        DEBUG_PRINTLN("[CommunicationManager] >>> Enviando HTTP/OBSAT...");
        DEBUG_PRINTF("[CommunicationManager] JSON size: %d bytes\n", jsonPayload.length());

        if (jsonPayload.length() > JSON_MAX_SIZE) {
            DEBUG_PRINTF("[CommunicationManager] ERRO: JSON muito grande (%d > %d)\n", 
                        jsonPayload.length(), JSON_MAX_SIZE);
            _packetsFailed++;
        } else {
            for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
                if (attempt > 0) {
                    _totalRetries++;
                    DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d...\n", 
                                attempt + 1, WIFI_RETRY_ATTEMPTS);
                    delay(1000);
                }

                if (_sendHTTPPost(jsonPayload)) {
                    _packetsSent++;
                    httpSuccess = true;
                    DEBUG_PRINTLN("[CommunicationManager] HTTP enviado com sucesso!");
                    break;
                }
            }
            
            if (!httpSuccess) {
                _packetsFailed++;
                DEBUG_PRINTLN("[CommunicationManager] Falha HTTP após todas tentativas");
            }
        }
    } else if (!_httpEnabled) {
        DEBUG_PRINTLN("[CommunicationManager] HTTP desabilitado (pulando)");
    } else if (!_connected) {
        DEBUG_PRINTLN("[CommunicationManager] Sem WiFi para HTTP");
        _packetsFailed++;
    }
    
    // Retorna sucesso se PELO MENOS UM método funcionou
    return (loraSuccess || httpSuccess);
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
    // Buffer estático para evitar fragmentação
    StaticJsonDocument<384> doc;

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

    JsonObject payload = doc.createNestedObject("payload");
    
    if (!isnan(data.altitude) && data.altitude >= 0) {
        payload["alt"] = round(data.altitude * 10) / 10;
    }
    
    if (!isnan(data.humidity) && data.humidity >= HUMIDITY_MIN_VALID && data.humidity <= HUMIDITY_MAX_VALID) {
        payload["hum"] = (int)data.humidity;
    }
    
    if (!isnan(data.co2) && data.co2 >= CO2_MIN_VALID && data.co2 <= CO2_MAX_VALID) {
        payload["co2"] = (int)data.co2;
    }
    
    if (!isnan(data.tvoc) && data.tvoc >= TVOC_MIN_VALID && data.tvoc <= TVOC_MAX_VALID) {
        payload["tvoc"] = (int)data.tvoc;
    }
    
    if (!isnan(data.magX) && !isnan(data.magY) && !isnan(data.magZ)) {
        if (data.magX != 0.0 || data.magY != 0.0 || data.magZ != 0.0) {
            payload["magX"] = round(data.magX * 100) / 100;
            payload["magY"] = round(data.magY * 100) / 100;
            payload["magZ"] = round(data.magZ * 100) / 100;
        }
    }
    
    payload["stat"] = (data.systemStatus == STATUS_OK) ? "ok" : "err";
    payload["time"] = data.missionTime / 1000;

    // Serializar para buffer estático
    char jsonBuffer[JSON_MAX_SIZE];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    
    if (jsonSize >= sizeof(jsonBuffer)) {
        DEBUG_PRINTLN("[Comm] JSON truncado!");
        return "";
    }
    
    return String(jsonBuffer);
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    HTTPClient http;
    
    String url = String("https://") + HTTP_SERVER + HTTP_ENDPOINT;
    
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
            
            if (response.indexOf("sucesso") >= 0 || response.indexOf("Sucesso") >= 0) {
                success = true;
            } else {
                StaticJsonDocument<256> responseDoc;
                DeserializationError error = deserializeJson(responseDoc, response);
                
                if (!error) {
                    const char* status = responseDoc["Status"];
                    if (status != nullptr) {
                        String statusStr = String(status);
                        if (statusStr.indexOf("Sucesso") >= 0 || statusStr.indexOf("sucesso") >= 0) {
                            success = true;
                        }
                    }
                } else {
                    success = true;
                }
            }
        }
    } else {
        DEBUG_PRINTF("[CommunicationManager] Erro: %s\n", 
                    http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}
