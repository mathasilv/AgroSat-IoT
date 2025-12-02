/**
 * @file PayloadManager.cpp
 * @brief Implementação PayloadManager (Legacy Compatible)
 */

#include "PayloadManager.h"

PayloadManager::PayloadManager() :
    _packetsReceived(0),
    _packetsLost(0)
{
    memset(&_lastMissionData, 0, sizeof(MissionData));
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        _expectedSeqNum[i] = 0;
        _seqNodeId[i] = 0;
    }
}

void PayloadManager::update() {
    // Lógica de timeout pode ser reintroduzida aqui se necessário
}

// ============================================================================
// TRANSMISSÃO (TX)
// ============================================================================

String PayloadManager::createSatellitePayload(const TelemetryData& data) {
    uint8_t buffer[64]; // Aumentado para caber IMU
    int offset = 0;

    // Header Original: 0xAB 0xCD + Team ID
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);

    return _binaryToHex(buffer, offset);
}

String PayloadManager::createRelayPayload(const TelemetryData& data, 
                                        const GroundNodeBuffer& buffer,
                                        std::vector<uint16_t>& includedNodes) {
    uint8_t buf[200];
    int offset = 0;

    // Header
    buf[offset++] = 0xAB;
    buf[offset++] = 0xCD;
    buf[offset++] = (TEAM_ID >> 8) & 0xFF;
    buf[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buf, offset);

    // Contagem de nós (placeholder)
    uint8_t nodeCountIndex = offset++; 
    uint8_t nodesAdded = 0;
    
    includedNodes.clear();

    for (int i = 0; i < buffer.activeNodes; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            if (offset + 10 > 190) break; // Proteção de buffer
            
            _encodeNodeData(buffer.nodes[i], buf, offset);
            includedNodes.push_back(buffer.nodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buf[nodeCountIndex] = nodesAdded; // Preenche contagem real

    if (nodesAdded == 0) return ""; 

    return _binaryToHex(buf, offset);
}

String PayloadManager::createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    // Formato OBSAT Oficial (Rigoroso)
    StaticJsonDocument<1536> doc; 

    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    
    char buf[16];
    dtostrf(data.temperature, 1, 2, buf); doc["temperatura"] = buf;
    dtostrf(data.pressure, 1, 2, buf);    doc["pressao"] = buf;

    // Arrays de IMU
    JsonArray gyro = doc.createNestedArray("giroscopio");
    dtostrf(data.gyroX, 1, 2, buf); gyro.add(buf);
    dtostrf(data.gyroY, 1, 2, buf); gyro.add(buf);
    dtostrf(data.gyroZ, 1, 2, buf); gyro.add(buf);

    JsonArray accel = doc.createNestedArray("acelerometro");
    dtostrf(data.accelX, 1, 2, buf); accel.add(buf);
    dtostrf(data.accelY, 1, 2, buf); accel.add(buf);
    dtostrf(data.accelZ, 1, 2, buf); accel.add(buf);

    // Payload Customizado
    JsonObject payload = doc.createNestedObject("payload");
    
    if (!isnan(data.altitude)) { dtostrf(data.altitude, 1, 2, buf); payload["altitude"] = buf; }
    if (!isnan(data.humidity)) { dtostrf(data.humidity, 1, 2, buf); payload["umidade"] = buf; }
    if (!isnan(data.co2)) payload["co2"] = (int)data.co2;
    if (!isnan(data.tvoc)) payload["tvoc"] = (int)data.tvoc;
    
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodes = payload.createNestedArray("nodes");
        for (int i = 0; i < groundBuffer.activeNodes; i++) {
            JsonObject n = nodes.createNestedObject();
            n["id"] = groundBuffer.nodes[i].nodeId;
            n["sm"] = groundBuffer.nodes[i].soilMoisture;
            n["t"]  = groundBuffer.nodes[i].ambientTemp;
            n["h"]  = groundBuffer.nodes[i].humidity;
            n["rs"] = groundBuffer.nodes[i].rssi;
        }
        payload["total_nodes"] = groundBuffer.activeNodes;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// RECEPÇÃO (RX) - Híbrido (Binário + ASCII)
// ============================================================================

bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1. Tentar formato BINÁRIO (Hex)
    // Ex: "ABCD..." (Header 0xAB 0xCD)
    if (packet.length() >= 12 && packet.startsWith("AB") && isxdigit(packet.charAt(2))) {
        if (_decodeBinaryPayload(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 2. Tentar formato ASCII LEGADO
    // Ex: "AGRO,1,105,45.5,25.0,60.0,1*CS"
    if (packet.startsWith("AGRO")) {
        if (_validateAsciiChecksum(packet)) {
            if (_decodeAsciiPayload(packet, data)) {
                _lastMissionData = data;
                return true;
            }
        } else {
            DEBUG_PRINTLN("[PayloadManager] Erro Checksum ASCII");
        }
    }

    return false;
}

// ============================================================================
// ENCODERS (TX)
// ============================================================================

void PayloadManager::_encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset) {
    // 1. Bateria (0-100)
    buffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);

    // 2. Temperatura (+50 offset, x10)
    float temp = !isnan(data.temperature) ? data.temperature : 0.0;
    int16_t tempEnc = (int16_t)((temp + 50.0) * 10);
    buffer[offset++] = (tempEnc >> 8) & 0xFF;
    buffer[offset++] = tempEnc & 0xFF;

    // 3. Pressão (-300 offset, x10)
    float press = !isnan(data.pressure) ? data.pressure : 1013.25;
    uint16_t pressEnc = (uint16_t)((press - 300.0) * 10);
    buffer[offset++] = (pressEnc >> 8) & 0xFF;
    buffer[offset++] = pressEnc & 0xFF;

    // 4. Altitude (0-65535)
    float alt = !isnan(data.altitude) ? data.altitude : 0.0;
    uint16_t altEnc = (uint16_t)constrain(alt, 0, 65535);
    buffer[offset++] = (altEnc >> 8) & 0xFF;
    buffer[offset++] = altEnc & 0xFF;

    // 5. IMU RESTAURADO (Giroscópio /2, range -127 a 127)
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroX) ? data.gyroX : 0.0) / 2.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroY) ? data.gyroY : 0.0) / 2.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.gyroZ) ? data.gyroZ : 0.0) / 2.0, -127, 127);

    // 6. IMU RESTAURADO (Acelerômetro *16, range -127 a 127)
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelX) ? data.accelX : 0.0) * 16.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelY) ? data.accelY : 0.0) * 16.0, -127, 127);
    buffer[offset++] = (uint8_t)(int8_t)constrain((!isnan(data.accelZ) ? data.accelZ : 0.0) * 16.0, -127, 127);
}

