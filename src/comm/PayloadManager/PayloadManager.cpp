/**
 * @file PayloadManager.cpp
 * @brief Implementação do PayloadManager (CLEANED)
 */

#include "PayloadManager.h"

PayloadManager::PayloadManager() {
    memset(&_lastMissionData, 0, sizeof(MissionData));
}

void PayloadManager::update() {}

// ============================================================================
// TRANSMISSÃO (TX) - DOWNLINK SATÉLITE
// ============================================================================

int PayloadManager::createSatellitePayload(const TelemetryData& data, uint8_t* buffer) {
    int offset = 0;
    
    // Header
    buffer[offset++] = 0x4E;  // 'N'
    buffer[offset++] = 0x50;  // 'P'
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
    
    // Header
    buffer[offset++] = 0x4E;  // 'N'
    buffer[offset++] = 0x50;  // 'P'
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);

    // Reserva byte para contagem de nós
    uint8_t nodeCountIndex = offset++; 
    
    includedNodes.clear();
    
    // Copiar nós para buffer local para ordenação
    MissionData sortedNodes[MAX_GROUND_NODES];
    memcpy(sortedNodes, nodeBuffer.nodes, sizeof(sortedNodes));
    
    sortNodesByPriority(sortedNodes, nodeBuffer.activeNodes);
    
    // Adicionar nós ao payload (ordem de prioridade)
    uint8_t nodesAdded = 0;
    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        if (!sortedNodes[i].forwarded && sortedNodes[i].nodeId > 0) {
            if (offset + 10 > 250) {
                DEBUG_PRINTLN("[PayloadManager] Buffer cheio!");
                break;
            }
            
            _encodeNodeData(sortedNodes[i], buffer, offset);
            includedNodes.push_back(sortedNodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buffer[nodeCountIndex] = nodesAdded; 

    if (nodesAdded == 0) return 0;
    
    DEBUG_PRINTF("[PayloadManager] Relay: %d nos, %d bytes\n", nodesAdded, offset);
    return offset;
}

// ============================================================================
// QoS - Cálculo de Prioridade
// ============================================================================
uint8_t PayloadManager::calculateNodePriority(const MissionData& node) {
    uint8_t priority = static_cast<uint8_t>(PacketPriority::NORMAL);
    
    // Alertas Críticos de Irrigação
    if (node.soilMoisture < 20.0f || node.soilMoisture > 90.0f) {
        return static_cast<uint8_t>(PacketPriority::CRITICAL);
    }
    
    // Temperatura Extrema
    if (node.ambientTemp > 40.0f || node.ambientTemp < 5.0f) {
        return static_cast<uint8_t>(PacketPriority::CRITICAL);
    }
    
    // Link Degradado
    if (node.rssi < -110) {
        return static_cast<uint8_t>(PacketPriority::HIGH_PRIORITY);
    }
    
    // Perda de Pacotes Excessiva
    if (node.packetsLost > 5) {
        return static_cast<uint8_t>(PacketPriority::HIGH_PRIORITY);
    }
    
    // Irrigação Ativa
    if (node.irrigationStatus == 1) {
        return static_cast<uint8_t>(PacketPriority::HIGH_PRIORITY);
    }
    
    // Dados Antigos
    unsigned long age = millis() - node.lastLoraRx;
    if (age > 300000) {
        priority = static_cast<uint8_t>(PacketPriority::LOW_PRIORITY);
    }
    
    return priority;
}

void PayloadManager::sortNodesByPriority(MissionData* nodes, uint8_t count) {
    if (count <= 1) return;
    
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            bool shouldSwap = false;
            
            if (nodes[j].priority > nodes[j + 1].priority) {
                shouldSwap = true;
            } else if (nodes[j].priority == nodes[j + 1].priority) {
                if (nodes[j].rssi > nodes[j + 1].rssi) {
                    shouldSwap = true;
                }
            }
            
            if (shouldSwap) {
                MissionData temp = nodes[j];
                nodes[j] = nodes[j + 1];
                nodes[j + 1] = temp;
            }
        }
    }
}

void PayloadManager::getPriorityStats(const GroundNodeBuffer& buffer, 
                                      uint8_t& critical, uint8_t& high, 
                                      uint8_t& normal, uint8_t& low) {
    critical = high = normal = low = 0;
    
    for (int i = 0; i < buffer.activeNodes; i++) {
        switch (buffer.nodes[i].priority) {
            case static_cast<uint8_t>(PacketPriority::CRITICAL): critical++; break;
            case static_cast<uint8_t>(PacketPriority::HIGH_PRIORITY): high++; break;
            case static_cast<uint8_t>(PacketPriority::NORMAL): normal++; break;
            case static_cast<uint8_t>(PacketPriority::LOW_PRIORITY): low++; break;
        }
    }
}

