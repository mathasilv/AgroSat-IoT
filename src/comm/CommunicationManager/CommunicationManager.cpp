#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() 
    : _loraEnabled(true), _httpEnabled(true) {}

bool CommunicationManager::begin() {
    bool ok = true;
    if (!_lora.begin()) ok = false;
    if (!_wifi.begin()) ok = false; // WiFi não-bloqueante
    return ok;
}

void CommunicationManager::update() {
    _wifi.update();
    _payload.update();
}

// === WiFi ===

bool CommunicationManager::isWiFiConnected() {
    return _wifi.isConnected();
}

void CommunicationManager::connectWiFi() {
    _wifi.begin();
}

// === LoRa ===

bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) return false;
    return _lora.send(data);
}

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraEnabled) return false;
    return _lora.receive(packet, rssi, snr);
}

// === Missão ===

bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    return _payload.processLoRaPacket(packet, data);
}

uint8_t CommunicationManager::calculatePriority(const MissionData& node) {
    return _payload.calculateNodePriority(node);
}

bool CommunicationManager::sendTelemetry(const TelemetryData& tData, const GroundNodeBuffer& gBuffer) {
    bool success = false;

    // 1. Envia via LoRa (Satélite + Relay)
    if (_loraEnabled) {
        // === MELHORIA: Controle Dinâmico de Potência ===
        // Se a bateria estiver crítica ou abaixo de 20%, reduz potência para 10dBm
        if (tData.batteryPercentage < 20.0 || (tData.systemStatus & STATUS_BATTERY_CRIT)) {
            _lora.setTxPower(10); 
        } else {
            _lora.setTxPower(LORA_TX_POWER); // Potência máxima definida no config.h
        }

        String satPayload = _payload.createSatellitePayload(tData);
        if (_lora.send(satPayload)) success = true;

        std::vector<uint16_t> relayedNodes;
        String relayPayload = _payload.createRelayPayload(tData, gBuffer, relayedNodes);
        
        if (relayPayload.length() > 0) {
            delay(200); // Pausa para não congestionar buffer TX
            if (_lora.send(relayPayload)) {
                _payload.markNodesAsForwarded(const_cast<GroundNodeBuffer&>(gBuffer), relayedNodes);
            }
        }
    }

    // 2. Envia via HTTP (Backup)
    if (_httpEnabled && _wifi.isConnected()) {
        String json = _payload.createTelemetryJSON(tData, gBuffer);
        if (_http.postJson(json)) success = true;
    }

    return success;
}

// === Controle ===

void CommunicationManager::enableLoRa(bool enable) { _loraEnabled = enable; }
void CommunicationManager::enableHTTP(bool enable) { _httpEnabled = enable; }