void PayloadManager::_encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset) {
    // 2 bytes ID
    buffer[offset++] = (node.nodeId >> 8) & 0xFF;
    buffer[offset++] = node.nodeId & 0xFF;
    // 1 byte Soil
    buffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);
    
    // 2 bytes Temp (+50, x10)
    int16_t tempEnc = (int16_t)((node.ambientTemp + 50.0) * 10);
    buffer[offset++] = (tempEnc >> 8) & 0xFF;
    buffer[offset++] = tempEnc & 0xFF;

    // 1 byte Humidity
    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    
    // 1 byte Irrigation
    buffer[offset++] = node.irrigationStatus;
    
    // 1 byte RSSI (+128 offset)
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

// ============================================================================
// DECODERS (RX)
// ============================================================================

bool PayloadManager::_decodeBinaryPayload(const String& hexPayload, MissionData& data) {
    if (hexPayload.length() < 24) return false;

    // Converter Hex para Bytes
    uint8_t buffer[128];
    size_t len = hexPayload.length() / 2;
    for (size_t i = 0; i < len; i++) {
        char byteStr[3] = { hexPayload.charAt(i*2), hexPayload.charAt(i*2+1), '\0' };
        buffer[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }

    if (buffer[0] != 0xAB || buffer[1] != 0xCD) return false;

    // Decodifica estrutura do nó terrestre (Offset 4)
    // Supondo que o pacote binário recebido seja de um nó, não do satélite
    int offset = 4;
    data.nodeId = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.soilMoisture = (float)buffer[offset++];
    
    int16_t tRaw = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.ambientTemp = (tRaw / 10.0) - 50.0;
    
    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;

    return true;
}

bool PayloadManager::_decodeAsciiPayload(const String& packet, MissionData& data) {
    // Formato: AGRO,seq,id,soil,temp,hum,irr
    String p = packet;
    if (p.indexOf('*') > 0) p = p.substring(0, p.indexOf('*')); // Remove checksum
    
    // Remove "AGRO,"
    p.remove(0, 5); 
    
    int idx = 0;
    String parts[10];
    while (p.length() > 0 && idx < 10) {
        int comma = p.indexOf(',');
        if (comma == -1) {
            parts[idx++] = p;
            break;
        } else {
            parts[idx++] = p.substring(0, comma);
            p.remove(0, comma + 1);
        }
    }

    if (idx < 4) return false; // Mínimo de campos

    data.sequenceNumber = parts[0].toInt();
    data.nodeId = parts[1].toInt();
    data.soilMoisture = parts[2].toFloat();
    data.ambientTemp = parts[3].toFloat();
    if (idx > 4) data.humidity = parts[4].toFloat();
    if (idx > 5) data.irrigationStatus = parts[5].toInt();

    // Rastreamento de pacotes
    int nodeIdx = findNodeIndex(data.nodeId);
    if (_expectedSeqNum[nodeIdx] > 0) {
        int16_t lost = data.sequenceNumber - _expectedSeqNum[nodeIdx];
        if (lost > 0) _packetsLost += lost;
    }
    _expectedSeqNum[nodeIdx] = data.sequenceNumber + 1;
    data.packetsReceived = 1; // Contagem local do pacote

    return true;
}

bool PayloadManager::_validateAsciiChecksum(const String& packet) {
    int star = packet.lastIndexOf('*');
    if (star == -1) return true; // Sem checksum é aceito (legado)

    String content = packet.substring(0, star);
    String checkStr = packet.substring(star + 1);
    
    uint8_t calc = 0;
    for (unsigned int i = 0; i < content.length(); i++) calc ^= content.charAt(i);
    
    uint8_t received = strtol(checkStr.c_str(), NULL, 16);
    return (calc == received);
}

// ============================================================================
// UTILS
// ============================================================================

String PayloadManager::_binaryToHex(const uint8_t* buffer, size_t length) {
    String hex = "";
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] < 16) hex += "0";
        hex += String(buffer[i], HEX);
    }
    hex.toUpperCase();
    return hex;
}

int PayloadManager::findNodeIndex(uint16_t nodeId) {
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == nodeId) return i;
    }
    // Novo nó
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == 0) {
            _seqNodeId[i] = nodeId;
            return i;
        }
    }
    return 0; // Fallback (sobrescreve primeiro se cheio)
}

void PayloadManager::markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds) {
    for (uint16_t id : nodeIds) {
        for (int i = 0; i < buffer.activeNodes; i++) {
            if (buffer.nodes[i].nodeId == id) {
                buffer.nodes[i].forwarded = true;
                break;
            }
        }
    }
}

uint8_t PayloadManager::calculateNodePriority(const MissionData& node) {
    uint8_t p = 0;
    if (node.soilMoisture < 30 || node.soilMoisture > 80) p += 5;
    if (node.rssi > -90) p += 2;
    if (node.packetsLost > 0) p += 3;
    return constrain(p, 0, 10);
}