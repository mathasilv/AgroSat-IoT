/**
 * @file PayloadManager.cpp
 * @brief Implementação Completa: TX Binário Otimizado + RX Legado (Hex/ASCII) Suportado
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
    // Reservado para lógica futura de limpeza ou timeouts
}

// ============================================================================
// TRANSMISSÃO BINÁRIA (TX) - OTIMIZADA
// ============================================================================

int PayloadManager::createSatellitePayload(const TelemetryData& data, uint8_t* buffer) {
    int offset = 0;
    // Header (4 Bytes): 0xAB 0xCD + Team ID
    buffer[offset++] = 0xAB;
    buffer[offset++] = 0xCD;
    buffer[offset++] = (TEAM_ID >> 8) & 0xFF;
    buffer[offset++] = TEAM_ID & 0xFF;

    _encodeSatelliteData(data, buffer, offset);
    
    return offset; // Retorna tamanho real em bytes
}

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
    uint8_t nodesAdded = 0;
    
    includedNodes.clear();

    for (int i = 0; i < nodeBuffer.activeNodes; i++) {
        // Envia apenas se não foi enviado E se tem ID válido
        if (!nodeBuffer.nodes[i].forwarded && nodeBuffer.nodes[i].nodeId > 0) {
            // Proteção de buffer (max 250 bytes LoRa)
            if (offset + 10 > 250) break;
            
            _encodeNodeData(nodeBuffer.nodes[i], buffer, offset);
            includedNodes.push_back(nodeBuffer.nodes[i].nodeId);
            nodesAdded++;
        }
    }
    
    buffer[nodeCountIndex] = nodesAdded; 

    if (nodesAdded == 0) return 0; // Nada para enviar
    return offset;
}

// ============================================================================
// TRANSMISSÃO HTTP (JSON) - MANTIDA PARA COMPATIBILIDADE
// ============================================================================

String PayloadManager::createTelemetryJSON(const TelemetryData& data, const GroundNodeBuffer& groundBuffer) {
    StaticJsonDocument<2048> doc; 
    
    auto fmt = [](float val) -> String {
        if (isnan(val)) return "0.00";
        return String(val, 2);
    };

    auto fmtGPS = [](double val) -> String {
        if (isnan(val)) return "0.000000";
        return String(val, 6);
    };

    doc["equipe"] = TEAM_ID;
    doc["bateria"] = (int)data.batteryPercentage;
    doc["temperatura"] = fmt(data.temperature);
    doc["pressao"]     = fmt(data.pressure);

    JsonArray gyro = doc.createNestedArray("giroscopio");
    gyro.add(fmt(data.gyroX));
    gyro.add(fmt(data.gyroY));
    gyro.add(fmt(data.gyroZ));

    JsonArray accel = doc.createNestedArray("acelerometro");
    accel.add(fmt(data.accelX));
    accel.add(fmt(data.accelY));
    accel.add(fmt(data.accelZ));

    JsonObject payload = doc.createNestedObject("payload");
    
    if (!isnan(data.altitude)) payload["altitude"] = fmt(data.altitude);
    if (!isnan(data.humidity)) payload["umidade"]  = fmt(data.humidity);
    if (!isnan(data.co2))      payload["co2"]      = (int)data.co2;
    if (!isnan(data.tvoc))     payload["tvoc"]     = (int)data.tvoc;
    
    if (data.gpsFix) {
        payload["lat"] = fmtGPS(data.latitude);
        payload["lng"] = fmtGPS(data.longitude);
        payload["gps_alt"] = (int)data.gpsAltitude;
        payload["sats"] = data.satellites;
    } else {
        payload["gps_status"] = "no_fix";
    }
    
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
// RECEPÇÃO (RX) - RESTAURADA E COMPLETA
// ============================================================================

bool PayloadManager::processLoRaPacket(const String& packet, MissionData& data) {
    memset(&data, 0, sizeof(MissionData));

    // 1. Verifica Binário Puro (Novo formato: começa com byte 0xAB)
    if (packet.length() > 0 && packet.charAt(0) == (char)0xAB) {
        // Se implementarmos RX binário no futuro, seria aqui.
        // Por enquanto, focamos em decodificar nós que enviam HEX ou ASCII (Legado)
    }

    // 2. Binário Hexadecimal (String ASCII "AB...")
    if (packet.length() >= 12 && packet.startsWith("AB") && isxdigit(packet.charAt(2))) {
        if (_decodeBinaryPayload(packet, data)) {
            _lastMissionData = data;
            return true;
        }
    }

    // 3. ASCII Legado ("AGRO...")
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
// ENCODERS (TX)
// ============================================================================

void PayloadManager::_encodeSatelliteData(const TelemetryData& data, uint8_t* buffer, int& offset) {
    // 1. Bateria (1 byte)
    buffer[offset++] = (uint8_t)constrain(data.batteryPercentage, 0, 100);
    
    // Auxiliar para encodings de 16-bit (2 bytes)
    auto enc16 = [&](float val, float scale, float offsetVal = 0) {
        int16_t v = (int16_t)((val + offsetVal) * scale);
        buffer[offset++] = (v >> 8) & 0xFF; 
        buffer[offset++] = v & 0xFF;
    };
    
    // Auxiliar para encodings de 8-bit de IMU (1 byte)
    auto encIMU = [&](float val, float scale) -> uint8_t {
        return (uint8_t)(int8_t)constrain(val * scale, -127, 127);
    };

    // 2. Temperatura (2 bytes: precisão 0.1, range -50°C a +100°C)
    enc16(data.temperature, 10.0, 50.0);
    
    // 3. Pressão (2 bytes: offset 300hPa, precisão 0.1hPa)
    enc16(data.pressure, 10.0, -300.0);
    
    // 4. Altitude do Altimetro (2 bytes: 0 a 65535m)
    enc16(data.altitude, 1.0);

    // 5. Umidade (1 byte: 0-100%)
    buffer[offset++] = (uint8_t)constrain(data.humidity, 0, 100);

    // 6. CO2 (2 bytes: 0 a 8192 ppm)
    enc16(data.co2, 1.0);
    
    // 7. TVOC (2 bytes: 0 a 1187 ppb)
    enc16(data.tvoc, 1.0);

    // 8. IMU (6 bytes: 3 Giroscópio, 3 Acelerômetro)
    buffer[offset++] = encIMU(data.gyroX, 0.5); 
    buffer[offset++] = encIMU(data.gyroY, 0.5);
    buffer[offset++] = encIMU(data.gyroZ, 0.5);
    buffer[offset++] = encIMU(data.accelX, 16.0); 
    buffer[offset++] = encIMU(data.accelY, 16.0);
    buffer[offset++] = encIMU(data.accelZ, 16.0);

    // 9. GPS (15 bytes: Lat/Lon, Altitude, Satélites)
    int32_t latI = 0, lonI = 0;
    uint16_t gpsAlt = 0;
    
    // A flag gpsFix é implícita se lat/lon forem 0 (0x00000000)
    if (data.gpsFix) {
        latI = (int32_t)(data.latitude * 10000000.0);
        lonI = (int32_t)(data.longitude * 10000000.0);
        gpsAlt = (uint16_t)constrain(data.gpsAltitude, 0, 65535);
    }
    
    // Latitude e Longitude (8 bytes: precisão 1e-7 deg)
    auto enc32 = [&](int32_t v) {
        buffer[offset++] = (v >> 24) & 0xFF;
        buffer[offset++] = (v >> 16) & 0xFF;
        buffer[offset++] = (v >> 8) & 0xFF;
        buffer[offset++] = v & 0xFF;
    };
    
    enc32(latI);
    enc32(lonI);
    
    // Altitude GPS (2 bytes: 0 a 65535m)
    buffer[offset++] = (gpsAlt >> 8) & 0xFF;
    buffer[offset++] = gpsAlt & 0xFF;
    
    // Número de Satélites (1 byte)
    buffer[offset++] = data.satellites;

    // 10. Status do Sistema (1 byte)
    buffer[offset++] = data.systemStatus;

    // **TAMANHO TOTAL AGORA É DE 34 BYTES** (4 bytes de Header + 30 bytes de Dados)
}

void PayloadManager::_encodeNodeData(const MissionData& node, uint8_t* buffer, int& offset) {
    buffer[offset++] = (node.nodeId >> 8) & 0xFF;
    buffer[offset++] = node.nodeId & 0xFF;
    buffer[offset++] = (uint8_t)constrain(node.soilMoisture, 0, 100);
    
    int16_t t = (int16_t)((node.ambientTemp + 50.0) * 10);
    buffer[offset++] = (t >> 8) & 0xFF; buffer[offset++] = t & 0xFF;
    
    buffer[offset++] = (uint8_t)constrain(node.humidity, 0, 100);
    buffer[offset++] = node.irrigationStatus;
    buffer[offset++] = (uint8_t)(node.rssi + 128);
}

// ============================================================================
// DECODERS E AUXILIARES (IMPLEMENTAÇÃO COMPLETA RESTAURADA)
// ============================================================================

bool PayloadManager::_decodeBinaryPayload(const String& hexPayload, MissionData& data) {
    // Decodifica Hex String vindo de nós de solo legados
    if (hexPayload.length() < 24) return false;
    
    uint8_t buffer[128];
    size_t len = hexPayload.length() / 2;
    
    // Converte String HEX para Array de Bytes
    for (size_t i = 0; i < len; i++) {
        // Pega 2 caracteres (ex: "AB")
        char byteStr[3] = { hexPayload.charAt(i*2), hexPayload.charAt(i*2+1), '\0' };
        // Converte para byte
        buffer[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }
    
    // Valida Header
    if (buffer[0] != 0xAB || buffer[1] != 0xCD) return false;

    int offset = 4;
    
    // Proteção de leitura
    if (len < offset + 8) return false;

    // Preenche estrutura MissionData
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
    // Decodifica formato: AGRO,Seq,ID,Solo,Temp,Umid,Irrig*Check
    String p = packet;
    
    // Remove Checksum para processar
    if (p.indexOf('*') > 0) p = p.substring(0, p.indexOf('*')); 
    p.remove(0, 5); // Remove "AGRO,"
    
    int idx = 0;
    String parts[10];
    
    // Split simples por vírgula
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
    
    if (idx < 4) return false; // Mínimo de campos não atingido

    data.sequenceNumber = parts[0].toInt();
    data.nodeId = parts[1].toInt();
    data.soilMoisture = parts[2].toFloat();
    data.ambientTemp = parts[3].toFloat();
    if (idx > 4) data.humidity = parts[4].toFloat();
    if (idx > 5) data.irrigationStatus = parts[5].toInt();
    
    // Lógica de perda de pacotes
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
    if (star == -1) return true; // Se não tiver checksum, aceita (modo debug)
    
    String content = packet.substring(0, star);
    String checkStr = packet.substring(star + 1);
    
    uint8_t calc = 0;
    for (unsigned int i = 0; i < content.length(); i++) calc ^= content.charAt(i);
    
    uint8_t received = strtol(checkStr.c_str(), NULL, 16);
    return (calc == received);
}

// === GESTÃO DO BUFFER DE NÓS (RESTAURADO) ===

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
    // Umidade crítica (muito seco ou muito úmido) ganha prioridade
    if (node.soilMoisture < 30 || node.soilMoisture > 90) priority += 5;
    // Sinal forte ganha prioridade (link mais confiável)
    if (node.rssi > -90) priority += 2;
    // Se houve perda recente, prioriza para recuperar histórico
    if (node.packetsLost > 0) priority += 2;
    
    return constrain(priority, 0, 10);
}

int PayloadManager::findNodeIndex(uint16_t nodeId) {
    // Busca índice existente
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == nodeId) return i;
    }
    // Se não achar, aloca novo slot vazio
    for (int i = 0; i < MAX_GROUND_NODES; i++) {
        if (_seqNodeId[i] == 0) {
            _seqNodeId[i] = nodeId;
            return i;
        }
    }
    return 0; // Fallback (sobrescreve o primeiro se cheio, não ideal mas seguro)
}