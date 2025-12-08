/**
 * @file PayloadManager.cpp - Parte 1
 * @brief Implementação Completa com QoS Priority
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
    
    // Header
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);
    
    return offset;
}

// ============================================================================
// NOVO 5.3: Relay com Priorização QoS
// ============================================================================
int PayloadManager::createRelayPayload(const TelemetryData& data, 
                                       const GroundNodeBuffer& nodeBuffer,
                                       uint8_t* buffer,
                                       std::vector<uint16_t>& includedNodes) {
    int offset = 0;
    
    // Header
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);

    // Reserva byte para contagem de nós
    uint8_t nodeCountIndex = offset++; 
    
    includedNodes.clear();
    
    // === NOVO 5.3: Ordenar nós por prioridade ===
    MissionData sortedNodes[MAX_GROUND_NODES];
    memcpy(sortedNodes, nodeBuffer.nodes, sizeof(sortedNodes));
    
    // Atualizar prioridades antes de ordenar
    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        sortedNodes[i].priority = calculateNodePriority(sortedNodes[i]);
    }
    
    sortNodesByPriority(sortedNodes, nodeBuffer.activeNodes);
    
    // Log de ordenação
    DEBUG_PRINTLN("[PayloadManager] === Nós Ordenados por Prioridade (QoS) ===");
    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        const char* priName[] = {"CRITICAL", "HIGH", "NORMAL", "LOW"};
        DEBUG_PRINTF("  %d. Node %u - %s (Pri=%d)\n", 
                     i+1, sortedNodes[i].nodeId, 
                     priName[sortedNodes[i].priority], 
                     sortedNodes[i].priority);
    }
    
    // Adicionar nós ao payload (ordem de prioridade)
    uint8_t nodesAdded = 0;
    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        // Envia apenas se não foi enviado e é válido
        if (!sortedNodes[i].forwarded && sortedNodes[i].nodeId > 0) {
            // Verificar se há espaço no buffer
            if (offset + 10 > 250) {
                DEBUG_PRINTLN("[PayloadManager] Buffer cheio! Nós restantes não incluídos.");
                break;
            }
            
            _encodeNodeData(sortedNodes[i], buffer, offset);
            includedNodes.push_back(sortedNodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buffer[nodeCountIndex] = nodesAdded; 

    if (nodesAdded == 0) return 0;
    
    DEBUG_PRINTF("[PayloadManager] Relay: %d nós incluídos, %d bytes\n", nodesAdded, offset);
    return offset;
}

// ============================================================================
// NOVO 5.3: Cálculo de Prioridade QoS Inteligente
// ============================================================================
uint8_t PayloadManager::calculateNodePriority(const MissionData& node) {
    uint8_t priority = (uint8_t)PacketPriority::NORMAL;  // Padrão: NORMAL
    
    // === REGRA 1: Alertas Críticos de Irrigação ===
    // Umidade do solo fora da faixa ideal (20-80%)
    if (node.soilMoisture < 20.0f) {
        priority = (uint8_t)PacketPriority::CRITICAL;
        DEBUG_PRINTF("[QoS] Node %u: CRÍTICO - Solo seco (%.1f%%)\n", 
                     node.nodeId, node.soilMoisture);
        return priority;
    }
    
    if (node.soilMoisture > 90.0f) {
        priority = (uint8_t)PacketPriority::CRITICAL;
        DEBUG_PRINTF("[QoS] Node %u: CRÍTICO - Solo encharcado (%.1f%%)\n", 
                     node.nodeId, node.soilMoisture);
        return priority;
    }
    
    // === REGRA 2: Temperatura Extrema ===
    if (node.ambientTemp > 40.0f || node.ambientTemp < 5.0f) {
        priority = (uint8_t)PacketPriority::CRITICAL;
        DEBUG_PRINTF("[QoS] Node %u: CRÍTICO - Temp extrema (%.1f°C)\n", 
                     node.nodeId, node.ambientTemp);
        return priority;
    }
    
    // === REGRA 3: Link Degradado (Priorizar antes de perder) ===
    if (node.rssi < -110) {
        priority = (uint8_t)PacketPriority::HIGH;
        DEBUG_PRINTF("[QoS] Node %u: HIGH - Link ruim (%d dBm)\n", 
                     node.nodeId, node.rssi);
        return priority;
    }
    
    // === REGRA 4: Perda de Pacotes Excessiva ===
    if (node.packetsLost > 5) {
        priority = (uint8_t)PacketPriority::HIGH;
        DEBUG_PRINTF("[QoS] Node %u: HIGH - Perdas (%d pacotes)\n", 
                     node.nodeId, node.packetsLost);
        return priority;
    }
    
    // === REGRA 5: Irrigação Ativa ===
    if (node.irrigationStatus == 1) {
        priority = (uint8_t)PacketPriority::HIGH;
        DEBUG_PRINTF("[QoS] Node %u: HIGH - Irrigação ativa\n", node.nodeId);
        return priority;
    }
    
    // === REGRA 6: Dados Recentes têm Prioridade ===
    unsigned long age = millis() - node.lastLoraRx;
    if (age < 60000) {  // < 1 minuto
        priority = (uint8_t)PacketPriority::NORMAL;
    } else if (age < 300000) {  // < 5 minutos
        priority = (uint8_t)PacketPriority::NORMAL;
    } else {
        priority = (uint8_t)PacketPriority::LOW;  // Dados antigos
        DEBUG_PRINTF("[QoS] Node %u: LOW - Dados antigos (%.1f min)\n", 
                     node.nodeId, age / 60000.0f);
    }
    
    // === REGRA 7: Condições de Solo Moderadas ===
    if (node.soilMoisture >= 30.0f && node.soilMoisture <= 70.0f) {
        // Solo em condição ideal, baixar prioridade se já era NORMAL
        if (priority == (uint8_t)PacketPriority::NORMAL && age > 120000) {
            priority = (uint8_t)PacketPriority::LOW;
        }
    }
    
    return priority;
}

// ============================================================================
// NOVO 5.3: Ordenação por Prioridade (Bubble Sort)
// ============================================================================
void PayloadManager::sortNodesByPriority(MissionData* nodes, uint8_t count) {
    if (count <= 1) return;
    
    // Bubble Sort (eficiente para poucos elementos)
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            // Ordenar por: 1) Prioridade, 2) RSSI (link pior primeiro)
            bool shouldSwap = false;
            
            if (nodes[j].priority > nodes[j + 1].priority) {
                shouldSwap = true;  // Menor prioridade = mais importante
            } else if (nodes[j].priority == nodes[j + 1].priority) {
                // Mesma prioridade: RSSI pior tem preferência
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

// ============================================================================
// NOVO 5.3: Estatísticas de Priorização
// ============================================================================
void PayloadManager::getPriorityStats(const GroundNodeBuffer& buffer, 
                                      uint8_t& critical, uint8_t& high, 
                                      uint8_t& normal, uint8_t& low) {
    critical = high = normal = low = 0;
    
    for (int i = 0; i < buffer.activeNodes; i++) {
        switch (buffer.nodes[i].priority) {
            case (uint8_t)PacketPriority::CRITICAL: critical++; break;
            case (uint8_t)PacketPriority::HIGH:     high++;     break;
            case (uint8_t)PacketPriority::NORMAL:   normal++;   break;
            case (uint8_t)PacketPriority::LOW:      low++;      break;
        }
    }
}

// ============================================================================
// Continuação PayloadManager.cpp - Parte 2
// ============================================================================

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
        
        // NOVO 5.3: Adicionar informação de prioridade
        uint8_t crit, high, norm, low;
        getPriorityStats(groundBuffer, crit, high, norm, low);
        
        for (int i = 0; i < groundBuffer.activeNodes; i++) {
            JsonObject n = nodes.createNestedObject();
            const MissionData& md = groundBuffer.nodes[i];
            
            n["id"] = md.nodeId;
            n["sm"] = fmt(md.soilMoisture);
            n["t"]  = fmt(md.ambientTemp);
            n["h"]  = fmt(md.humidity);
            n["rs"] = md.rssi;
            
            // NOVO 5.3: Priority no JSON
            const char* priStr[] = {"CRIT", "HIGH", "NORM", "LOW"};
            n["pri"] = priStr[md.priority];
        }
        
        payload["total_nodes"] = groundBuffer.activeNodes;
        payload["total_pkts"]  = groundBuffer.totalPacketsCollected;
        
        // NOVO 5.3: Estatísticas de QoS
        payload["qos_crit"] = crit;
        payload["qos_high"] = high;
        payload["qos_norm"] = norm;
        payload["qos_low"]  = low;
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
    
    // Extrair timestamp do nó (se disponível)
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
    // Dados de sensor SEM timestamp (economia de downlink)
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

// Stubs legados
bool PayloadManager::_decodeAsciiPayload(const String& packet, MissionData& data) { 
    return false; 
}

bool PayloadManager::_validateAsciiChecksum(const String& packet) { 
    return true; 
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
    return 0;
}