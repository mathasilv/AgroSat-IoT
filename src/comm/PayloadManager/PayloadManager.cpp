/**
 * @file PayloadManager.cpp
 * @brief Implementação do gerenciador de payloads
 */

#include "PayloadManager.h"

PayloadManager::PayloadManager() :
    _packetsReceived(0),
    _packetsLost(0),
    _lastPacketTime(0),
    _packetTimeout(60000)
{
    memset(&_lastMissionData, 0, sizeof(MissionData));
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        _expectedSeqNum[i] = 0;
        _seqNodeId[i] = 0;
    }
}

void PayloadManager::update() {
    uint32_t currentTime = millis();
    
    // Verificar timeout de pacotes
    if (_lastPacketTime > 0 && (currentTime - _lastPacketTime) > _packetTimeout) {
        // Detectar pacotes perdidos baseado na sequência esperada
        uint16_t expected = 0;
        for (int i = 0; i < MAX_GROUND_NODES; i++) {
            if (_expectedSeqNum[i] > expected) {
                expected = _expectedSeqNum[i];
            }
        }
        _packetsLost = expected - _packetsReceived;
        
        DEBUG_PRINTF("[PayloadManager] Timeout detectado: %d pacotes perdidos\n", _packetsLost);
        _lastPacketTime = 0;
    }
}

// ========================================
// CONSTRUÇÃO - LoRa
// ========================================

String PayloadManager::createSatellitePayload(const TelemetryData& data) {
    uint8_t buffer[20];
    int offset = 0;

    // Header (4 bytes)
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    // Satélite (13 bytes)
    _encodeSatelliteData(data, buffer, offset);

    DEBUG_PRINTF("[PayloadManager] Satélite payload: %d bytes\n", offset);
    return _binaryToHex(buffer, offset);
}

String PayloadManager::createRelayPayload(const TelemetryData& data,
                                         const GroundNodeBuffer& buffer,
                                         std::vector<uint16_t>& includedNodes) {
    uint8_t binaryBuffer[200];
    int offset = 0;

    // Header
    binaryBuffer[offset++] = 0xAB;
    binaryBuffer[offset++] = 0xCD;
    binaryBuffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    binaryBuffer[offset++] = TEAM_ID & 0xFF;

    // Satélite
    _encodeSatelliteData(data, binaryBuffer, offset);

    // Contar nós elegíveis
    uint8_t nodesAdded = 0;
    for (uint8_t i = 0; i < buffer.activeNodes; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            nodesAdded++;
        }
    }

    if (nodesAdded == 0) {
        DEBUG_PRINTLN("[PayloadManager] Nenhum nó para relay");
        return "";
    }

    // Nodes count
    binaryBuffer[offset++] = nodesAdded;

    // Nós terrestres
    includedNodes.clear();
    for (uint8_t i = 0; i < buffer.activeNodes; i++) {
        const MissionData& node = buffer.nodes[i];
        
        if (!node.forwarded && node.nodeId > 0) {
            if (offset + 8 > 195) {
                DEBUG_PRINTLN("[PayloadManager] Buffer relay cheio");
                break;
            }
            
            _encodeNodeData(node, binaryBuffer, offset);
            includedNodes.push_back(node.nodeId);
        }
    }

    if (offset < 18) {
        return "";
    }

    String hexPayload = _binaryToHex(binaryBuffer, offset);
    DEBUG_PRINTF("[PayloadManager] Relay: %d nós, %d bytes\n", includedNodes.size(), offset);
    
    return hexPayload;
}

