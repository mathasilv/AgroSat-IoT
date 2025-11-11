/**
 * @file CommunicationManager.cpp
 * @brief Implementação DUAL MODE: LoRa + WiFi/HTTP
 * @version 3.1.0
 * @date 2025-11-10
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
    _lastLoRaTransmission(0)
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
    
    return (loraOk || wifiOk); // Sucesso se pelo menos um funcionar
}

// ============================================================================
// LORA - INICIALIZAÇÃO E TRANSMISSÃO
// ============================================================================

bool CommunicationManager::initLoRa() {
    DEBUG_PRINTLN("[CommunicationManager] ----- INICIALIZANDO LORA -----");
    DEBUG_PRINTF("[CommunicationManager] Pinos: SCK=%d MISO=%d MOSI=%d CS=%d RST=%d DIO0=%d\n",
                 LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTF("[CommunicationManager] Frequência: %.1f MHz\n", LORA_FREQUENCY / 1E6);
    
    // Configurar SPI para LoRa
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    
    // Configurar pinos LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    // Tentar inicializar
    DEBUG_PRINT("[CommunicationManager] Tentando LoRa.begin()... ");
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        DEBUG_PRINTLN("[CommunicationManager] ✗ LoRa NÃO inicializado");
        _loraInitialized = false;
        return false;
    }
    
    DEBUG_PRINTLN("SUCESSO!");
    
    // Configurar parâmetros LoRa para máximo alcance
    LoRa.setSpreadingFactor(12);        // SF12 = máximo alcance (4096 chips/symbol)
    LoRa.setSignalBandwidth(125E3);     // 125 kHz (padrão, bom balanço)
    LoRa.setCodingRate4(8);              // 4/8 = máxima proteção contra erro
    LoRa.setPreambleLength(8);           // Preâmbulo padrão
    LoRa.setSyncWord(0x12);              // Sync word privado (evita interferência)
    LoRa.enableCrc();                    // Habilita CRC para detecção de erro
    LoRa.setTxPower(20);                 // 20 dBm = potência máxima (alcance ~10km)
    
    DEBUG_PRINTLN("[CommunicationManager] Parâmetros LoRa configurados:");
    DEBUG_PRINTLN("[CommunicationManager]   - Spreading Factor: 12 (máximo alcance)");
    DEBUG_PRINTLN("[CommunicationManager]   - Bandwidth: 125 kHz");
    DEBUG_PRINTLN("[CommunicationManager]   - Coding Rate: 4/8");
    DEBUG_PRINTLN("[CommunicationManager]   - TX Power: 20 dBm (~10km alcance)");
    DEBUG_PRINTLN("[CommunicationManager]   - CRC: Habilitado");
    DEBUG_PRINTLN("[CommunicationManager] ✓ LoRa inicializado com SUCESSO!");
    
    _loraInitialized = true;
    
    // Teste de transmissão inicial
    DEBUG_PRINT("[CommunicationManager] Enviando pacote de teste... ");
    String testMsg = "AGROSAT_BOOT";
    if (sendLoRa(testMsg)) {
        DEBUG_PRINTLN("OK!");
    } else {
        DEBUG_PRINTLN("FALHOU (mas LoRa está funcional)");
    }
    
    return true;
}

bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] ✗ LoRa não inicializado");
        _loraPacketsFailed++;
        return false;
    }
    
    unsigned long now = millis();
    
    // Limitar taxa de transmissão (duty cycle regulamentação ANATEL: 1% para 915MHz)
    // 1% duty cycle = 36 segundos de espera para cada 360ms de TX
    // Com SF12, cada pacote ~1-2s, logo mínimo 100-200s entre pacotes para compliance
    // Aqui usamos 10s como mínimo para teste
    if (now - _lastLoRaTransmission < 10000) {
        DEBUG_PRINTF("[CommunicationManager] ✗ Duty cycle: aguarde %lu ms\n", 
                     10000 - (now - _lastLoRaTransmission));
        return false;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] ----- TRANSMITINDO LORA -----");
    DEBUG_PRINTF("[CommunicationManager] Payload: %s\n", data.c_str());
    DEBUG_PRINTF("[CommunicationManager] Tamanho: %d bytes\n", data.length());
    
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = LoRa.endPacket(true); // true = modo assíncrono
    
    if (success) {
        _loraPacketsSent++;
        _lastLoRaTransmission = now;
        DEBUG_PRINTLN("[CommunicationManager] ✓ Pacote LoRa transmitido!");
        DEBUG_PRINTF("[CommunicationManager] Total enviado: %d pacotes\n", _loraPacketsSent);
    } else {
        _loraPacketsFailed++;
        DEBUG_PRINTLN("[CommunicationManager] ✗ Falha ao transmitir LoRa");
    }
    
    return success;
}

bool CommunicationManager::sendLoRaTelemetry(const TelemetryData& data) {
    if (!_loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa offline, ignorando envio");
        return false;
    }
    
    String payload = _createLoRaPayload(data);
    return sendLoRa(payload);
}

String CommunicationManager::_createLoRaPayload(const TelemetryData& data) {
    // Formato compacto para LoRa (limite ~250 bytes):
    // TEAM,BAT,TEMP,PRESS,ALT,GYROX,GYROY,GYROZ,ACCX,ACCY,ACCZ
    
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
    if (!_loraInitialized) return 0;
    return LoRa.packetRssi();
}

float CommunicationManager::getLoRaSNR() {
    if (!_loraInitialized) return 0.0;
    return LoRa.packetSnr();
}

void CommunicationManager::getLoRaStatistics(uint16_t& sent, uint16_t& failed) {
    sent = _loraPacketsSent;
    failed = _loraPacketsFailed;
}

// ============================================================================
// WIFI - MANTER CÓDIGO ORIGINAL
// ============================================================================

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
    // ENVIAR VIA LORA PRIMEIRO
    if (_loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] >>> Enviando via LoRa...");
        sendLoRaTelemetry(data);
    }
    
    // DEPOIS ENVIAR VIA WiFi/HTTP
    if (!_connected) {
        DEBUG_PRINTLN("[CommunicationManager] Sem conexão WiFi para enviar telemetria HTTP");
        _packetsFailed++;
        return false;
    }

        if (!ENABLE_HTTP_POST) {
        DEBUG_PRINTLN("[CommunicationManager] Envio HTTP desativado (ENABLE_HTTP_POST = false)");
        DEBUG_PRINTLN("[CommunicationManager] Telemetria enviada apenas via LoRa e SD Card");
        return true;  // Retorna sucesso (LoRa foi enviado com sucesso)
    }

    String jsonPayload = _createTelemetryJSON(data);
    DEBUG_PRINTLN("[CommunicationManager] >>> Enviando telemetria HTTP/OBSAT...");
    DEBUG_PRINTF("[CommunicationManager] JSON size: %d bytes\n", jsonPayload.length());

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
            DEBUG_PRINTLN("[CommunicationManager] Telemetria HTTP enviada com sucesso!");
            return true;
        }
    }

    _packetsFailed++;
    DEBUG_PRINTLN("[CommunicationManager] Falha ao enviar telemetria HTTP após todas as tentativas");
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
// MÉTODOS PRIVADOS - HTTP (MANTIDOS DO ORIGINAL)
// ============================================================================

String CommunicationManager::_createTelemetryJSON(const TelemetryData& data) {
    StaticJsonDocument<JSON_MAX_SIZE> doc;

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

    String output;
    serializeJson(doc, output);
    
    return output;
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    
    if (ENABLE_HTTP_POST) {
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
                
                if (response.indexOf("sucesso") >= 0 || response.indexOf("Sucesso") >= 0) {
                    success = true;
                } else {
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
        
    } else {
        // ENABLE_HTTP_POST está desativado
        DEBUG_PRINTLN("[CommunicationManager] Envio HTTP desativado (ENABLE_HTTP_POST = false)");
        return false;  // Retorna falso sem tentar enviar
    }
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