// ============================================================================
// JSON para HTTP
// ============================================================================
String PayloadManager::createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    DynamicJsonDocument doc(2048); 
    auto fmt = [](float val) -> String { return isnan(val) ? "0.00" : String(val, 2); };

    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    doc["temperatura"] = fmt(data.temperature);
    doc["pressao"] = fmt(data.pressure);
    doc["giroscopio"] = fmt(data.gyroX) + "," + fmt(data.gyroY) + "," + fmt(data.gyroZ);
    doc["acelerometro"] = fmt(data.accelX) + "," + fmt(data.accelY) + "," + fmt(data.accelZ);

    JsonObject payload = doc.createNestedObject("payload");
    payload["stat"] = (data.systemStatus == 0) ? "ok" : String(data.systemStatus, HEX);
    
    if (groundBuffer.activeNodes > 0) {
        JsonArray nodes = payload.createNestedArray("nodes");
        
        uint8_t crit, high, norm, low;
        getPriorityStats(groundBuffer, crit, high, norm, low);
        
        for (int i = 0; i < groundBuffer.activeNodes; i++) {
            JsonObject n = nodes.createNestedObject();
            const MissionData& md = groundBuffer.nodes[i];
            
            n["id"] = md.nodeId;
            n["sm"] = fmt(md.soilMoisture);
            n["t"] = fmt(md.ambientTemp);
            n["h"] = fmt(md.humidity);
            n["rs"] = md.rssi;
            
            const char* priStr[] = {"CRIT", "HIGH", "NORM", "LOW"};
            n["pri"] = priStr[md.priority];
        }
        
        payload["total_nodes"] = groundBuffer.activeNodes;
        payload["total_pkts"] = groundBuffer.totalPacketsCollected;
        payload["qos_crit"] = crit;
        payload["qos_high"] = high;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// RECEPÇÃO (RX)
// ============================================================================
bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1. Binário RAW
    if (packet.length() >= 12 && (uint8_t)packet[0] == 0x4E && (uint8_t)packet[1] == 0x50) {
        if (_decodeRawPacket(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 2. Hex String (Legado)
    if (packet.startsWith("NP") && isxdigit(packet.charAt(2))) {
        if (_decodeHexStringPayload(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    return false;
}

bool PayloadManager::_decodeRawPacket(const String& rawData, MissionData& data) {
    const uint8_t* buffer = (const uint8_t*)rawData.c_str();
    size_t len = rawData.length();
    
    int offset = 4;

    data.nodeId = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.soilMoisture = (float)buffer[offset++];
    
    int16_t tRaw = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.ambientTemp = (tRaw / 10.0) - 50.0;
    
    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;
    
    if (len >= (size_t)(offset + 4)) {
        data.nodeTimestamp  = (uint32_t)buffer[offset] << 24;
        data.nodeTimestamp |= (uint32_t)buffer[offset+1] << 16;
        data.nodeTimestamp |= (uint32_t)buffer[offset+2] << 8;
        data.nodeTimestamp |= (uint32_t)buffer[offset+3];
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
    
    if (buffer[0] != 0x4E || buffer[1] != 0x50) return false;
    int offset = 4;

    data.nodeId = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.soilMoisture = (float)buffer[offset++];
    int16_t tRaw = (buffer[offset] << 8) | buffer[offset+1]; offset += 2;
    data.ambientTemp = (tRaw / 10.0) - 50.0;
    data.humidity = (float)buffer[offset++];
    data.irrigationStatus = buffer[offset++];
    data.rssi = (int16_t)buffer[offset++] - 128;
    
    if (len >= (size_t)(offset + 4)) {
        data.nodeTimestamp  = (uint32_t)buffer[offset] << 24;
        data.nodeTimestamp |= (uint32_t)buffer[offset+1] << 16;
        data.nodeTimestamp |= (uint32_t)buffer[offset+2] << 8;
        data.nodeTimestamp |= (uint32_t)buffer[offset+3];
    }
    
    return true;
}

// ============================================================================
// ENCODERS
// ============================================================================
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

    buffer[offset++] = encIMU(data.gyroX, 0.5); 
    buffer[offset++] = encIMU(data.gyroY, 0.5); 
    buffer[offset++] = encIMU(data.gyroZ, 0.5);
    buffer[offset++] = encIMU(data.accelX, 16.0); 
    buffer[offset++] = encIMU(data.accelY, 16.0); 
    buffer[offset++] = encIMU(data.accelZ, 16.0);

    int32_t latI = 0, lonI = 0; 
    uint16_t gpsAlt = 0;
    if (data.gpsFix) {
        latI = (int32_t)(data.latitude * 10000000.0); 
        lonI = (int32_t)(data.longitude * 10000000.0);
        gpsAlt = (uint16_t)constrain(data.gpsAltitude, 0, 65535);
    }
    
    auto enc32 = [&](int32_t v) {
        buffer[offset++] = (v >> 24) & 0xFF; 
        buffer[offset++] = (v >> 16) & 0xFF;
        buffer[offset++] = (v >> 8) & 0xFF; 
        buffer[offset++] = v & 0xFF;
    };
    
    enc32(latI); 
    enc32(lonI);
    buffer[offset++] = (gpsAlt >> 8) & 0xFF; 
    buffer[offset++] = gpsAlt & 0xFF;
    buffer[offset++] = data.satellites; 
    buffer[offset++] = data.systemStatus;
}

void PayloadManager::_encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset) {
    buffer[offset++] = (node.nodeId >> 8) & 0xFF;
    buffer[offset++] = node.nodeId & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);
    int16_t t = (int16_t)((node.ambientTemp + 50.0) * 10);
    buffer[offset++] = (t >> 8) & 0xFF; 
    buffer[offset++] = t & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    buffer[offset++] = node.irrigationStatus;
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

void PayloadManager::markNodesAsForwarded(GroundNodeBuffer& buffer, 
                                         const std::vector<uint16_t>& nodeIds, 
                                         unsigned long timestamp) {
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
