/**
 * @file PayloadManager.cpp
 * @brief Implementação do gerenciador de payload (missão LoRa)
 */

#include "PayloadManager.h"

SPIClass spiLoRa(VSPI);

PayloadManager::PayloadManager() :
    _online(false),
    _lastPacketTime(0),
    _packetTimeout(60000),
    _expectedSeqNum(0)
{
    // Inicializar dados da missão
    _missionData.soilMoisture = 0.0;
    _missionData.ambientTemp = 0.0;
    _missionData.humidity = 0.0;
    _missionData.irrigationStatus = 0;
    _missionData.rssi = 0;
    _missionData.snr = 0.0;
    _missionData.packetsReceived = 0;
    _missionData.packetsLost = 0;
    _missionData.lastLoraRx = 0;
}

bool PayloadManager::begin() {
    DEBUG_PRINTLN("[PayloadManager] Inicializando módulo LoRa no VSPI...");
    
    // CRÍTICO: Inicializar VSPI com pinos do LoRa
    spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    
    // Configurar LoRa para usar SPIClass dedicado
    LoRa.setSPI(spiLoRa);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    // Inicializar LoRa com frequência configurada
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[PayloadManager] ERRO: Falha ao inicializar LoRa!");
        _online = false;
        return false;
    }
    
    // Configurações LoRa otimizadas para longo alcance
    LoRa.setSpreadingFactor(12);      // SF12 - máximo alcance
    LoRa.setSignalBandwidth(125E3);   // 125 kHz
    LoRa.setCodingRate4(8);           // CR 4/8 - máxima correção de erros
    LoRa.setPreambleLength(8);        // Preâmbulo
    LoRa.setSyncWord(0x34);           // Palavra de sincronização privada
    LoRa.enableCrc();                 // Habilitar CRC
    LoRa.setTxPower(20);              // Potência máxima (20 dBm)
    
    _online = true;
    
    DEBUG_PRINTF("[PayloadManager] LoRa OK - Freq: %.2f MHz, SF: 12\n", 
                 LORA_FREQUENCY / 1E6);
    
    // Colocar em modo recepção contínua
    LoRa.receive();
    
    return true;
}

void PayloadManager::update() {
    if (!_online) return;
    
    // Verificar se há pacotes disponíveis
    int packetSize = LoRa.parsePacket();
    
    if (packetSize > 0) {
        // Ler pacote
        String packet = "";
        while (LoRa.available()) {
            packet += (char)LoRa.read();
        }
        
        // Obter RSSI e SNR
        _missionData.rssi = LoRa.packetRssi();
        _missionData.snr = LoRa.packetSnr();
        
        DEBUG_PRINTF("[PayloadManager] Pacote recebido (%d bytes): %s\n", 
                    packetSize, packet.c_str());
        DEBUG_PRINTF("  RSSI: %d dBm, SNR: %.2f dB\n", 
                    _missionData.rssi, _missionData.snr);
        
        // Processar pacote
        if (_processPacket(packet)) {
            _missionData.packetsReceived++;
            _missionData.lastLoraRx = millis();
            _lastPacketTime = millis();
        } else {
            DEBUG_PRINTLN("[PayloadManager] Pacote inválido ou corrompido");
        }
    }
    
    // Verificar timeout de pacotes (detectar pacotes perdidos)
    uint32_t currentTime = millis();
    if (_lastPacketTime > 0 && (currentTime - _lastPacketTime) > _packetTimeout) {
        // Incrementar contador de pacotes perdidos
        if (_expectedSeqNum > _missionData.packetsReceived) {
            _missionData.packetsLost = _expectedSeqNum - _missionData.packetsReceived;
        }
    }
}

MissionData PayloadManager::getMissionData() {
    return _missionData;
}

