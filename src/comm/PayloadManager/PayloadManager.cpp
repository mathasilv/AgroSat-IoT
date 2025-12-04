/**
 * @file PayloadManager.cpp
 * @brief Implementação Completa Satélite (RX com Timestamp, TX Otimizado)
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

void PayloadManager::update() {}

// ============================================================================
// TRANSMISSÃO (TX) - DOWNLINK SATÉLITE
// ============================================================================

int PayloadManager::createSatellitePayload(const TelemetryData& data, uint8_t* buffer) {
    int offset = 0;
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);
    
    return offset;
}

int PayloadManager::createRelayPayload(const TelemetryData& data, 
                                       const GroundNodeBuffer& nodeBuffer,
                                       uint8_t* buffer,
                                       std::vector<uint16_t>& includedNodes) {
    int offset = 0;
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);

    // Reserva byte para contagem
    uint8_t nodeCountIndex = offset++; 
    uint8_t nodesAdded = 0;
    
    includedNodes.clear();

    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        // Envia apenas se não foi enviado e é válido
        if (!nodeBuffer.nodes[i].forwarded && nodeBuffer.nodes[i].nodeId > 0) {
            if (offset + 10 > 250) break; // Buffer check
            
            // Encode SEM o Timestamp (Economia de downlink)
            _encodeNodeData(nodeBuffer.nodes[i], buffer, offset);
            
            includedNodes.push_back(nodeBuffer.nodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buffer[nodeCountIndex] = nodesAdded; 

    if (nodesAdded == 0) return 0;
    return offset;
}

String PayloadManager::createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    DynamicJsonDocument doc(2048); 
    auto fmt = [](float val) -> String { return isnan(val) ? "0.00" : String(val, 2); };

    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    doc["temperatura"] = fmt(data.temperature);
    doc["pressao"]     = fmt(data.pressure);
    doc["giroscopio"] = fmt(data.gyroX) + "," + fmt(data.gyroY) + "," + fmt(data.gyroZ);
    doc["acelerometro"] = fmt(data.accelX) + "," + fmt(data.accelY) + "," + fmt(data.accelZ);

    JsonObject payload = doc.createNestedObject("payload");
    payload["stat"] = (data.systemStatus == 0) ? "ok" : String(data.systemStatus, HEX);
    
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
// RECEPÇÃO (RX) - UPLINK DOS NÓS
// ============================================================================

bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1. Binário RAW (Novo Simulador)
    // O pacote já vem como bytes no objeto String
    if (packet.length() >= 12 && (uint8_t)packet[0] == 0xAB && (uint8_t)packet[1] == 0xCD) {
        if (_decodeRawPacket(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 2. Hex String (Legado)
    if (packet.startsWith("AB") && isxdigit(packet.charAt(2))) {
        if (_decodeHexStringPayload(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 3. ASCII (Legado)
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

bool PayloadManager::_decodeRawPacket(const String& rawData, MissionData& data) {
    const uint8_t* buffer = (const uint8_t*)rawData.c_str();
    size_t len = rawData.length();
    
    int offset = 4; // Pula Header

    data.nodeId = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.soilMoisture = (float)buffer[offset++];
    
    int16_t tRaw = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.ambientTemp = (tRaw / 10.0) - 50.0;
    
    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;
    
    // EXTRAIR TIMESTAMP DO NÓ (Se disponível)
    if (len >= offset + 4) {
        data.nodeTimestamp  = (uint32_t)buffer[offset] << 24;
        data.nodeTimestamp |= (uint32_t)buffer[offset+1] << 16;
        data.nodeTimestamp |= (uint32_t)buffer[offset+2] << 8;
        data.nodeTimestamp |= (uint32_t)buffer[offset+3];
    } else {
        data.nodeTimestamp = 0;
    }
    
    return true;
}

bool PayloadManager::_decodeHexStringPayload(const String& hexPayload, MissionData& data) {
    size_t len = hexPayload.length() / 2;
    if (len < 12) return false;
    
    uint8_t buffer[128];
    for (size_t i = 0; i < len; i++) {
        char byteStr[3] = { hexPayload.charAt(i*2), hexPayload.charAt(i*2+1), '\0' };
        buffer[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }
    
    if (buffer[0] != 0xAB || buffer[1] != 0xCD) return false;
    int offset = 4;

    data.nodeId = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.soilMoisture = (float)buffer[offset++];
    int16_t tRaw = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.ambientTemp = (tRaw / 10.0) - 50.0;
    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;
    
    if (len >= offset + 4) {
        data.nodeTimestamp  = (uint32_t)buffer[offset] << 24;
        data.nodeTimestamp |= (uint32_t)buffer[offset+1] << 16;
        data.nodeTimestamp |= (uint32_t)buffer[offset+2] << 8;
        data.nodeTimestamp |= (uint32_t)buffer[offset+3];
    }
    
    return true;
}

// ... Encoders e Auxiliares ...

void PayloadManager::_encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset) {
    buffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);
    
    auto enc16 = [&](float val, float scale, float offsetVal = 0) {
        int16_t v = (int16_t)((val + offsetVal) * scale);
        buffer[offset++] = (v >> 8) & 0xFF; buffer[offset++] = v & 0xFF;
    };
    auto encIMU = [&](float val, float scale) -> uint8_t {
        return (uint8_t)(int8_t)constrain(val * scale, -127, 127);
    };

    enc16(data.temperature, 10.0, 50.0);
    enc16(data.pressure, 10.0, -300.0);
    enc16(data.altitude, 1.0);
    buffer[offset++] = (uint8_t)constrain(data.humidity, 0, 100);
    enc16(data.co2, 1.0);
    enc16(data.tvoc, 1.0);

    buffer[offset++] = encIMU(data.gyroX, 0.5); buffer[offset++] = encIMU(data.gyroY, 0.5); buffer[offset++] = encIMU(data.gyroZ, 0.5);
    buffer[offset++] = encIMU(data.accelX, 16.0); buffer[offset++] = encIMU(data.accelY, 16.0); buffer[offset++] = encIMU(data.accelZ, 16.0);

    int32_t latI = 0, lonI = 0; uint16_t gpsAlt = 0;
    if (data.gpsFix) {
        latI = (int32_t)(data.latitude * 10000000.0); lonI = (int32_t)(data.longitude * 10000000.0);
        gpsAlt = (uint16_t)constrain(data.gpsAltitude, 0, 65535);
    }
    
    auto enc32 = [&](int32_t v) {
        buffer[offset++] = (v >> 24) & 0xFF; buffer[offset++] = (v >> 16) & 0xFF;
        buffer[offset++] = (v >> 8) & 0xFF; buffer[offset++] = v & 0xFF;
    };
    
    enc32(latI); enc32(lonI);
    buffer[offset++] = (gpsAlt >> 8) & 0xFF; buffer[offset++] = gpsAlt & 0xFF;
    buffer[offset++] = data.satellites; buffer[offset++] = data.systemStatus;
}

void PayloadManager::_encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset) {
    // Apenas dados de sensor, SEM timestamp (Economia de Downlink)
    buffer[offset++] = (node.nodeId >> 8) & 0xFF;
    buffer[offset++] = node.nodeId & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);
    int16_t t = (int16_t)((node.ambientTemp + 50.0) * 10);
    buffer[offset++] = (t >> 8) & 0xFF; buffer[offset++] = t & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    buffer[offset++] = node.irrigationStatus;
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

// ... Stubs legados ...
bool PayloadManager::_decodeAsciiPayload(const String& packet, MissionData& data) { return false; }
bool PayloadManager::_validateAsciiChecksum(const String& packet) { return true; }

void PayloadManager::markNodesAsForwarded(GroundNodeBuffer& buffer, const std::vector<uint16_t>& nodeIds, unsigned long timestamp) {
    for (uint16_t id : nodeIds) {
        for (int i = 0; i < buffer.activeNodes; i++) {
            if (buffer.nodes[i].nodeId == id) {
                buffer.nodes[i].forwarded = true;
                buffer.nodes[i].retransmissionTime = timestamp;
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
        if (_seqNodeId[i] == 0) { _seqNodeId[i] = nodeId; return i; }
    }
    return 0;
}