String PayloadManager::createForwardPayload(const GroundNodeBuffer& buffer,
                                           std::vector<uint16_t>& includedNodes) {
    String payload = "";
    
    // Header
    char headerHex[10];
    snprintf(headerHex, sizeof(headerHex), "ABCD%04X", TEAM_ID);
    payload = String(headerHex);

    // Contar nós
    uint8_t nodesAdded = 0;
    for (uint8_t i = 0; i < buffer.activeNodes; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            nodesAdded++;
        }
    }

    // Nodes count
    char countHex[4];
    snprintf(countHex, sizeof(countHex), "%02X", nodesAdded);
    payload += String(countHex);

    // Payloads originais
    includedNodes.clear();
    for (uint8_t i = 0; i < buffer.activeNodes; i++) {
        const MissionData& node = buffer.nodes[i];
        
        if (!node.forwarded && node.nodeId > 0) {
            if (payload.length() + 16 > 510) {
                break;
            }
            
            if (node.payloadLength > 0) {
                payload += String(node.originalPayloadHex);
                includedNodes.push_back(node.nodeId);
            }
        }
    }

    return payload;
}

// ========================================
// CONSTRUÇÃO - HTTP/JSON
// ========================================

String PayloadManager::createTelemetryJSON(const TelemetryData& data,
                                          const GroundNodeBuffer& groundBuffer) {
    StaticJsonDocument<JSON_MAX_SIZE> doc;

    _addSatelliteDataToJson(doc, data);

    JsonObject payload = doc.createNestedObject("payload");
    
    // Nós terrestres
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodesArray = payload.createNestedArray("nodes");
        
        for (uint8_t i = 0; i < groundBuffer.activeNodes; i++) {
            const MissionData& node = groundBuffer.nodes[i];
            
            JsonObject nodeObj = nodesArray.createNestedObject();
            nodeObj["id"] = node.nodeId;
            
            char smStr[8], tStr[8], hStr[8], snrStr[8];
            sprintf(smStr, "%.2f", node.soilMoisture);
            sprintf(tStr, "%.2f", node.ambientTemp);
            sprintf(hStr, "%.2f", node.humidity);
            sprintf(snrStr, "%.2f", node.snr);
            
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

    // Campos opcionais
    if (!isnan(data.altitude) && data.altitude >= 0) {
        char altStr[8];
        sprintf(altStr, "%.2f", data.altitude);
        payload["alt"] = altStr;
    }

    if (!isnan(data.humidity) && data.humidity >= HUMIDITY_MIN_VALID && data.humidity <= HUMIDITY_MAX_VALID) {
        char humStr[8];
        sprintf(humStr, "%.2f", data.humidity);
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

    char jsonBuffer[JSON_MAX_SIZE];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

    if (jsonSize >= sizeof(jsonBuffer) || jsonSize == 0) {
        return "";
    }

    return String(jsonBuffer);
}

// ========================================
// DECODIFICAÇÃO
// ========================================

bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // Tentar formato binário (novo)
    if (_decodeBinaryPayload(packet, data)) {
        _lastMissionData = data;
        _packetsReceived++;
        _lastPacketTime = millis();
        return true;
    }

    // Fallback formato ASCII (antigo)
    if (!_validateAsciiChecksum(packet)) {
        return false;
    }

    if (!_decodeAsciiPayload(packet, data)) {
        return false;
    }

    _lastMissionData = data;
    _packetsReceived++;
    _lastPacketTime = millis();
    return true;
}

void PayloadManager::markNodesAsForwarded(GroundNodeBuffer& buffer,
                                         const std::vector<uint16_t>& nodeIds) {
    uint8_t marked = 0;
    
    for (uint16_t nodeId : nodeIds) {
        for (uint8_t i = 0; i < buffer.activeNodes; i++) {
            if (buffer.nodes[i].nodeId == nodeId) {
                buffer.nodes[i].forwarded = true;
                marked++;
                break;
            }
        }
    }
    
    DEBUG_PRINTF("[PayloadManager] %d nós marcados como retransmitidos\n", marked);
}

uint8_t PayloadManager::calculateNodePriority(const MissionData& node) {
    uint8_t priority = 0;
    
    // RSSI
    if (node.rssi > -80) priority += 3;
    else if (node.rssi > -100) priority += 2;
    else priority += 1;
    
    // Irrigação ativa
    if (node.irrigationStatus == 1) priority += 2;
    
    // Umidade fora do ideal
    if (node.soilMoisture < 20.0 || node.soilMoisture > 80.0) priority += 2;
    
    // Penalidade por perdas
    if (node.packetsLost > 10) priority = (priority > 1) ? priority - 1 : 0;
    
    return (priority > 10) ? 10 : priority;
}

MissionData PayloadManager::getLastMissionData() const {
    return _lastMissionData;
}

void PayloadManager::getMissionStatistics(uint16_t& received, uint16_t& lost) const {
    received = _packetsReceived;
    lost = _packetsLost;
}

int PayloadManager::findNodeIndex(uint16_t nodeId) {
    // Procurar se já mapeado
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == nodeId) {
            return i;
        }
    }

    // Procurar slot livre
    int freeSlot = -1;
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == 0 && freeSlot == -1) {
            freeSlot = i;
        }
    }

    if (freeSlot >= 0) {
        _seqNodeId[freeSlot] = nodeId;
        _expectedSeqNum[freeSlot] = 0;
        return freeSlot;
    }

    // Round-robin se cheio
    static int nextIndex = 0;
    int index = nextIndex;
    nextIndex = (nextIndex + 1) % MAX_GROUND_NODES;
    
    _seqNodeId[index] = nodeId;
    _expectedSeqNum[index] = 0;
    
    return index;
}

