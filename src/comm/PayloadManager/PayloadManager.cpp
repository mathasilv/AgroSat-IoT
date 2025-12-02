/**
 * @file PayloadManager.cpp
 * @brief Implementação Final: Correção de Buffer JSON e Formato OBSAT
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
    // Espaço para lógica futura
}

// ============================================================================
// TRANSMISSÃO (TX) - LORA (BINÁRIO COMPACTO)
// ============================================================================

String PayloadManager::createSatellitePayload(const TelemetryData& data) {
    uint8_t buffer[64];
    int offset = 0;

    // Header: 0xAB 0xCD + Team ID
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

    // Byte para contagem de nós
    uint8_t nodeCountIndex = offset++; 
    uint8_t nodesAdded = 0;
    
    includedNodes.clear();

    for (int i = 0; i < buffer.activeNodes; i++) {
        if (!buffer.nodes[i].forwarded && buffer.nodes[i].nodeId > 0) {
            // Proteção de buffer
            if (offset + 10 > 190) break;
            
            _encodeNodeData(buffer.nodes[i], buf, offset);
            includedNodes.push_back(buffer.nodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buf[nodeCountIndex] = nodesAdded; 

    if (nodesAdded == 0) return ""; 

    return _binaryToHex(buf, offset);
}

// ============================================================================
// TRANSMISSÃO (TX) - HTTP JSON (OBSAT RIGOROSO + ARREDONDAMENTO CORRIGIDO)
// ============================================================================

String PayloadManager::createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    StaticJsonDocument<2048> doc; 
    
    // CORREÇÃO: Usar String() cria um novo objeto para cada valor,
    // evitando o problema do buffer compartilhado sobrescrito.
    auto fmt = [](float val) -> String {
        if (isnan(val)) return "0.00";
        return String(val, 2); // Arredonda para 2 casas decimais
    };

    // 1. Campos Obrigatórios (Nomes exatos da OBSAT)
    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    
    doc["temperatura"] = fmt(data.temperature);
    doc["pressao"]     = fmt(data.pressure);

    // 2. Arrays de IMU (Valores arredondados dentro do array)
    JsonArray gyro = doc.createNestedArray("giroscopio");
    gyro.add(fmt(data.gyroX));
    gyro.add(fmt(data.gyroY));
    gyro.add(fmt(data.gyroZ));

    JsonArray accel = doc.createNestedArray("acelerometro");
    accel.add(fmt(data.accelX));
    accel.add(fmt(data.accelY));
    accel.add(fmt(data.accelZ));

    // 3. Objeto Payload (Dados Extras + Nós de Solo)
    JsonObject payload = doc.createNestedObject("payload");
    
    if (!isnan(data.altitude)) payload["altitude"] = fmt(data.altitude);
    if (!isnan(data.humidity)) payload["umidade"]  = fmt(data.humidity);
    if (!isnan(data.co2))      payload["co2"]      = (int)data.co2;
    if (!isnan(data.tvoc))     payload["tvoc"]     = (int)data.tvoc;
    
    payload["stat"] = (data.systemStatus == 0) ? "ok" : String(data.systemStatus, HEX);
    
    // Nós de Solo
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodes = payload.createNestedArray("nodes");
        
        for (int i = 0; i < groundBuffer.activeNodes; i++) {
            JsonObject n = nodes.createNestedObject();
            const MissionData& md = groundBuffer.nodes[i];
            
            n["id"] = md.nodeId;
            n["sm"] = fmt(md.soilMoisture);
            n["t"]  = fmt(md.ambientTemp);
            n["h"]  = fmt(md.humidity);
            n["rs"] = md.rssi;
        }
        payload["total_nodes"] = groundBuffer.activeNodes;
        payload["total_pkts"]  = groundBuffer.totalPacketsCollected;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// RECEPÇÃO (RX) - BINÁRIO + ASCII
// ============================================================================

bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1. Binário Hex (Inicia com AB...)
    if (packet.length() >= 12 && packet.startsWith("AB") && isxdigit(packet.charAt(2))) {
        if (_decodeBinaryPayload(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 2. ASCII Legado (Inicia com AGRO...)
    if (packet.startsWith("AGRO")) {
        if (_validateAsciiChecksum(packet)) {
            if (_decodeAsciiPayload(packet, data)) {
                _lastMissionData = data;
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// GESTÃO E HELPERS PÚBLICOS
// ============================================================================

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
    uint8_t priority = 0;
    if (node.soilMoisture < 30 || node.soilMoisture > 90) priority += 5;
    if (node.rssi > -90) priority += 2;
    if (node.packetsLost > 0) priority += 2;
    return constrain(priority, 0, 10);
}

int PayloadManager::findNodeIndex(uint16_t nodeId) {
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == nodeId) return i;
    }
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == 0) {
            _seqNodeId[i] = nodeId;
            return i;
        }
    }
    return 0; // Fallback
}

// ============================================================================
// HELPERS PRIVADOS (ENCODERS & DECODERS)
// ============================================================================

void PayloadManager::_encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset) {
    buffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);
    
    int16_t temp = (int16_t)((data.temperature + 50.0) * 10);
    buffer[offset++] = (temp >> 8) & 0xFF; buffer[offset++] = temp & 0xFF;
    
    uint16_t press = (uint16_t)((data.pressure - 300) * 10);
    buffer[offset++] = (press >> 8) & 0xFF; buffer[offset++] = press & 0xFF;
    
    uint16_t alt = (uint16_t)constrain(data.altitude, 0, 65535);
    buffer[offset++] = (alt >> 8) & 0xFF; buffer[offset++] = alt & 0xFF;

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
    buffer[offset++] = (tempEnc >> 8) & 0xFF; buffer[offset++] = tempEnc & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    buffer[offset++] = node.irrigationStatus;
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

bool PayloadManager::_decodeBinaryPayload(const String& hexPayload, MissionData& data) {
    if (hexPayload.length() < 24) return false;
    uint8_t buffer[128];
    size_t len = hexPayload.length() / 2;
    for (size_t i = 0; i < len; i++) {
        char byteStr[3] = { hexPayload.charAt(i*2), hexPayload.charAt(i*2+1), '\0' };
        buffer[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }
    if (buffer[0] != 0xAB || buffer[1] != 0xCD) return false;

    int offset = 4;
    // Validação mínima de tamanho
    if (len < offset + 8) return false;

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
    String p = packet;
    if (p.indexOf('*') > 0) p = p.substring(0, p.indexOf('*')); 
    p.remove(0, 5); 
    int idx = 0;
    String parts[10];
    while (p.length() > 0 && idx < 10) {
        int comma = p.indexOf(',');
        if (comma == -1) { parts[idx++] = p; break; } 
        else { parts[idx++] = p.substring(0, comma); p.remove(0, comma + 1); }
    }
    if (idx < 4) return false;
    data.sequenceNumber = parts[0].toInt();
    data.nodeId = parts[1].toInt();
    data.soilMoisture = parts[2].toFloat();
    data.ambientTemp = parts[3].toFloat();
    if (idx > 4) data.humidity = parts[4].toFloat();
    if (idx > 5) data.irrigationStatus = parts[5].toInt();
    
    int nodeIdx = findNodeIndex(data.nodeId);
    if (_expectedSeqNum[nodeIdx] > 0) {
        int16_t lost = data.sequenceNumber - _expectedSeqNum[nodeIdx];
        if (lost > 0) _packetsLost += lost;
    }
    _expectedSeqNum[nodeIdx] = data.sequenceNumber + 1;
    data.packetsReceived = 1;
    return true;
}

bool PayloadManager::_validateAsciiChecksum(const String& packet) {
    int star = packet.lastIndexOf('*');
    if (star == -1) return true;
    String content = packet.substring(0, star);
    String checkStr = packet.substring(star + 1);
    uint8_t calc = 0;
    for (unsigned int i = 0; i < content.length(); i++) calc ^= content.charAt(i);
    uint8_t received = strtol(checkStr.c_str(), NULL, 16);
    return (calc == received);
}

String PayloadManager::_binaryToHex(const uint8_t* buffer, size_t length) {
    String hex = "";
    hex.reserve(length * 2);
    for (size_t i = 0; i < length; i++) {
        char temp[3];
        snprintf(temp, sizeof(temp), "%02X", buffer[i]);
        hex += temp;
    }
    return hex;
}