String PayloadManager::generatePayload() {
    // Gerar payload JSON compacto (máximo 90 bytes)
    // Formato: {"sm":45.2,"t":28.5,"h":65.3,"ir":1,"rs":-85,"sn":8.5}
    
    String payload = "{";
    payload += "\"sm\":" + String(_missionData.soilMoisture, 1) + ",";
    payload += "\"t\":" + String(_missionData.ambientTemp, 1) + ",";
    payload += "\"h\":" + String(_missionData.humidity, 1) + ",";
    payload += "\"ir\":" + String(_missionData.irrigationStatus) + ",";
    payload += "\"rs\":" + String(_missionData.rssi) + ",";
    payload += "\"sn\":" + String(_missionData.snr, 1) + ",";
    payload += "\"rx\":" + String(_missionData.packetsReceived) + ",";
    payload += "\"ls\":" + String(_missionData.packetsLost);
    payload += "}";
    
    // Garantir que não exceda 90 bytes
    if (payload.length() > PAYLOAD_MAX_SIZE) {
        DEBUG_PRINTF("[PayloadManager] AVISO: Payload excede limite (%d bytes)\n", 
                    payload.length());
        payload = payload.substring(0, PAYLOAD_MAX_SIZE);
    }
    
    return payload;
}

bool PayloadManager::sendCommand(const String& command) {
    if (!_online) return false;
    
    DEBUG_PRINTF("[PayloadManager] Enviando comando: %s\n", command.c_str());
    
    // Enviar comando via LoRa
    LoRa.beginPacket();
    LoRa.print(command);
    LoRa.endPacket();
    
    // Voltar para modo recepção
    LoRa.receive();
    
    return true;
}

bool PayloadManager::isOnline() {
    return _online;
}

int16_t PayloadManager::getLastRSSI() {
    return _missionData.rssi;
}

float PayloadManager::getLastSNR() {
    return _missionData.snr;
}

void PayloadManager::getStatistics(uint16_t& received, uint16_t& lost, uint16_t& sent) {
    received = _missionData.packetsReceived;
    lost = _missionData.packetsLost;
    sent = 0;  // Implementar contador de envio se necessário
}

void PayloadManager::setSpreadingFactor(uint8_t sf) {
    if (sf < 6 || sf > 12) return;
    LoRa.setSpreadingFactor(sf);
    DEBUG_PRINTF("[PayloadManager] Spreading Factor: %d\n", sf);
}

void PayloadManager::setBandwidth(long bandwidth) {
    LoRa.setSignalBandwidth(bandwidth);
    DEBUG_PRINTF("[PayloadManager] Bandwidth: %ld Hz\n", bandwidth);
}

void PayloadManager::setTxPower(uint8_t power) {
    if (power < 2 || power > 20) return;
    LoRa.setTxPower(power);
    DEBUG_PRINTF("[PayloadManager] TX Power: %d dBm\n", power);
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

bool PayloadManager::_processPacket(String packet) {
    // Validar checksum (se implementado)
    if (!_validateChecksum(packet)) {
        return false;
    }
    
    // Parsear dados agrícolas
    // Formato esperado: "AGRO,seq,sm,temp,hum,irr"
    // Exemplo: "AGRO,123,45.2,28.5,65.3,1"
    
    if (!packet.startsWith("AGRO,")) {
        DEBUG_PRINTLN("[PayloadManager] Formato de pacote inválido");
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
    if (fieldIndex < 6) {
        fields[fieldIndex] = packet;  // Último campo
    }
    
    // Extrair dados
    if (fieldIndex >= 4) {
        uint16_t seqNum = fields[0].toInt();
        _missionData.soilMoisture = fields[1].toFloat();
        _missionData.ambientTemp = fields[2].toFloat();
        _missionData.humidity = fields[3].toFloat();
        _missionData.irrigationStatus = fields[4].toInt();
        
        // Verificar sequência (detectar pacotes perdidos)
        if (seqNum != _expectedSeqNum) {
            if (seqNum > _expectedSeqNum) {
                _missionData.packetsLost += (seqNum - _expectedSeqNum);
            }
        }
        _expectedSeqNum = seqNum + 1;
        
        return true;
    }
    
    return false;
}

bool PayloadManager::_validateChecksum(const String& packet) {
    // Implementação simplificada - sempre válido
    // Em produção, implementar checksum real (CRC16, etc.)
    return true;
}

bool PayloadManager::_parseAgroData(const String& data) {
    // Implementação adicional se necessário
    return true;
}