// ========================================
// MÉTODOS PRIVADOS - DECODIFICAÇÃO
// ========================================

bool PayloadManager::_decodeBinaryPayload(const String& hexPayload, MissionData& data) {
    if (hexPayload.length() < 24) {
        return false;
    }

    // Validar hex
    for (size_t i = 0; i < hexPayload.length(); i++) {
        if (!isxdigit(hexPayload.charAt(i))) {
            return false;
        }
    }

    // Converter hex → bytes
    uint8_t buffer[128];
    size_t bufferLen = hexPayload.length() / 2;

    for (size_t i = 0; i < bufferLen; i++) {
        String byteStr = hexPayload.substring(i * 2, i * 2 + 2);
        buffer[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }

    // Validar header
    if (buffer[0] != 0xAB || buffer[1] != 0xCD) {
        return false;
    }

    // Decodificar nó (offset 4)
    int offset = 4;
    data.nodeId = (buffer[offset] << 8) | buffer[offset + 1];
    offset += 2;

    data.soilMoisture = (float)buffer[offset++];
    
    int16_t tempEncoded = (buffer[offset] << 8) | buffer[offset + 1];
    data.ambientTemp = (tempEncoded / 10.0) - 50.0;
    offset += 2;

    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;

    // Salvar payload original
    String nodeHex = hexPayload.substring(8, 24);
    strncpy(data.originalPayloadHex, nodeHex.c_str(), sizeof(data.originalPayloadHex) - 1);
    data.payloadLength = nodeHex.length();

    // Validar ranges
    if (data.soilMoisture < 0.0 || data.soilMoisture > 100.0 ||
        data.ambientTemp < -50.0 || data.ambientTemp > 100.0 ||
        data.humidity < 0.0 || data.humidity > 100.0) {
        return false;
    }

    // Atualizar sequência esperada
    int nodeIndex = findNodeIndex(data.nodeId);
    if (_expectedSeqNum[nodeIndex] > 0) {
        int16_t seqDiff = (int16_t)(data.sequenceNumber - _expectedSeqNum[nodeIndex]);
        if (seqDiff > 0 && seqDiff < 1000) {
            _packetsLost += seqDiff;
        }
    }
    _expectedSeqNum[nodeIndex] = data.sequenceNumber + 1;

    return true;
}

bool PayloadManager::_decodeAsciiPayload(const String& packet, MissionData& data) {
    String p = packet;

    if (!p.startsWith("AGRO,")) {
        return false;
    }

    p.remove(0, 5);

    int commaIndex = 0;
    int fieldIndex = 0;
    String fields[6];

    while ((commaIndex = p.indexOf(',')) != -1 && fieldIndex < 6) {
        fields[fieldIndex++] = p.substring(0, commaIndex);
        p.remove(0, commaIndex + 1);
    }
    if (fieldIndex < 6 && p.length() > 0) {
        fields[fieldIndex] = p;
    }

    if (fieldIndex < 4) {
        return false;
    }

    data.sequenceNumber = fields[0].toInt();
    data.nodeId = fields[1].toInt();
    data.soilMoisture = fields[2].toFloat();
    data.ambientTemp = fields[3].toFloat();
    if (fieldIndex > 4) data.humidity = fields[4].toFloat();
    if (fieldIndex > 5) data.irrigationStatus = fields[5].toInt();

    // Validar ranges
    if (data.soilMoisture < 0.0 || data.soilMoisture > 100.0 ||
        data.ambientTemp < -50.0 || data.ambientTemp > 100.0 ||
        (data.humidity < 0.0 && data.humidity > 0.0) || data.humidity > 100.0) {
        return false;
    }

    // Sequência
    int nodeIndex = findNodeIndex(data.nodeId);
    if (_expectedSeqNum[nodeIndex] > 0) {
        int16_t seqDiff = (int16_t)(data.sequenceNumber - _expectedSeqNum[nodeIndex]);
        if (seqDiff > 0 && seqDiff < 1000) {
            _packetsLost += seqDiff;
        }
    }
    _expectedSeqNum[nodeIndex] = data.sequenceNumber + 1;

    data.packetsReceived++;
    return true;
}

bool PayloadManager::_validateAsciiChecksum(const String& packet) {
    int checksumPos = packet.lastIndexOf('*');
    if (checksumPos < 0) {
        return true; // sem checksum
    }

    String data = packet.substring(0, checksumPos);
    String checksumStr = packet.substring(checksumPos + 1);

    if (checksumStr.length() != 2) {
        return false;
    }

    uint8_t calc = 0;
    for (size_t i = 0; i < data.length(); i++) {
        calc ^= data.charAt(i);
    }

    uint8_t received = (uint8_t)strtol(checksumStr.c_str(), NULL, 16);
    return calc == received;
}

// ========================================
// MÉTODOS PRIVADOS - CODIFICAÇÃO
// ========================================

void PayloadManager::_encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset) {
    buffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);

    float temp = !isnan(data.temperature) ? data.temperature : 0.0;
    int16_t tempEnc = (int16_t)((temp + 50.0) * 10);
    buffer[offset++] = (tempEnc >> 8) & 0xFF;
    buffer[offset++] = tempEnc & 0xFF;

    float press = !isnan(data.pressure) ? data.pressure : 1013.25;
    uint16_t pressEnc = (uint16_t)((press - 300.0) * 10);
    buffer[offset++] = (pressEnc >> 8) & 0xFF;
    buffer[offset++] = pressEnc & 0xFF;

    float alt = !isnan(data.altitude) ? data.altitude : 0.0;
    uint16_t altEnc = (uint16_t)constrain(alt, 0, 65535);
    buffer[offset++] = (altEnc >> 8) & 0xFF;
    buffer[offset++] = altEnc & 0xFF;

    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroX) ? data.gyroX : 0.0) / 2.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroY) ? data.gyroY : 0.0) / 2.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroZ) ? data.gyroZ : 0.0) / 2.0, -127, 127);

    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelX) ? data.accelX : 0.0) * 16.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelY) ? data.accelY : 0.0) * 16.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelZ) ? data.accelZ : 0.0) * 16.0, -127, 127);
}

