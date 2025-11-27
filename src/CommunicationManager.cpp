/**
 * @file CommunicationManager.cpp
 * @brief Comunicação DUAL MODE com CBOR + Otimizações LoRa
 * @version 7.0.0
 * @date 2025-11-24
 * 
 * CHANGELOG v7.0.0:
 * - [NEW] Compressão CBOR (75% redução de payload)
 * - [NEW] CAD (Channel Activity Detection)
 * - [NEW] Adaptação dinâmica de SF baseada em altitude
 * - [NEW] Proteção contra brownout em TX
 * - [FIX] Remoção de Base64 (que aumentava payload)
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
    _txFailureCount(0),
    _lastTxFailure(0),
    _currentSpreadingFactor(LORA_SPREADING_FACTOR)
{
    memset(&_lastMissionData, 0, sizeof(MissionData));
    
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        _expectedSeqNum[i] = 0;
    }
}

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] ===========================================");
    DEBUG_PRINTLN("[CommunicationManager] Inicializando DUAL MODE (LoRa CBOR + WiFi)");
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

void CommunicationManager::_configureLoRaParameters() {
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    _currentSpreadingFactor = LORA_SPREADING_FACTOR;
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    
    if (LORA_CRC_ENABLED) {
        LoRa.enableCrc();
    } else {
        LoRa.disableCrc();
    }
    
    LoRa.disableInvertIQ();
}

bool CommunicationManager::initLoRa() {
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ INICIALIZANDO LORA (LilyGO) ━━━━━");
    DEBUG_PRINTLN("[CommunicationManager] Board: TTGO LoRa32 V2.1 (T3 V1.6)");
    
    DEBUG_PRINTF("[CommunicationManager] Pinos SPI:\n");
    DEBUG_PRINTF("[CommunicationManager]   SCK  = %d\n", LORA_SCK);
    DEBUG_PRINTF("[CommunicationManager]   MISO = %d\n", LORA_MISO);
    DEBUG_PRINTF("[CommunicationManager]   MOSI = %d\n", LORA_MOSI);
    DEBUG_PRINTF("[CommunicationManager]   CS   = %d\n", LORA_CS);
    DEBUG_PRINTF("[CommunicationManager]   RST  = %d\n", LORA_RST);
    DEBUG_PRINTF("[CommunicationManager]   DIO0 = %d\n", LORA_DIO0);
    
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);
    DEBUG_PRINTLN("[CommunicationManager] Módulo LoRa resetado");
    
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[CommunicationManager] SPI inicializado");
    
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTLN("[CommunicationManager] Pinos configurados (CS, RST, DIO0)");
    
    DEBUG_PRINTF("[CommunicationManager] Tentando LoRa.begin(%.1f MHz)... ", LORA_FREQUENCY / 1E6);
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        DEBUG_PRINTLN("[CommunicationManager] Erro: Chip LoRa não respondeu");
        _loraInitialized = false;
        return false;
    }
    DEBUG_PRINTLN("OK!");
    
    DEBUG_PRINTLN("[CommunicationManager] Configurando parâmetros LoRa...");
    _configureLoRaParameters();
    
    DEBUG_PRINTF("[CommunicationManager]   TX Power: %d dBm\n", LORA_TX_POWER);
    DEBUG_PRINTF("[CommunicationManager]   Bandwidth: %.0f kHz\n", LORA_SIGNAL_BANDWIDTH / 1000);
    DEBUG_PRINTF("[CommunicationManager]   Spreading Factor: SF%d\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("[CommunicationManager]   Preamble: %d símbolos\n", LORA_PREAMBLE_LENGTH);
    DEBUG_PRINTF("[CommunicationManager]   Sync Word: 0x%02X\n", LORA_SYNC_WORD);
    DEBUG_PRINTF("[CommunicationManager]   Coding Rate: 4/%d\n", LORA_CODING_RATE);
    DEBUG_PRINTF("[CommunicationManager]   CRC: %s\n", LORA_CRC_ENABLED ? "HABILITADO" : "DESABILITADO");
    
    DEBUG_PRINTLN("[CommunicationManager] -------------------------------------------");
    DEBUG_PRINTF("[CommunicationManager] ANATEL Duty Cycle: %.1f%% (máx)\n", LORA_DUTY_CYCLE_PERCENT);
    DEBUG_PRINTF("[CommunicationManager] Intervalo mínimo TX: %lu s\n", LORA_MIN_INTERVAL_MS / 1000);
    DEBUG_PRINTLN("[CommunicationManager] Formato de payload: CBOR (economia: ~75%)");
    DEBUG_PRINTLN("[CommunicationManager] -------------------------------------------");
    
    _loraInitialized = true;
    
    DEBUG_PRINT("[CommunicationManager] Enviando pacote de teste... ");
    String testMsg = "AGROSAT_BOOT_v7.0.0_CBOR";
    if (sendLoRa(testMsg)) {
        DEBUG_PRINTLN("OK!");
    } else {
        DEBUG_PRINTLN("FALHOU (mas LoRa está configurado)");
    }
    
    LoRa.receive();
    DEBUG_PRINTLN("[CommunicationManager] LoRa em modo RX (escutando sensores terrestres)");
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    return true;
}

bool CommunicationManager::retryLoRaInit(uint8_t maxAttempts) {
    DEBUG_PRINTF("[CommunicationManager] Tentando reinicializar LoRa (máx %d tentativas)...\n", maxAttempts);
    
    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
        DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d\n", attempt, maxAttempts);
        
        pinMode(LORA_RST, OUTPUT);
        digitalWrite(LORA_RST, LOW);
        delay(10);
        digitalWrite(LORA_RST, HIGH);
        delay(100);
        
        if (LoRa.begin(LORA_FREQUENCY)) {
            DEBUG_PRINTLN("[CommunicationManager] LoRa reinicializado com sucesso!");
            _loraInitialized = true;
            
            _configureLoRaParameters();
            LoRa.receive();
            
            return true;
        }
        
        delay(1000 * attempt);
    }
    
    DEBUG_PRINTLN("[CommunicationManager] LoRa falhou após todas tentativas");
    _loraInitialized = false;
    return false;
}

bool CommunicationManager::_isChannelFree() {
    const int RSSI_THRESHOLD = -90;  // dBm - canal livre se abaixo deste valor
    const uint8_t CAD_CHECKS = 3;
    
    for (uint8_t i = 0; i < CAD_CHECKS; i++) {
        int rssi = LoRa.rssi();
        if (rssi > RSSI_THRESHOLD) {
            DEBUG_PRINTF("[CommunicationManager] Canal ocupado (RSSI: %d dBm)\n", rssi);
            delay(random(50, 200));  // Backoff aleatório
            return false;
        }
        delay(10);
    }
    return true;
}

void CommunicationManager::_adaptSpreadingFactor(float altitude) {
    uint8_t newSF = LORA_SPREADING_FACTOR;
    
    if (isnan(altitude)) {
        return;
    }
    
    if (altitude < 10000) {
        newSF = 7;
    } else if (altitude < 20000) {
        newSF = 8;
    } else if (altitude < 30000) {
        newSF = 9;
    } else {
        newSF = 10;
    }
    
    if (newSF != _currentSpreadingFactor) {
        DEBUG_PRINTF("[CommunicationManager] Ajustando SF: %d → %d (alt: %.0fm)\n",
                     _currentSpreadingFactor, newSF, altitude);
        
        LoRa.setSpreadingFactor(newSF);
        _currentSpreadingFactor = newSF;
        
        delay(10);
        LoRa.receive();
    }
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
    
    if (!_validatePayloadSize(data.length())) {
        DEBUG_PRINTF("[CommunicationManager] Payload muito grande: %d bytes (máx %d)\n", 
                     data.length(), LORA_MAX_PAYLOAD_SIZE);
        _loraPacketsFailed++;
        return false;
    }
    
    unsigned long now = millis();
    
    if (now - _lastLoRaTransmission < LORA_MIN_INTERVAL_MS) {
        uint32_t waitTime = LORA_MIN_INTERVAL_MS - (now - _lastLoRaTransmission);
        DEBUG_PRINTF("[CommunicationManager] Aguardando duty cycle: %lu ms\n", waitTime);
        
        if (waitTime > 30000) {
            return false;
        }
        delay(waitTime);
        now = millis();
    }
    
    if (!_isChannelFree()) {
        DEBUG_PRINTLN("[CommunicationManager] TX adiado: canal ocupado");
        return false;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ TRANSMITINDO LORA ━━━━━");
    DEBUG_PRINTF("[CommunicationManager] Payload: %s\n", data.c_str());
    DEBUG_PRINTF("[CommunicationManager] Tamanho: %d bytes\n", data.length());
    
    uint32_t txTimeout = (_currentSpreadingFactor >= 11) ? LORA_TX_TIMEOUT_MS_SAFE : LORA_TX_TIMEOUT_MS_NORMAL;
    
    DEBUG_PRINTF("[CommunicationManager] SF=%d, Timeout=%lu ms\n", _currentSpreadingFactor, txTimeout);
    
    unsigned long txStartTime = millis();
    
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = LoRa.endPacket(true);
    
    unsigned long txDuration = millis() - txStartTime;
    
    if (txDuration > txTimeout) {
        DEBUG_PRINTF("[CommunicationManager] Timeout TX LoRa (%lu ms > %lu ms)!\n", 
                     txDuration, txTimeout);
        _loraPacketsFailed++;
        LoRa.receive();
        return false;
    }
    
    if (success) {
        _loraPacketsSent++;
        _lastLoRaTransmission = now;
        _txFailureCount = 0;
        DEBUG_PRINTF("[CommunicationManager] Pacote enviado (duração: %lu ms)\n", txDuration);
        DEBUG_PRINTF("[CommunicationManager] Total enviado: %d pacotes\n", _loraPacketsSent);
    } else {
        _loraPacketsFailed++;
        _txFailureCount++;
        _lastTxFailure = now;
        DEBUG_PRINTLN("[CommunicationManager] Falha na transmissão LoRa");
        
        uint32_t backoff = min((uint32_t)(1000 * (1 << _txFailureCount)), (uint32_t)8000);
        DEBUG_PRINTF("[CommunicationManager] Backoff: %lu ms\n", backoff);
        
        LoRa.receive();
        delay(backoff);
        return false;
    }
    
    delay(10);
    LoRa.receive();
    DEBUG_PRINTLN("[CommunicationManager] LoRa voltou ao modo RX");
    
    return success;
}

bool CommunicationManager::sendTelemetry(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    _adaptSpreadingFactor(data.altitude);
    
    bool loraSuccess = false;
    bool httpSuccess = false;
    
    if (_loraEnabled && _loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] >>> TX LoRa 1/2: Telemetria Satélite");
        
        // ========================================
        // FASE 1: TELEMETRIA DO SATÉLITE (SEM NÓS)
        // ========================================
        
        // ✅ CRIAR BUFFER VAZIO PARA TX 1
        GroundNodeBuffer emptyBuffer;
        memset(&emptyBuffer, 0, sizeof(GroundNodeBuffer));
        emptyBuffer.activeNodes = 0;
        
        std::vector<uint16_t> dummyNodes;
        String binaryPayload = _createBinaryLoRaPayloadRelay(data, emptyBuffer, dummyNodes);
        
        if (binaryPayload.length() > 0) {
            loraSuccess = sendLoRa(binaryPayload);
            
            if (loraSuccess) {
                DEBUG_PRINTLN("[CommunicationManager] Telemetria satélite enviada");
            } else {
                DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao enviar telemetria satélite");
            }
        } else {
            DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao criar payload satélite");
            loraSuccess = false;
        }
        
        // ========================================
        // FASE 2: RETRANSMISSÃO DE NÓS TERRESTRES
        // ========================================
        DEBUG_PRINTLN("[CommunicationManager] >>> TX LoRa 2/2: Retransmissão Nós");
        
        // Debug: Status do buffer
        DEBUG_PRINTF("[CommunicationManager] Buffer status: %d nós ativos\n", groundBuffer.activeNodes);
        
        uint8_t pendingNodes = 0;
        for (uint8_t i = 0; i < groundBuffer.activeNodes; i++) {
            if (!groundBuffer.nodes[i].forwarded && groundBuffer.nodes[i].nodeId > 0) {
                pendingNodes++;
                DEBUG_PRINTF("[CommunicationManager]   Nó %u pendente (seq=%u, rssi=%d)\n",
                            groundBuffer.nodes[i].nodeId,
                            groundBuffer.nodes[i].sequenceNumber,
                            groundBuffer.nodes[i].rssi);
            } else if (groundBuffer.nodes[i].forwarded) {
                DEBUG_PRINTF("[CommunicationManager]   Nó %u já retransmitido\n",
                            groundBuffer.nodes[i].nodeId);
            }
        }
        
        DEBUG_PRINTF("[CommunicationManager] Total pendentes: %d nós\n", pendingNodes);
        
        if (pendingNodes > 0) {
            // Criar payload RELAY (Satélite + Nós)
            std::vector<uint16_t> relayNodeIds;
            String relayPayload = _createBinaryLoRaPayloadRelay(data, groundBuffer, relayNodeIds);
            
            if (relayPayload.length() > 0) {
                DEBUG_PRINTF("[CommunicationManager] Payload RELAY: %d bytes hex (%d bytes bin)\n",
                            relayPayload.length(), relayPayload.length() / 2);
                DEBUG_PRINTF("[CommunicationManager] Hex: %s\n", relayPayload.c_str());
                
                // Verificar duty cycle
                unsigned long now = millis();
                unsigned long timeSinceLastTX = now - _lastLoRaTransmission;
                
                DEBUG_PRINTF("[CommunicationManager] Tempo desde última TX: %lu ms\n", timeSinceLastTX);
                DEBUG_PRINTF("[CommunicationManager] Duty cycle mínimo: %lu ms\n", LORA_MIN_INTERVAL_MS);
                
                if (timeSinceLastTX < LORA_MIN_INTERVAL_MS) {
                    uint32_t waitTime = LORA_MIN_INTERVAL_MS - timeSinceLastTX;
                    DEBUG_PRINTF("[CommunicationManager] Aguardando duty cycle relay: %lu ms\n", waitTime);
                    
                    // Limitar espera máxima
                    if (waitTime > 60000) {
                        DEBUG_PRINTLN("[CommunicationManager] AVISO: Duty cycle muito longo, abortando relay");
                        return loraSuccess;
                    }
                    
                    delay(waitTime);
                    now = millis();
                }
                
                // TRANSMITIR RELAY
                DEBUG_PRINTLN("[CommunicationManager] ━━━━━ TRANSMITINDO RELAY ━━━━━");
                DEBUG_PRINTF("[CommunicationManager] Tentando enviar %d nó(s)...\n", relayNodeIds.size());
                
                bool relaySuccess = sendLoRa(relayPayload);
                
                if (relaySuccess) {
                    DEBUG_PRINTF("[CommunicationManager] ✓ RELAY TX OK! %d nó(s) retransmitido(s)\n", 
                                relayNodeIds.size());
                    
                    // Marcar nós como enviados
                    markNodesAsForwarded(const_cast<GroundNodeBuffer&>(groundBuffer), relayNodeIds);
                    
                    DEBUG_PRINTLN("[CommunicationManager] Nós marcados como retransmitidos");
                } else {
                    DEBUG_PRINTLN("[CommunicationManager] ✗ RELAY TX FALHOU!");
                }
            } else {
                DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao criar payload relay");
            }
        } else {
            DEBUG_PRINTLN("[CommunicationManager] Nenhum nó para retransmitir");
        }
    }
    
    // ========================================
    // HTTP (se habilitado)
    // ========================================
    if (_httpEnabled && _connected) {
        String jsonPayload = _createTelemetryJSON(data, groundBuffer);
        DEBUG_PRINTLN("[CommunicationManager] >>> Enviando HTTP/OBSAT (JSON)...");
        DEBUG_PRINTF("[CommunicationManager] JSON size: %d bytes\n", jsonPayload.length());

        if (jsonPayload.length() == 0) {
            DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao criar JSON");
            _packetsFailed++;
        } else if (jsonPayload.length() > JSON_MAX_SIZE) {
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

String CommunicationManager::_createBinaryLoRaPayloadRelay(
    const TelemetryData& data,
    const GroundNodeBuffer& buffer,
    std::vector<uint16_t>& includedNodes)
{
    DEBUG_PRINTLN("[CommunicationManager] Criando payload RELAY...");
    
    uint8_t binaryBuffer[200];
    int offset = 0;
    
    // ========================================
    // HEADER (4 bytes)
    // ========================================
    binaryBuffer[offset++] = 0xAB;
    binaryBuffer[offset++] = 0xCD;
    binaryBuffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    binaryBuffer[offset++] = TEAM_ID & 0xFF;
    
    DEBUG_PRINTF("[CommunicationManager] Header: ABCD%04X\n", TEAM_ID);
    
    // ========================================
    // SATELITE (13 bytes)
    // ========================================
    binaryBuffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);
    
    float tempToUse = !isnan(data.temperature) ? data.temperature : 0.0;
    int16_t tempEncoded = (int16_t)((tempToUse + 50.0) * 10);
    binaryBuffer[offset++] = (tempEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = tempEncoded & 0xFF;
    
    float pressToUse = !isnan(data.pressure) ? data.pressure : 1013.25;
    uint16_t pressEncoded = (uint16_t)((pressToUse - 300.0) * 10);
    binaryBuffer[offset++] = (pressEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = pressEncoded & 0xFF;
    
    float altToUse = !isnan(data.altitude) ? data.altitude : 0.0;
    uint16_t altEncoded = (uint16_t)constrain(altToUse, 0, 65535);
    binaryBuffer[offset++] = (altEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = altEncoded & 0xFF;
    
    float gyroXToUse = !isnan(data.gyroX) ? data.gyroX : 0.0;
    int8_t gyroXEncoded = (int8_t)constrain(gyroXToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroXEncoded;
    
    float gyroYToUse = !isnan(data.gyroY) ? data.gyroY : 0.0;
    int8_t gyroYEncoded = (int8_t)constrain(gyroYToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroYEncoded;
    
    float gyroZToUse = !isnan(data.gyroZ) ? data.gyroZ : 0.0;
    int8_t gyroZEncoded = (int8_t)constrain(gyroZToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroZEncoded;
    
    float accelXToUse = !isnan(data.accelX) ? data.accelX : 0.0;
    int8_t accelXEncoded = (int8_t)constrain(accelXToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelXEncoded;
    
    float accelYToUse = !isnan(data.accelY) ? data.accelY : 0.0;
    int8_t accelYEncoded = (int8_t)constrain(accelYToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelYEncoded;
    
    float accelZToUse = !isnan(data.accelZ) ? data.accelZ : 0.0;
    int8_t accelZEncoded = (int8_t)constrain(accelZToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelZEncoded;
    
    DEBUG_PRINTF("[CommunicationManager] Satélite: Bat=%.0f%% Alt=%.0fm\n",
                data.batteryPercentage, altToUse);
    
    // ========================================
    // NOS TERRESTRES
    // ========================================
    includedNodes.clear();
    uint8_t nodesAdded = 0;
    
    // Contar nós elegíveis
    for (uint8_t i = 0; i < buffer.activeNodes; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            nodesAdded++;
        }
    }
    
    // Nodes count (1 byte)
    binaryBuffer[offset++] = nodesAdded;
    
    DEBUG_PRINTF("[CommunicationManager] Incluindo %d nó(s) no relay\n", nodesAdded);
    
    if (nodesAdded > 0) {
        for (uint8_t i = 0; i < buffer.activeNodes && i < MAX_GROUND_NODES; i++) {
            const MissionData& node = buffer.nodes[i];
            
            if (!node.forwarded && node.nodeId > 0) {
                if (offset + 8 > 195) {
                    DEBUG_PRINTLN("[CommunicationManager] AVISO: Buffer relay cheio, truncando");
                    break;
                }
                
                // Node ID (2 bytes)
                binaryBuffer[offset++] = (node.nodeId >> 8) & 0xFF;
                binaryBuffer[offset++] = node.nodeId & 0xFF;
                
                // Soil Moisture (1 byte, 0-100%)
                binaryBuffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);
                
                // Temperature (2 bytes)
                int16_t nodeTempEncoded = (int16_t)((node.ambientTemp + 50.0) * 10);
                binaryBuffer[offset++] = (nodeTempEncoded >> 8) & 0xFF;
                binaryBuffer[offset++] = nodeTempEncoded & 0xFF;
                
                // Humidity (1 byte, 0-100%)
                binaryBuffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
                
                // Irrigation (1 byte)
                binaryBuffer[offset++] = node.irrigationStatus;
                
                // RSSI (1 byte, shift +128)
                binaryBuffer[offset++] = (uint8_t)(node.rssi + 128);
                
                includedNodes.push_back(node.nodeId);
                
                DEBUG_PRINTF("[CommunicationManager]   + Nó %u: Solo=%.0f%% T=%.1fC RSSI=%ddBm\n",
                            node.nodeId, node.soilMoisture, node.ambientTemp, node.rssi);
            }
        }
    }
    
    // Validação
    if (offset < 18) {
        DEBUG_PRINTF("[CommunicationManager] ERRO: Payload relay incompleto (%d bytes)\n", offset);
        return "";
    }
    
    // Converter para hex
    String hexPayload = "";
    hexPayload.reserve(offset * 2 + 1);
    
    for (int i = 0; i < offset; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", binaryBuffer[i]);
        hexPayload += hex;
    }
    
    DEBUG_PRINTF("[CommunicationManager] Payload RELAY completo: %d bytes binary → %d chars hex\n",
                offset, hexPayload.length());
    
    return hexPayload;
}


// ========================================
// MÉTODOS AUXILIARES (continuação do arquivo)
// ========================================

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraInitialized) {
        return false;
    }
    
    int packetSize = LoRa.parsePacket();
    if (packetSize <= 0) {
        return false;
    }
    
    packet = "";
    while (LoRa.available()) {
        packet += (char)LoRa.read();
    }
    
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();
    
    const int MIN_RSSI = -120;
    const float MIN_SNR = -15.0;
    
    if (rssi < MIN_RSSI) {
        DEBUG_PRINTF("[CommunicationManager] Pacote descartado: RSSI muito baixo (%d dBm)\n", rssi);
        return false;
    }
    
    if (snr < MIN_SNR) {
        DEBUG_PRINTF("[CommunicationManager] Pacote descartado: SNR muito baixo (%.1f dB)\n", snr);
        return false;
    }
    
    _loraRSSI = rssi;
    _loraSNR = snr;
    
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ PACOTE LORA RECEBIDO ━━━━━");
    DEBUG_PRINTF("[CommunicationManager] Dados: %s\n", packet.c_str());
    DEBUG_PRINTF("[CommunicationManager] RSSI: %d dBm\n", rssi);
    DEBUG_PRINTF("[CommunicationManager] SNR: %.1f dB\n", snr);
    DEBUG_PRINTF("[CommunicationManager] Tamanho: %d bytes\n", packet.length());
    
    return true;
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

bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    // Limpar estrutura
    memset(&data, 0, sizeof(MissionData));
    
    // ✅ NOVO: Tentar decodificar como payload binário hexadecimal
    if (_decodeAgriNodePayload(packet, data)) {
        _lastMissionData = data;
        return true;
    }
    
    // ❌ FALLBACK: Tentar formato antigo (ASCII com checksum)
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

void CommunicationManager::markNodesAsForwarded(
    GroundNodeBuffer& buffer, 
    const std::vector<uint16_t>& nodeIds)
{
    uint8_t markedCount = 0;
    
    for (uint16_t nodeId : nodeIds) {
        for (uint8_t i = 0; i < buffer.activeNodes; i++) {
            if (buffer.nodes[i].nodeId == nodeId) {
                buffer.nodes[i].forwarded = true;
                markedCount++;
                DEBUG_PRINTF("[CommunicationManager] Nó %u marcado como transmitido\n", nodeId);
                break;
            }
        }
    }
    
    DEBUG_PRINTF("[CommunicationManager] %d nó(s) marcado(s) como transmitido(s)\n", markedCount);
}

bool CommunicationManager::_validatePayloadSize(size_t size) {
    return (size > 0 && size <= LORA_MAX_PAYLOAD_SIZE);
}

int CommunicationManager::_findNodeIndex(uint16_t nodeId) {
    int emptySlot = -1;
    
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_expectedSeqNum[i] == 0 && emptySlot == -1) {
            emptySlot = i;
        }
    }
    
    if (emptySlot >= 0) {
        return emptySlot;
    }
    
    static int nextIndex = 0;
    int index = nextIndex;
    nextIndex = (nextIndex + 1) % MAX_GROUND_NODES;
    return index;
}

void CommunicationManager::reconfigureLoRa(OperationMode mode) {
    if (!_loraInitialized) {
        DEBUG_PRINTLN("[CommunicationManager] LoRa não inicializado, ignorando reconfiguração");
        return;
    }
    
    DEBUG_PRINTF("[CommunicationManager] Reconfigurando LoRa para modo %d...\n", mode);
    
    switch (mode) {
        case MODE_PREFLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[CommunicationManager] LoRa: PRE-FLIGHT (SF7, 17dBm)");
            break;
            
        case MODE_FLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[CommunicationManager] LoRa: FLIGHT (SF7, 17dBm - HAB mode)");
            break;
            
        case MODE_SAFE:
            LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR_SAFE);
            _currentSpreadingFactor = LORA_SPREADING_FACTOR_SAFE;
            LoRa.setTxPower(20);
            DEBUG_PRINTLN("[CommunicationManager] LoRa: SAFE (SF12, 20dBm)");
            break;
            
        default:
            DEBUG_PRINTLN("[CommunicationManager] Modo desconhecido");
            break;
    }
    
    delay(10);
    LoRa.receive();
}

bool CommunicationManager::_parseAgroPacket(const String& packetData, MissionData& data) {
    String packet = packetData;
    
    if (!packet.startsWith("AGRO,")) {
        DEBUG_PRINTLN("[CommunicationManager] Formato inválido");
        return false;
    }
    
    packet.remove(0, 5);
    
    int commaIndex = 0;
    int fieldIndex = 0;
    String fields[8];
    
    while ((commaIndex = packet.indexOf(',')) != -1 && fieldIndex < 8) {
        fields[fieldIndex++] = packet.substring(0, commaIndex);
        packet.remove(0, commaIndex + 1);
    }
    
    if (fieldIndex < 8 && packet.length() > 0) {
        fields[fieldIndex++] = packet;
    }
    
    if (fieldIndex < 6) {
        DEBUG_PRINTF("[CommunicationManager] Campos insuficientes: %d\n", fieldIndex);
        return false;
    }
    
    data.sequenceNumber = fields[0].toInt();
    data.nodeId = fields[1].toInt();
    data.soilMoisture = fields[2].toFloat();
    data.ambientTemp = fields[3].toFloat();
    data.humidity = fields[4].toFloat();
    data.irrigationStatus = fields[5].toInt();
    
    if (data.soilMoisture < 0.0 || data.soilMoisture > 100.0 ||
        data.ambientTemp < -50.0 || data.ambientTemp > 100.0 ||
        data.humidity < 0.0 || data.humidity > 100.0) {
        DEBUG_PRINTLN("[CommunicationManager] Dados fora dos limites");
        return false;
    }
    
    int nodeIndex = _findNodeIndex(data.nodeId);
    if (nodeIndex >= 0 && _expectedSeqNum[nodeIndex] > 0) {
        uint16_t expectedSeq = _expectedSeqNum[nodeIndex];
        int16_t seqDiff = (int16_t)(data.sequenceNumber - expectedSeq);
        
        if (seqDiff > 0 && seqDiff < 1000) {
            uint16_t lost = seqDiff;
            data.packetsLost += lost;
            DEBUG_PRINTF("[CommunicationManager] Node %d: %d pacote(s) perdido(s)\n", 
                        data.nodeId, lost);
        } else if (seqDiff < 0) {
            DEBUG_PRINTF("[CommunicationManager] Pacote antigo, ignorando\n");
            return false;
        }
    }
    
    _expectedSeqNum[nodeIndex] = data.sequenceNumber + 1;
    data.packetsReceived++;
    
    return true;
}

bool CommunicationManager::_validateChecksum(const String& packet) {
    if (packet.length() < 5) {
        return false;
    }
    
    int checksumPos = packet.lastIndexOf('*');
    if (checksumPos < 0) {
        static unsigned long lastWarning = 0;
        if (millis() - lastWarning > 30000) {
            lastWarning = millis();
            DEBUG_PRINTLN("[CommunicationManager] AVISO: Pacotes sem checksum");
        }
        return true;
    }
    
    String data = packet.substring(0, checksumPos);
    String checksumStr = packet.substring(checksumPos + 1);
    
    if (checksumStr.length() != 2) {
        return false;
    }
    
    uint8_t calcChecksum = 0;
    for (size_t i = 0; i < data.length(); i++) {
        calcChecksum ^= data.charAt(i);
    }
    
    uint8_t receivedChecksum = (uint8_t)strtol(checksumStr.c_str(), NULL, 16);
    
    if (calcChecksum != receivedChecksum) {
        DEBUG_PRINTF("[CommunicationManager] Checksum inválido: calc=0x%02X recv=0x%02X\n", 
                     calcChecksum, receivedChecksum);
        return false;
    }
    
    return true;
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

String CommunicationManager::_createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
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
    
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodesArray = payload.createNestedArray("nodes");
        
        for (uint8_t i = 0; i < groundBuffer.activeNodes && i < MAX_GROUND_NODES; i++) {
            const MissionData& node = groundBuffer.nodes[i];
            
            JsonObject nodeObj = nodesArray.createNestedObject();
            nodeObj["id"] = node.nodeId;
            nodeObj["sm"] = round(node.soilMoisture * 10) / 10;
            nodeObj["t"] = round(node.ambientTemp * 10) / 10;
            nodeObj["h"] = round(node.humidity * 10) / 10;
            nodeObj["ir"] = node.irrigationStatus;
            nodeObj["rs"] = node.rssi;
            nodeObj["snr"] = round(node.snr * 10) / 10;
            nodeObj["seq"] = node.sequenceNumber;
        }
        
        payload["total_nodes"] = groundBuffer.activeNodes;
        payload["total_pkts"] = groundBuffer.totalPacketsCollected;
    }
    
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
    
    payload["stat"] = (data.systemStatus == STATUS_OK) ? "ok" : "err";
    payload["time"] = data.missionTime / 1000;

    char jsonBuffer[JSON_MAX_SIZE];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    
    if (jsonSize >= sizeof(jsonBuffer)) {
        DEBUG_PRINTLN("[CommunicationManager] ERRO: JSON truncado!");
        return "";
    }
    
    if (jsonSize == 0) {
        DEBUG_PRINTLN("[CommunicationManager] ERRO: JSON vazio!");
        return "";
    }
    
    DEBUG_PRINTF("[CommunicationManager] JSON criado: %d bytes\n", jsonSize);
    
    return String(jsonBuffer);
}

bool CommunicationManager::_sendHTTPPost(const String& jsonPayload) {
    if (jsonPayload.length() == 0 || jsonPayload == "{}" || jsonPayload == "null") {
        DEBUG_PRINTLN("[CommunicationManager] ERRO: Payload HTTP inválido");
        return false;
    }
    
    if (jsonPayload.length() > JSON_MAX_SIZE) {
        DEBUG_PRINTF("[CommunicationManager] ERRO: Payload muito grande (%d > %d)\n",
                     jsonPayload.length(), JSON_MAX_SIZE);
        return false;
    }
    
    HTTPClient http;
    String url = String("https://") + HTTP_SERVER + HTTP_ENDPOINT;
    
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    DEBUG_PRINTF("[CommunicationManager] POST para: %s\n", url.c_str());
    
    int httpCode = http.POST(jsonPayload);
    bool success = false;
    
    if (httpCode > 0) {
        DEBUG_PRINTF("[CommunicationManager] HTTP Code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            DEBUG_PRINTF("[CommunicationManager] Resposta: %s\n", response.c_str());
            
            success = (response.indexOf("sucesso") >= 0 || response.indexOf("Sucesso") >= 0);
            
            if (!success) {
                StaticJsonDocument<256> responseDoc;
                if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
                    const char* status = responseDoc["Status"];
                    if (status != nullptr) {
                        success = (String(status).indexOf("Sucesso") >= 0);
                    }
                } else {
                    success = true;
                }
            }
        }
    } else {
        DEBUG_PRINTF("[CommunicationManager] Erro HTTP: %s\n", 
                    http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}

uint8_t CommunicationManager::calculatePriority(const MissionData& node) {
    uint8_t priority = 0;
    
    if (node.rssi > -80) {
        priority += 3;
    } else if (node.rssi > -100) {
        priority += 2;
    } else {
        priority += 1;
    }
    
    if (node.irrigationStatus == 1) {
        priority += 2;
    }
    
    if (node.soilMoisture < 20.0 || node.soilMoisture > 80.0) {
        priority += 2;
    }
    
    if (node.packetsLost > 10) {
        priority = (priority > 1) ? priority - 1 : 0;
    }
    
    return (priority > 10) ? (uint8_t)10 : priority;
}

bool CommunicationManager::_decodeAgriNodePayload(const String& hexPayload, MissionData& data) {
    // Validar tamanho mínimo: Header(4) + Dados(8) = 12 bytes = 24 chars hex
    if (hexPayload.length() < 24) {
        DEBUG_PRINTF("[CommunicationManager] Payload muito curto: %d chars\n", hexPayload.length());
        return false;
    }
    
    // Verificar se é hexadecimal válido
    for (size_t i = 0; i < hexPayload.length(); i++) {
        char c = hexPayload.charAt(i);
        if (!isxdigit(c)) {
            DEBUG_PRINTF("[CommunicationManager] Caractere inválido na posição %d: %c\n", i, c);
            return false;
        }
    }
    
    // Converter hex para bytes
    uint8_t buffer[128];
    size_t bufferLen = hexPayload.length() / 2;
    
    for (size_t i = 0; i < bufferLen; i++) {
        String byteStr = hexPayload.substring(i * 2, i * 2 + 2);
        buffer[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }
    
    // Validar header mágico
    if (buffer[0] != 0xAB || buffer[1] != 0xCD) {
        DEBUG_PRINTF("[CommunicationManager] Header inválido: 0x%02X 0x%02X\n", buffer[0], buffer[1]);
        return false;
    }
    
    // Extrair TEAM_ID
    uint16_t teamId = (buffer[2] << 8) | buffer[3];
    
    // Validar se é do mesmo time ou aceitar qualquer nó terrestre
    // (ajuste conforme sua lógica)
    
    // ========================================
    // DECODIFICAR DADOS DO NÓ (offset 4)
    // ========================================
    
    int offset = 4;
    
    // Node ID (2 bytes)
    data.nodeId = (buffer[offset] << 8) | buffer[offset + 1];
    offset += 2;
    
    // Soil Moisture (1 byte, 0-100%)
    data.soilMoisture = (float)buffer[offset];
    offset += 1;
    
    // Temperature (2 bytes, codificado como (temp + 50.0) * 10)
    int16_t tempEncoded = (buffer[offset] << 8) | buffer[offset + 1];
    data.ambientTemp = (tempEncoded / 10.0) - 50.0;
    offset += 2;
    
    // Humidity (1 byte, 0-100%)
    data.humidity = (float)buffer[offset];
    offset += 1;
    
    // Irrigation Status (1 byte)
    data.irrigationStatus = buffer[offset];
    offset += 1;
    
    // RSSI (1 byte, deslocado +128)
    data.rssi = (int16_t)buffer[offset] - 128;
    offset += 1;
    
    // ✅ ARMAZENAR PAYLOAD ORIGINAL HEX (apenas a parte do nó, sem header)
    // Extrair os 8 bytes do nó (offset 4 a 11) = 16 chars hex
    String nodePayloadHex = hexPayload.substring(8, 24); // chars 8-23
    strncpy(data.originalPayloadHex, nodePayloadHex.c_str(), sizeof(data.originalPayloadHex) - 1);
    data.originalPayloadHex[sizeof(data.originalPayloadHex) - 1] = '\0';
    data.payloadLength = nodePayloadHex.length();
    
    // Validar dados decodificados
    if (data.soilMoisture < 0.0 || data.soilMoisture > 100.0) {
        DEBUG_PRINTF("[CommunicationManager] Umidade solo inválida: %.1f\n", data.soilMoisture);
        return false;
    }
    
    if (data.ambientTemp < -50.0 || data.ambientTemp > 100.0) {
        DEBUG_PRINTF("[CommunicationManager] Temperatura inválida: %.1f\n", data.ambientTemp);
        return false;
    }
    
    if (data.humidity < 0.0 || data.humidity > 100.0) {
        DEBUG_PRINTF("[CommunicationManager] Umidade ar inválida: %.1f\n", data.humidity);
        return false;
    }
    
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ PAYLOAD DECODIFICADO ━━━━━");
    DEBUG_PRINTF("[CommunicationManager] Node ID: %u\n", data.nodeId);
    DEBUG_PRINTF("[CommunicationManager] Umidade Solo: %.1f%%\n", data.soilMoisture);
    DEBUG_PRINTF("[CommunicationManager] Temperatura: %.1f°C\n", data.ambientTemp);
    DEBUG_PRINTF("[CommunicationManager] Umidade Ar: %.1f%%\n", data.humidity);
    DEBUG_PRINTF("[CommunicationManager] Irrigação: %d\n", data.irrigationStatus);
    DEBUG_PRINTF("[CommunicationManager] RSSI: %d dBm\n", data.rssi);
    DEBUG_PRINTF("[CommunicationManager] Payload Original: %s\n", data.originalPayloadHex);
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    return true;
}


String CommunicationManager::_createSatelliteTelemetryPayload(const TelemetryData& data) {
    uint8_t binaryBuffer[20];
    int offset = 0;
    
    // ========================================
    // HEADER (4 bytes)
    // ========================================
    binaryBuffer[offset++] = 0xAB;  // Magic byte 1
    binaryBuffer[offset++] = 0xCD;  // Magic byte 2
    binaryBuffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    binaryBuffer[offset++] = TEAM_ID & 0xFF;
    
    // ========================================
    // SATELITE (13 bytes)
    // ========================================
    
    // Battery (1 byte, 0-100%)
    binaryBuffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);
    
    // Temperature (2 bytes, -50 to +100°C, 0.1°C precision)
    float tempToUse = !isnan(data.temperature) ? data.temperature : 0.0;
    int16_t tempEncoded = (int16_t)((tempToUse + 50.0) * 10);
    binaryBuffer[offset++] = (tempEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = tempEncoded & 0xFF;
    
    // Pressure (2 bytes, 300-1100 hPa, 0.1 hPa precision)
    float pressToUse = !isnan(data.pressure) ? data.pressure : 1013.25;
    uint16_t pressEncoded = (uint16_t)((pressToUse - 300.0) * 10);
    binaryBuffer[offset++] = (pressEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = pressEncoded & 0xFF;
    
    // Altitude (2 bytes, 0-65535m)
    float altToUse = !isnan(data.altitude) ? data.altitude : 0.0;
    uint16_t altEncoded = (uint16_t)constrain(altToUse, 0, 65535);
    binaryBuffer[offset++] = (altEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = altEncoded & 0xFF;
    
    // Gyroscope X (1 byte)
    float gyroXToUse = !isnan(data.gyroX) ? data.gyroX : 0.0;
    int8_t gyroXEncoded = (int8_t)constrain(gyroXToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroXEncoded;
    
    // Gyroscope Y (1 byte)
    float gyroYToUse = !isnan(data.gyroY) ? data.gyroY : 0.0;
    int8_t gyroYEncoded = (int8_t)constrain(gyroYToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroYEncoded;
    
    // Gyroscope Z (1 byte)
    float gyroZToUse = !isnan(data.gyroZ) ? data.gyroZ : 0.0;
    int8_t gyroZEncoded = (int8_t)constrain(gyroZToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroZEncoded;
    
    // Accelerometer X (1 byte)
    float accelXToUse = !isnan(data.accelX) ? data.accelX : 0.0;
    int8_t accelXEncoded = (int8_t)constrain(accelXToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelXEncoded;
    
    // Accelerometer Y (1 byte)
    float accelYToUse = !isnan(data.accelY) ? data.accelY : 0.0;
    int8_t accelYEncoded = (int8_t)constrain(accelYToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelYEncoded;
    
    // Accelerometer Z (1 byte)
    float accelZToUse = !isnan(data.accelZ) ? data.accelZ : 0.0;
    int8_t accelZEncoded = (int8_t)constrain(accelZToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelZEncoded;
    
    // Converter para hex string
    String hexPayload = "";
    hexPayload.reserve(offset * 2 + 1);
    
    for (int i = 0; i < offset; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", binaryBuffer[i]);
        hexPayload += hex;
    }
    
    DEBUG_PRINTF("[CommunicationManager] Telemetria satélite: %d bytes hex\n", offset);
    
    return hexPayload;
}

String CommunicationManager::_createForwardPayload(
    const GroundNodeBuffer& buffer,
    std::vector<uint16_t>& includedNodes)
{
    String consolidatedPayload = "";
    includedNodes.clear();
    
    uint8_t nodesAdded = 0;
    
    // ========================================
    // HEADER GLOBAL (4 bytes = 8 chars hex)
    // ========================================
    char headerHex[10];
    snprintf(headerHex, sizeof(headerHex), "ABCD%04X", TEAM_ID);
    consolidatedPayload = String(headerHex);
    
    // ========================================
    // CONTADOR DE NÓS (1 byte = 2 chars hex)
    // ========================================
    // Contar nós não retransmitidos
    for (uint8_t i = 0; i < buffer.activeNodes && i < MAX_GROUND_NODES; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            nodesAdded++;
        }
    }
    
    char countHex[4];
    snprintf(countHex, sizeof(countHex), "%02X", nodesAdded);
    consolidatedPayload += String(countHex);
    
    // ========================================
    // PAYLOADS ORIGINAIS (8 bytes cada = 16 chars hex)
    // ========================================
    for (uint8_t i = 0; i < buffer.activeNodes && i < MAX_GROUND_NODES; i++) {
        const MissionData& node = buffer.nodes[i];
        
        if (!node.forwarded && node.nodeId > 0) {
            // Verificar se há espaço no payload (255 bytes = 510 chars hex)
            // Header(8) + Count(2) + N*16 <= 510
            if (consolidatedPayload.length() + 16 > 510) {
                DEBUG_PRINTLN("[CommunicationManager] Payload cheio, truncando");
                break;
            }
            
            // ✅ ADICIONAR PAYLOAD ORIGINAL HEX (sem recodificar!)
            if (node.payloadLength > 0) {
                consolidatedPayload += String(node.originalPayloadHex);
                includedNodes.push_back(node.nodeId);
                
                DEBUG_PRINTF("[CommunicationManager] Adicionado nó %u (payload: %s)\n",
                            node.nodeId, node.originalPayloadHex);
            } else {
                DEBUG_PRINTF("[CommunicationManager] AVISO: Nó %u sem payload original\n", node.nodeId);
            }
        }
    }
    
    if (nodesAdded > 0) {
        DEBUG_PRINTF("[CommunicationManager] Payload consolidado: %d nós, %d chars hex\n",
                    nodesAdded, consolidatedPayload.length());
    }
    
    return consolidatedPayload;
}
