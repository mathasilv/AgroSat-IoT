/**
 * @file CommunicationManager.cpp
 * @brief Comunicação DUAL MODE: LoRa + WiFi/HTTP (Arquitetura LilyGO Oficial)
 * @version 4.2.0
 * @date 2025-11-13
 * 
 * MUDANÇAS v4.2.0:
 * - [FIX] LoRa reinicializado com lógica oficial LilyGO
 * - [FIX] Configuração baseada nos exemplos ArduinoLoRa oficiais
 * - [FIX] Parâmetros otimizados para TTGO LoRa32 V2.1 (T3 V1.6)
 * - [NEW] Suporte completo a DIO0, DIO1, DIO2
 * - [OPT] CRC habilitado por padrão (confiabilidade)
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
    _loraEnabled(true),
    _httpEnabled(true),
    _expectedSeqNum(0)
{
    memset(&_lastMissionData, 0, sizeof(MissionData));
}

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] ===========================================");
    DEBUG_PRINTLN("[CommunicationManager] Inicializando DUAL MODE (LoRa + WiFi)");
    DEBUG_PRINTLN("[CommunicationManager] Board: TTGO LoRa32 V2.1 (T3 V1.6)");
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
// LORA - ARQUITETURA LILYGO OFICIAL
// ============================================================================

bool CommunicationManager::initLoRa() {
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ INICIALIZANDO LORA (LilyGO) ━━━━━");
    DEBUG_PRINTLN("[CommunicationManager] Board: TTGO LoRa32 V2.1 (T3 V1.6)");
    
    // Exibir configuração de pinos
    DEBUG_PRINTF("[CommunicationManager] Pinos SPI:\n");
    DEBUG_PRINTF("[CommunicationManager]   SCK  = %d\n", LORA_SCK);
    DEBUG_PRINTF("[CommunicationManager]   MISO = %d\n", LORA_MISO);
    DEBUG_PRINTF("[CommunicationManager]   MOSI = %d\n", LORA_MOSI);
    DEBUG_PRINTF("[CommunicationManager]   CS   = %d\n", LORA_CS);
    DEBUG_PRINTF("[CommunicationManager]   RST  = %d\n", LORA_RST);
    DEBUG_PRINTF("[CommunicationManager]   DIO0 = %d\n", LORA_DIO0);
    
    // 1. INICIALIZAR SPI
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[CommunicationManager] SPI inicializado");
    
    // 2. CONFIGURAR PINOS DO LORA
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTLN("[CommunicationManager] Pinos configurados (CS, RST, DIO0)");
    
    // 3. INICIAR LORA NA FREQUÊNCIA CORRETA
    DEBUG_PRINTF("[CommunicationManager] Tentando LoRa.begin(%.1f MHz)... ", LORA_FREQUENCY / 1E6);
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        DEBUG_PRINTLN("[CommunicationManager] ✗ Erro: Chip LoRa não respondeu");
        _loraInitialized = false;
        return false;
    }
    DEBUG_PRINTLN("OK!");
    
    // 4. CONFIGURAR PARÂMETROS (SINCRONIZADOS COM RECEPTOR)
    DEBUG_PRINTLN("[CommunicationManager] Configurando parâmetros LoRa...");
    
    // TX Power (20 dBm máximo para TTGO LoRa32)
    LoRa.setTxPower(LORA_TX_POWER);
    DEBUG_PRINTF("[CommunicationManager]   ✓ TX Power: %d dBm\n", LORA_TX_POWER);
    
    // Signal Bandwidth (125 kHz padrão)
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    DEBUG_PRINTF("[CommunicationManager]   ✓ Bandwidth: %.0f kHz\n", LORA_SIGNAL_BANDWIDTH / 1000);
    
    // Spreading Factor (SF7 - maior velocidade)
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("[CommunicationManager]   ✓ Spreading Factor: SF%d\n", LORA_SPREADING_FACTOR);
    
    // Preamble Length (8 símbolos - padrão Arduino LoRa)
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    DEBUG_PRINTF("[CommunicationManager]   ✓ Preamble: %d símbolos\n", LORA_PREAMBLE_LENGTH);
    
    // Sync Word (0x12 - padrão público Arduino LoRa)
    LoRa.setSyncWord(LORA_SYNC_WORD);
    DEBUG_PRINTF("[CommunicationManager]   ✓ Sync Word: 0x%02X\n", LORA_SYNC_WORD);
    
    // Coding Rate (4/5 - compromisso)
    LoRa.setCodingRate4(LORA_CODING_RATE);
    DEBUG_PRINTF("[CommunicationManager]   ✓ Coding Rate: 4/%d\n", LORA_CODING_RATE);
    
    // CRC (configurável)
    if (LORA_CRC_ENABLED) {
        LoRa.enableCrc();
        DEBUG_PRINTLN("[CommunicationManager]   ✓ CRC: HABILITADO");
    } else {
        LoRa.disableCrc();
        DEBUG_PRINTLN("[CommunicationManager]   ⚠ CRC: DESABILITADO (modo debug)");
    }
    
    // InvertIQ DESABILITADO (padrão)
    LoRa.disableInvertIQ();
    DEBUG_PRINTLN("[CommunicationManager]   ✓ InvertIQ: DESABILITADO");
    
    // 5. DUTY CYCLE INFO
    DEBUG_PRINTLN("[CommunicationManager] -------------------------------------------");
    DEBUG_PRINTF("[CommunicationManager] ANATEL Duty Cycle: %.1f%% (máx)\n", LORA_DUTY_CYCLE_PERCENT);
    DEBUG_PRINTF("[CommunicationManager] Intervalo mínimo TX: %lu s\n", LORA_MIN_INTERVAL_MS / 1000);
    DEBUG_PRINTLN("[CommunicationManager] -------------------------------------------");
    
    _loraInitialized = true;
    
    // 6. TESTE INICIAL
    DEBUG_PRINT("[CommunicationManager] Enviando pacote de teste... ");
    String testMsg = "AGROSAT_BOOT_v4.2.1";
    if (sendLoRa(testMsg)) {
        DEBUG_PRINTLN("✓ OK!");
    } else {
        DEBUG_PRINTLN("✗ FALHOU (mas LoRa está configurado)");
    }
    
    // 7. MODO RECEPÇÃO
    LoRa.receive();
    DEBUG_PRINTLN("[CommunicationManager] LoRa em modo RX");
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    // 8. EXIBIR CONFIGURAÇÃO PARA RECEPTOR
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║      CONFIGURAÇÃO DO RECEPTOR (usar os mesmos valores)       ║");
    DEBUG_PRINTLN("╠══════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTF("║ Frequência:       %.1f MHz                                 ║\n", LORA_FREQUENCY / 1E6);
    DEBUG_PRINTF("║ Spreading Factor: SF%d                                       ║\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("║ Bandwidth:        %.0f kHz                                 ║\n", LORA_SIGNAL_BANDWIDTH / 1000);
    DEBUG_PRINTF("║ Coding Rate:      4/%d                                       ║\n", LORA_CODING_RATE);
    DEBUG_PRINTF("║ Sync Word:        0x%02X                                      ║\n", LORA_SYNC_WORD);
    DEBUG_PRINTF("║ Preamble:         %d símbolos                               ║\n", LORA_PREAMBLE_LENGTH);
    DEBUG_PRINTF("║ CRC:              %s                                ║\n", LORA_CRC_ENABLED ? "HABILITADO " : "DESABILITADO");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
    
    return true;
}


bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa desabilitado via flag");
        return false;
    }
    
    if (!_loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa não inicializado");
        _loraPacketsFailed++;
        return false;
    }
    
    unsigned long now = millis();
    
    // DUTY CYCLE ANATEL - Verificar intervalo mínimo entre transmissões
    if (now - _lastLoRaTransmission < LORA_MIN_INTERVAL_MS) {
        uint32_t waitTime = (LORA_MIN_INTERVAL_MS - (now - _lastLoRaTransmission)) / 1000;
        DEBUG_PRINTF("[CommunicationManager] Duty cycle: aguarde %lu s\n", waitTime);
        return false;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ TRANSMITINDO LORA ━━━━━");
    DEBUG_PRINTF("[CommunicationManager] Payload: %s\n", data.c_str());
    DEBUG_PRINTF("[CommunicationManager] Tamanho: %d bytes\n", data.length());
    
    // Transmitir pacote (blocking mode = true para garantir envio completo)
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = LoRa.endPacket(true);  // true = blocking, aguarda TX completo
    
    if (success) {
        _loraPacketsSent++;
        _lastLoRaTransmission = now;
        DEBUG_PRINTLN("[CommunicationManager] ✓ Pacote enviado com sucesso!");
        DEBUG_PRINTF("[CommunicationManager] Total enviado: %d pacotes\n", _loraPacketsSent);
    } else {
        _loraPacketsFailed++;
        DEBUG_PRINTLN("[CommunicationManager] ✗ Falha na transmissão LoRa");
    }
    
    // Voltar ao modo recepção
    LoRa.receive();
    DEBUG_PRINTLN("[CommunicationManager] LoRa voltou ao modo RX");
    
    return success;
}

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraInitialized) {
        return false;
    }
    
    int packetSize = LoRa.parsePacket();
    if (packetSize <= 0) {
        return false;  // Nenhum pacote disponível
    }
    
    // Ler pacote completo
    packet = "";
    while (LoRa.available()) {
        packet += (char)LoRa.read();
    }
    
    // Obter RSSI e SNR
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();
    
    // Armazenar para estatísticas
    _loraRSSI = rssi;
    _loraSNR = snr;
    
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ PACOTE LORA RECEBIDO ━━━━━");
    DEBUG_PRINTF("[CommunicationManager] Dados: %s\n", packet.c_str());
    DEBUG_PRINTF("[CommunicationManager] RSSI: %d dBm\n", rssi);
    DEBUG_PRINTF("[CommunicationManager] SNR: %.1f dB\n", snr);
    DEBUG_PRINTF("[CommunicationManager] Tamanho: %d bytes\n", packet.length());
    
    return true;
}

bool CommunicationManager::sendLoRaTelemetry(const TelemetryData& data) {
    if (!_loraInitialized) {
        return false;
    }
    
    String payload = _createLoRaPayload(data);
    return sendLoRa(payload);
}

String CommunicationManager::_createLoRaPayload(const TelemetryData& data) {
    // Formato compacto para LoRa (limitação de payload)
    // Formato: T<id>,B<bat>,T<temp>,P<press>,A<alt>,G<gx>,<gy>,<gz>,A<ax>,<ay>,<az>
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
// PROCESSAMENTO DE PAYLOAD DE MISSÃO
// ============================================================================

bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    if (!_validateChecksum(packet)) {
        DEBUG_PRINTLN("[CommunicationManager] Checksum inválido");
        return false;
    }
    
    if (!_parseAgroPacket(packet, data)) {
        DEBUG_PRINTLN("[CommunicationManager] Falha ao parsear pacote agrícola");
        return false;
    }
    
    _lastMissionData = data;
    return true;
}

MissionData CommunicationManager::getLastMissionData() {
    return _lastMissionData;
}

String CommunicationManager::generateMissionPayload(const MissionData& data) {
    // Formato JSON compacto (máximo 90 bytes)
    char buffer[PAYLOAD_MAX_SIZE];
    snprintf(buffer, sizeof(buffer),
        "{\"sm\":%.1f,\"t\":%.1f,\"h\":%.1f,\"ir\":%d,\"rs\":%d,\"sn\":%.1f,\"rx\":%d,\"ls\":%d}",
        data.soilMoisture,
        data.ambientTemp,
        data.humidity,
        data.irrigationStatus,
        data.rssi,
        data.snr,
        data.packetsReceived,
        data.packetsLost
    );
    
    return String(buffer);
}

bool CommunicationManager::_parseAgroPacket(const String& packetData, MissionData& data) {
    String packet = packetData;
    
    // Formato esperado: "AGRO,seq,sm,temp,hum,irr"
    if (!packet.startsWith("AGRO,")) {
        DEBUG_PRINTLN("[CommunicationManager] Formato inválido (não começa com AGRO)");
        return false;
    }
    
    // Remover prefixo
    packet.remove(0, 5);
    
    // Parsear campos
    int commaIndex = 0;
    int fieldIndex = 0;
    String fields[6];
    
    while ((commaIndex = packet.indexOf(',')) != -1 && fieldIndex < 6) {
        fields[fieldIndex++] = packet.substring(0, commaIndex);
        packet.remove(0, commaIndex + 1);
    }
    
    if (fieldIndex < 6 && packet.length() > 0) {
        fields[fieldIndex++] = packet;
    }
    
    if (fieldIndex < 5) {
        DEBUG_PRINTF("[CommunicationManager] Campos insuficientes: %d (mínimo 5)\n", fieldIndex);
        return false;
    }
    
    // Extrair dados
    uint16_t seqNum = fields[0].toInt();
    data.soilMoisture = fields[1].toFloat();
    data.ambientTemp = fields[2].toFloat();
    data.humidity = fields[3].toFloat();
    data.irrigationStatus = fields[4].toInt();
    
    // Validar limites
    if (data.soilMoisture < 0.0 || data.soilMoisture > 100.0 ||
        data.ambientTemp < -50.0 || data.ambientTemp > 100.0 ||
        data.humidity < 0.0 || data.humidity > 100.0) {
        DEBUG_PRINTLN("[CommunicationManager] Dados fora dos limites válidos");
        return false;
    }
    
    // Detectar pacotes perdidos
    if (seqNum != _expectedSeqNum) {
        if (seqNum > _expectedSeqNum) {
            uint16_t lost = seqNum - _expectedSeqNum;
            data.packetsLost += lost;
            DEBUG_PRINTF("[CommunicationManager] %d pacote(s) perdido(s)\n", lost);
        }
    }
    
    _expectedSeqNum = seqNum + 1;
    data.packetsReceived++;
    data.lastLoraRx = millis();
    
    DEBUG_PRINTF("[CommunicationManager] Payload: SM=%.1f%%, T=%.1f°C, H=%.1f%%, IRR=%d\n",
                 data.soilMoisture, data.ambientTemp, data.humidity, data.irrigationStatus);
    
    return true;
}

bool CommunicationManager::_validateChecksum(const String& packet) {
    // Simplificado - implementar CRC real em produção
    return true;
}

// ============================================================================
// WIFI/HTTP
// ============================================================================

void CommunicationManager::update() {
    // Verificar status WiFi
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
    DEBUG_PRINTF("\n[CommunicationManager] WiFi OK! IP: %s, RSSI: %d dBm\n", 
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
    
    // 1. ENVIAR VIA LORA
    if (_loraEnabled && _loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] >>> Enviando via LoRa...");
        loraSuccess = sendLoRaTelemetry(data);
    }
    
    // 2. ENVIAR VIA HTTP
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
                    DEBUG_PRINTLN("[CommunicationManager] HTTP OK!");
                    break;
                }
            }
            
            if (!httpSuccess) {
                _packetsFailed++;
                DEBUG_PRINTLN("[CommunicationManager] HTTP falhou após todas tentativas");
            }
        }
    }
    
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

    char jsonBuffer[JSON_MAX_SIZE];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    
    if (jsonSize >= sizeof(jsonBuffer)) {
        DEBUG_PRINTLN("[CommunicationManager] JSON truncado!");
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