void PayloadManager::_encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset) {
    buffer[offset++] = (node.nodeId >> 8) & 0xFF;
    buffer[offset++] = node.nodeId & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);

    int16_t tempEnc = (int16_t)((node.ambientTemp + 50.0) * 10);
    buffer[offset++] = (tempEnc >> 8) & 0xFF;
    buffer[offset++] = tempEnc & 0xFF;

    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    buffer[offset++] = node.irrigationStatus;
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

String PayloadManager::_binaryToHex(const uint8_t* buffer, size_t length) {
    String hex = "";
    hex.reserve(length * 2);

    for (size_t i = 0; i < length; i++) {
        char hexByte[3];
        snprintf(hexByte, sizeof(hexByte), "%02X", buffer[i]);
        hex += hexByte;
    }

    return hex;
}

// ========================================
// MÉTODOS PRIVADOS - JSON
// ========================================

void PayloadManager::_addSatelliteDataToJson(JsonDocument& doc, const TelemetryData& data) {
    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;

    char tempStr[16]; // Aumentado para 16
    snprintf(tempStr, sizeof(tempStr), "%.2f", isnan(data.temperature) ? 0.0f : data.temperature);
    if (atof(tempStr) < TEMP_MIN_VALID || atof(tempStr) > TEMP_MAX_VALID) strlcpy(tempStr, "0.00", sizeof(tempStr));
    doc["temperatura"] = tempStr;

    char pressStr[16];
    snprintf(pressStr, sizeof(pressStr), "%.2f", isnan(data.pressure) ? 1013.25f : data.pressure);
    if (atof(pressStr) < PRESSURE_MIN_VALID || atof(pressStr) > PRESSURE_MAX_VALID) strlcpy(pressStr, "0.00", sizeof(pressStr));
    doc["pressao"] = pressStr;

    // Giroscópio
    char gyroX[16], gyroY[16], gyroZ[16]; // Aumentado para 16
    snprintf(gyroX, sizeof(gyroX), "%.2f", isnan(data.gyroX) ? 0.0f : data.gyroX);
    snprintf(gyroY, sizeof(gyroY), "%.2f", isnan(data.gyroY) ? 0.0f : data.gyroY);
    snprintf(gyroZ, sizeof(gyroZ), "%.2f", isnan(data.gyroZ) ? 0.0f : data.gyroZ);
    JsonArray giroscopio = doc.createNestedArray("giroscopio");
    giroscopio.add(gyroX); giroscopio.add(gyroY); giroscopio.add(gyroZ);

    // Acelerômetro
    char accelX[8], accelY[8], accelZ[8];
    sprintf(accelX, "%.2f", isnan(data.accelX) ? 0.0f : data.accelX);
    sprintf(accelY, "%.2f", isnan(data.accelY) ? 0.0f : data.accelY);
    sprintf(accelZ, "%.2f", isnan(data.accelZ) ? 0.0f : data.accelZ);

    JsonArray acelerometro = doc.createNestedArray("acelerometro");
    acelerometro.add(accelX); acelerometro.add(accelY); acelerometro.add(accelZ);
}

void PayloadManager::_addGroundNodesToJson(JsonObject& payload, const GroundNodeBuffer& buffer) {
    if (buffer.activeNodes > 0) {
        JsonArray nodesArray = payload.createNestedArray("nodes");

        for (uint8_t i = 0; i < buffer.activeNodes; i++) {
            const MissionData& node = buffer.nodes[i];

            JsonObject nodeObj = nodesArray.createNestedObject();
            nodeObj["id"] = node.nodeId;

            char smStr[8], tStr[8], hStr[8], snrStr[8];
            sprintf(smStr, "%.2f", node.soilMoisture);
            sprintf(tStr, "%.2f", node.ambientTemp);
            sprintf(hStr, "%.2f", node.humidity);
            sprintf(snrStr, "%.2f", node.snr);

            nodeObj["sm"] = smStr;
            nodeObj["t"] = tStr;
            nodeObj["h"] = hStr;
            nodeObj["ir"] = node.irrigationStatus;
            nodeObj["rs"] = node.rssi;
            nodeObj["snr"] = snrStr;
            nodeObj["seq"] = node.sequenceNumber;
        }

        payload["total_nodes"] = buffer.activeNodes;
        payload["total_pkts"] = buffer.totalPacketsCollected;
    }
}
