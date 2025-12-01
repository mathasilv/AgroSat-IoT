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
    _lora(),
    _http(),
    _wifi(),              // ← NOVO
    _packetsSent(0),
    _packetsFailed(0),
    _totalRetries(0),
    _httpEnabled(true)
{
    memset(&_lastMissionData, 0, sizeof(MissionData));
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        _expectedSeqNum[i] = 0;
        _seqNodeId[i]      = 0;
    }
}

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] Inicializando DUAL MODE");

    bool loraOk = _lora.begin();
    bool wifiOk = _wifi.begin();  // ← usar WiFiService

    DEBUG_PRINTF("[CommunicationManager] LoRa=%s, WiFi=%s\n",
                 loraOk ? "OK" : "FALHOU",
                 wifiOk ? "OK" : "FALHOU");

    return loraOk || wifiOk;
}


bool CommunicationManager::initLoRa() {
    // Mantido por compatibilidade: delega para LoRaService
    return _lora.begin();
}

bool CommunicationManager::retryLoRaInit(uint8_t maxAttempts) {
    return _lora.retryInit(maxAttempts);
}

bool CommunicationManager::sendTelemetry(const TelemetryData& data,
                                         const GroundNodeBuffer& groundBuffer) {
    // Adapta SF pela altitude antes do TX, como antes
    _lora.adaptSpreadingFactor(data.altitude);

    bool loraSuccess = false;
    bool httpSuccess = false;

    // FASE 1: telemetria do satélite (sem nós)
    if (_lora.isOnline()) {
        DEBUG_PRINTLN("[CommunicationManager] TX LoRa [1/2] Telemetria Satelite");

        String satPayload = _createSatelliteTelemetryPayload(data);
        if (satPayload.length() > 0) {
            bool ok = _lora.send(satPayload);
            if (ok) {
                loraSuccess = true;
                DEBUG_PRINTLN("[CommunicationManager] Telemetria satelite enviada");
            } else {
                DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao enviar telemetria satelite");
            }
        } else {
            DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao criar payload satelite");
        }
    }

    // FASE 2: retransmissão de nós terrestres (relay)
    if (_lora.isOnline()) {
        DEBUG_PRINTLN("[CommunicationManager] TX LoRa [2/2] Retransmissao Nos");

        std::vector<uint16_t> relayNodeIds;
        String relayPayload = _createBinaryLoRaPayloadRelay(data, groundBuffer, relayNodeIds);

        // Se _createBinaryLoRaPayloadRelay não tiver nós, já retorna ""
        if (relayPayload.length() > 0) {
            DEBUG_PRINTF("[CommunicationManager] Payload RELAY: %d bytes hex (%d bytes bin)\n",
                         relayPayload.length(), relayPayload.length() / 2);

            bool relaySuccess = _lora.send(relayPayload);
            if (relaySuccess) {
                DEBUG_PRINTF("[CommunicationManager] RELAY TX OK! %d nos retransmitidos\n",
                             relayNodeIds.size());
                markNodesAsForwarded(const_cast<GroundNodeBuffer&>(groundBuffer), relayNodeIds);
            } else {
                DEBUG_PRINTLN("[CommunicationManager] RELAY TX FALHOU!");
            }
        } else {
            DEBUG_PRINTLN("[CommunicationManager] Nenhum no para retransmitir");
        }
    }

    // HTTP opcional (mesma lógica que você já tinha)
    if (_httpEnabled && _wifi.isConnected()) {

        String jsonPayload = _createTelemetryJSON(data, groundBuffer);
        DEBUG_PRINTLN("[CommunicationManager] Enviando HTTP OBSAT JSON...");
        DEBUG_PRINTF("[CommunicationManager] JSON size = %d bytes\n", jsonPayload.length());

        if (jsonPayload.length() == 0) {
            DEBUG_PRINTLN("[CommunicationManager] ERRO: Falha ao criar JSON");
            _packetsFailed++;
        } else if (jsonPayload.length() > JSON_MAX_SIZE) {
            DEBUG_PRINTF("[CommunicationManager] ERRO: JSON muito grande (%d > %d)\n",
                         jsonPayload.length(), JSON_MAX_SIZE);
            _packetsFailed++;
        } else {
            bool ok = false;
            for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
                if (attempt > 0) {
                    _totalRetries++;
                    DEBUG_PRINTF("[CommunicationManager] Tentativa %d/%d...\n",
                                 attempt + 1, WIFI_RETRY_ATTEMPTS);
                    delay(1000);
                }
                if (_http.postJson(jsonPayload)) {
                    _packetsSent++;
                    ok = true;
                    DEBUG_PRINTLN("[CommunicationManager] HTTP OK!");
                    break;
                }
            }
            if (!ok) {
                _packetsFailed++;
                DEBUG_PRINTLN("[CommunicationManager] HTTP falhou apos todas tentativas");
            }
            httpSuccess = ok;
        }
} else if (_httpEnabled && !_wifi.isConnected()) {
    DEBUG_PRINTLN("[CommunicationManager] HTTP habilitado mas WiFi offline");
}

    return loraSuccess || httpSuccess;
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

    float   tempToUse   = !isnan(data.temperature) ? data.temperature : 0.0;
    int16_t tempEncoded = (int16_t)((tempToUse + 50.0) * 10);
    binaryBuffer[offset++] = (tempEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = tempEncoded & 0xFF;

    float    pressToUse   = !isnan(data.pressure) ? data.pressure : 1013.25;
    uint16_t pressEncoded = (uint16_t)((pressToUse - 300.0) * 10);
    binaryBuffer[offset++] = (pressEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = pressEncoded & 0xFF;

    float    altToUse   = !isnan(data.altitude) ? data.altitude : 0.0;
    uint16_t altEncoded = (uint16_t)constrain(altToUse, 0, 65535);
    binaryBuffer[offset++] = (altEncoded >> 8) & 0xFF;
    binaryBuffer[offset++] = altEncoded & 0xFF;

    float  gyroXToUse   = !isnan(data.gyroX) ? data.gyroX : 0.0;
    int8_t gyroXEncoded = (int8_t)constrain(gyroXToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroXEncoded;

    float  gyroYToUse   = !isnan(data.gyroY) ? data.gyroY : 0.0;
    int8_t gyroYEncoded = (int8_t)constrain(gyroYToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroYEncoded;

    float  gyroZToUse   = !isnan(data.gyroZ) ? data.gyroZ : 0.0;
    int8_t gyroZEncoded = (int8_t)constrain(gyroZToUse / 2.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)gyroZEncoded;

    float  accelXToUse   = !isnan(data.accelX) ? data.accelX : 0.0;
    int8_t accelXEncoded = (int8_t)constrain(accelXToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelXEncoded;

    float  accelYToUse   = !isnan(data.accelY) ? data.accelY : 0.0;
    int8_t accelYEncoded = (int8_t)constrain(accelYToUse * 16.0, -127, 127);
    binaryBuffer[offset++] = (uint8_t)accelYEncoded;

    float  accelZToUse   = !isnan(data.accelZ) ? data.accelZ : 0.0;
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
    for (uint8_t i = 0; i < buffer.activeNodes && i < MAX_GROUND_NODES; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            nodesAdded++;
        }
    }

    // Se não há nenhum nó pendente, não gera relay
    if (nodesAdded == 0) {
        DEBUG_PRINTLN("[CommunicationManager] Nenhum nó pendente para relay");
        return "";
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

    // Validação mínima
    if (offset < 18) {
        DEBUG_PRINTF("[CommunicationManager] ERRO: Payload relay incompleto (%d bytes)\n", offset);
        return "";
    }

    // Converter para hex
    String hexPayload;
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
    return _lora.receive(packet, rssi, snr);
}


bool CommunicationManager::isLoRaOnline() {
    return _lora.isOnline();
}

int CommunicationManager::getLoRaRSSI() {
    return _lora.getLastRSSI();
}


float CommunicationManager::getLoRaSNR() {
    return _lora.getLastSNR();
}

void CommunicationManager::getLoRaStatistics(uint16_t& sent, uint16_t& failed) {
    _lora.getStatistics(sent, failed);
}
bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1) Tenta decodificar como payload binário AgriNode
    if (_decodeAgriNodePayload(packet, data)) {
        _lastMissionData = data;
        return true;
    }

    // 2) Fallback para formato ASCII antigo com checksum
    if (!_validateChecksum(packet)) {
        DEBUG_PRINTLN("[CommunicationManager] Checksum invalido");
        return false;
    }

    if (!_parseAgroPacket(packet, data)) {
        DEBUG_PRINTLN("[CommunicationManager] Falha ao parsear pacote agricola");
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

int CommunicationManager::_findNodeIndex(uint16_t nodeId) {
    int freeSlot = -1;

    // 1) procurar se o nodeId já está mapeado
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == nodeId) {
            return i;
        }
        if (_seqNodeId[i] == 0 && freeSlot == -1) {
            freeSlot = i;   // primeiro slot livre
        }
    }

    // 2) se ainda não existe, usar primeiro livre
    if (freeSlot >= 0) {
        _seqNodeId[freeSlot]      = nodeId;
        _expectedSeqNum[freeSlot] = 0;
        return freeSlot;
    }

    // 3) se não há livre, reaproveitar via round-robin
    static int nextIndex = 0;
    int index = nextIndex;
    nextIndex = (nextIndex + 1) % MAX_GROUND_NODES;

    _seqNodeId[index]      = nodeId;
    _expectedSeqNum[index] = 0;

    return index;
}


void CommunicationManager::reconfigureLoRa(OperationMode mode) {
    _lora.reconfigure(mode);
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
    _wifi.update();  // ← delegar para WiFiService
}

bool CommunicationManager::connectWiFi() {
    return _wifi.connect();
}

void CommunicationManager::disconnectWiFi() {
    _wifi.disconnect();
}

void CommunicationManager::enableLoRa(bool enable) {
    _lora.enable(enable);
}
void CommunicationManager::enableHTTP(bool enable) {
    _httpEnabled = enable;
    DEBUG_PRINTF("[CommunicationManager] HTTP %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

bool CommunicationManager::isConnected() {
    return _wifi.isConnected();
}

int8_t CommunicationManager::getRSSI() {
    return _wifi.getRSSI();
}

String CommunicationManager::getIPAddress() {
    return _wifi.getIPAddress();
}

void CommunicationManager::getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries) {
    sent = _packetsSent;
    failed = _packetsFailed;
    retries = _totalRetries;
}

bool CommunicationManager::testConnection() {
    if (!_wifi.isConnected()) return false;

    HTTPClient http;
    http.begin("https://obsat.org.br/testepost/index.php");
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    return httpCode == 200;
}

String CommunicationManager::_createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    StaticJsonDocument<JSON_MAX_SIZE> doc;

    // === DADOS SATÉLITE ===
    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    
    // ✅ sprintf "%.2f" - 2 DECIMAIS GARANTIDAS
    char tempStr[8]; 
    sprintf(tempStr, "%.2f", isnan(data.temperature) ? 0.0f : data.temperature);
    if (atof(tempStr) < TEMP_MIN_VALID || atof(tempStr) > TEMP_MAX_VALID) 
        strcpy(tempStr, "0.00");
    doc["temperatura"] = tempStr;
    
    char pressStr[8]; 
    sprintf(pressStr, "%.2f", isnan(data.pressure) ? 1013.25f : data.pressure);
    if (atof(pressStr) < PRESSURE_MIN_VALID || atof(pressStr) > PRESSURE_MAX_VALID) 
        strcpy(pressStr, "0.00");
    doc["pressao"] = pressStr;

    // Giroscópio (3 strings)
    char gyroX[8]; sprintf(gyroX, "%.2f", isnan(data.gyroX) ? 0.0f : data.gyroX);
    char gyroY[8]; sprintf(gyroY, "%.2f", isnan(data.gyroY) ? 0.0f : data.gyroY);
    char gyroZ[8]; sprintf(gyroZ, "%.2f", isnan(data.gyroZ) ? 0.0f : data.gyroZ);
    
    JsonArray giroscopio = doc.createNestedArray("giroscopio");
    giroscopio.add(gyroX); giroscopio.add(gyroY); giroscopio.add(gyroZ);

    // Acelerômetro (3 strings)
    char accelX[8]; sprintf(accelX, "%.2f", isnan(data.accelX) ? 0.0f : data.accelX);
    char accelY[8]; sprintf(accelY, "%.2f", isnan(data.accelY) ? 0.0f : data.accelY);
    char accelZ[8]; sprintf(accelZ, "%.2f", isnan(data.accelZ) ? 0.0f : data.accelZ);
    
    JsonArray acelerometro = doc.createNestedArray("acelerometro");
    acelerometro.add(accelX); acelerometro.add(accelY); acelerometro.add(accelZ);

    // === NÓS TERRESTRES ===
    JsonObject payload = doc.createNestedObject("payload");
    
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodesArray = payload.createNestedArray("nodes");
        
        for (uint8_t i = 0; i < groundBuffer.activeNodes && i < MAX_GROUND_NODES; i++) {
            const MissionData& node = groundBuffer.nodes[i];
            
            JsonObject nodeObj = nodesArray.createNestedObject();
            nodeObj["id"] = node.nodeId;
            
            char smStr[8]; sprintf(smStr, "%.2f", node.soilMoisture);
            char tStr[8]; sprintf(tStr, "%.2f", node.ambientTemp);
            char hStr[8]; sprintf(hStr, "%.2f", node.humidity);
            char snrStr[8]; sprintf(snrStr, "%.2f", node.snr);
            
            nodeObj["sm"] = smStr;
            nodeObj["t"] = tStr;
            nodeObj["h"] = hStr;
            nodeObj["ir"] = node.irrigationStatus;
            nodeObj["rs"] = node.rssi;
            nodeObj["snr"] = snrStr;
            nodeObj["seq"] = node.sequenceNumber;
        }
        
        payload["total_nodes"] = groundBuffer.activeNodes;
        payload["total_pkts"] = groundBuffer.totalPacketsCollected;
    }

    // === CAMPOS OPCIONAIS ===
    if (!isnan(data.altitude) && data.altitude >= 0) {
        char altStr[8]; sprintf(altStr, "%.2f", data.altitude);
        payload["alt"] = altStr;
    }
    
    if (!isnan(data.humidity) && data.humidity >= HUMIDITY_MIN_VALID && data.humidity <= HUMIDITY_MAX_VALID) {
        char humStr[8]; sprintf(humStr, "%.2f", data.humidity);
        payload["hum"] = humStr;
    }
    
    if (!isnan(data.co2) && data.co2 >= CO2_MIN_VALID && data.co2 <= CO2_MAX_VALID) {
        payload["co2"] = (int)roundf(data.co2);
    }
    
    if (!isnan(data.tvoc) && data.tvoc >= TVOC_MIN_VALID && data.tvoc <= TVOC_MAX_VALID) {
        payload["tvoc"] = (int)roundf(data.tvoc);
    }
    
    payload["stat"] = (data.systemStatus == STATUS_OK) ? "ok" : "err";
    payload["time"] = data.missionTime / 1000;

    // === SERIALIZE SIMPLES ===
    char jsonBuffer[JSON_MAX_SIZE];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    
    // ✅ DEBUG (remover após teste)
    DEBUG_PRINTLN("=== JSON 2 DECIMAIS ===");
    DEBUG_PRINTF("Tamanho: %d bytes\n", jsonSize);
    DEBUG_PRINTF("JSON: %s\n", jsonBuffer);
    DEBUG_PRINTLN("======================");

    if (jsonSize >= sizeof(jsonBuffer) || jsonSize == 0) {
        DEBUG_PRINTLN("ERRO: JSON inválido!");
        return "";
    }
    
    return String(jsonBuffer);
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

bool CommunicationManager::sendLoRa(const String& data) {
    return _lora.send(data);
}
