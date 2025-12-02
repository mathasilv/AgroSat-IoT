/**
 * @file CommunicationManager.cpp
 * @brief Comunicação DUAL MODE com CBOR + Otimizações LoRa
 * @version 8.0.0
 * @date 2025-12-01
 * 
 * CHANGELOG v8.0.0:
 * - [REFACTOR] Integração completa com PayloadManager
 * - [CLEAN] Removidas 550+ linhas de código duplicado
 * - [IMPROVE] Separação clara de responsabilidades
 */

#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() :
    _lora(),
    _http(),
    _wifi(),
    _payloadManager(),      // Gerencia TODOS os payloads
    _packetsSent(0),
    _packetsFailed(0),
    _totalRetries(0),
    _httpEnabled(true)
{}

// ========================================
// INICIALIZAÇÃO
// ========================================

bool CommunicationManager::begin() {
    DEBUG_PRINTLN("[CommunicationManager] Inicializando DUAL MODE");

    bool loraOk = _lora.begin();
    bool wifiOk = _wifi.begin();

    DEBUG_PRINTF("[CommunicationManager] LoRa=%s, WiFi=%s\n",
                 loraOk ? "OK" : "FALHOU",
                 wifiOk ? "OK" : "FALHOU");

    return loraOk || wifiOk;
}

bool CommunicationManager::initLoRa() {
    return _lora.begin();
}

bool CommunicationManager::retryLoRaInit(uint8_t maxAttempts) {
    return _lora.retryInit(maxAttempts);
}

// ========================================
// TRANSMISSÃO TELEMETRIA (ORQUESTRAÇÃO)
// ========================================

bool CommunicationManager::sendTelemetry(const TelemetryData& data,
                                         const GroundNodeBuffer& groundBuffer) {
    _lora.adaptSpreadingFactor(data.altitude);

    bool loraSuccess = false;
    bool httpSuccess = false;

    // FASE 1: Telemetria Satélite
    if (_lora.isOnline()) {
        String satPayload = _payloadManager.createSatellitePayload(data);
        if (satPayload.length() > 0 && _lora.send(satPayload)) {
            loraSuccess = true;
            DEBUG_PRINTLN("Satélite TX OK");
        }
    }

    // FASE 2: Relay Nós Terrestres
    if (_lora.isOnline()) {
        std::vector<uint16_t> relayNodes;
        String relayPayload = _payloadManager.createRelayPayload(data, groundBuffer, relayNodes);
        if (relayPayload.length() > 0 && _lora.send(relayPayload)) {
            _payloadManager.markNodesAsForwarded(const_cast<GroundNodeBuffer&>(groundBuffer), relayNodes);
            loraSuccess = true;
            DEBUG_PRINTLN("Relay TX OK");
        }
    }

    // FASE 3: HTTP Backup (OBSAT Server)
    if (_httpEnabled && _wifi.isConnected()) {
        String jsonPayload = _payloadManager.createTelemetryJSON(data, groundBuffer);
        if (jsonPayload.length() > 0 && jsonPayload.length() <= JSON_MAX_SIZE) {
            for (uint8_t attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
                if (attempt > 0) {
                    _totalRetries++;
                    delay(1000);
                }
                if (_http.postJson(jsonPayload)) {
                    _packetsSent++;
                    httpSuccess = true;
                    DEBUG_PRINTLN("HTTP OK");
                    break;
                }
            }
            if (!httpSuccess) {
                _packetsFailed++;
                DEBUG_PRINTLN("✗ HTTP falhou");
            }
        }
    }

    return loraSuccess || httpSuccess;
}

// ========================================
// LORA - RECEPÇÃO E STATUS
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
    return _payloadManager.processLoRaPacket(packet, data);
}

// ========================================
// WIFI - GERENCIAMENTO
// ========================================

bool CommunicationManager::connectWiFi() {
    return _wifi.connect();
}

void CommunicationManager::disconnectWiFi() {
    _wifi.disconnect();
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

bool CommunicationManager::testConnection() {
    if (!_wifi.isConnected()) return false;

    HTTPClient http;
    http.begin("https://obsat.org.br/testepost/index.php");
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    return httpCode == 200;
}

// ========================================
// CONTROLE E CONFIGURAÇÃO
// ========================================

void CommunicationManager::enableLoRa(bool enable) {
    _lora.enable(enable);
}

void CommunicationManager::enableHTTP(bool enable) {
    _httpEnabled = enable;
    DEBUG_PRINTF("[CommunicationManager] HTTP %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

void CommunicationManager::reconfigureLoRa(OperationMode mode) {
    _lora.reconfigure(mode);
}

// ========================================
// MISSÃO
// ========================================

MissionData CommunicationManager::getLastMissionData() {
    return _payloadManager.getLastMissionData();
}

void CommunicationManager::markNodesAsForwarded(GroundNodeBuffer& buffer, 
                                               const std::vector<uint16_t>& nodeIds) {
    _payloadManager.markNodesAsForwarded(buffer, nodeIds);
}

uint8_t CommunicationManager::calculatePriority(const MissionData& node) {
    return _payloadManager.calculateNodePriority(node);
}

bool CommunicationManager::sendLoRa(const String& data) {
    return _lora.send(data);
}

// ========================================
// ATUALIZAÇÃO PERIÓDICA
// ========================================

void CommunicationManager::update() {
    _wifi.update();
    _payloadManager.update();
}

// ========================================
// ESTATÍSTICAS HTTP
// ========================================

void CommunicationManager::getStatistics(uint16_t& sent, uint16_t& failed, uint16_t& retries) {
    sent = _packetsSent;
    failed = _packetsFailed;
    retries = _totalRetries;
}
