#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() : _loraEnabled(true), _httpEnabled(true) {}

bool CommunicationManager::begin() {
    bool ok = true;
    if (!_lora.begin()) ok = false;
    if (!_wifi.begin()) ok = false; 
    return ok;
}

void CommunicationManager::update() {
    _wifi.update();
    _payload.update();
}

bool CommunicationManager::isWiFiConnected() { return _wifi.isConnected(); }
void CommunicationManager::connectWiFi() { _wifi.begin(); }

bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) return false;
    return _lora.send(data);
}

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraEnabled) return false;
    return _lora.receive(packet, rssi, snr);
}

bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    return _payload.processLoRaPacket(packet, data);
}

uint8_t CommunicationManager::calculatePriority(const MissionData& node) {
    return _payload.calculateNodePriority(node);
}

bool CommunicationManager::sendTelemetry(const TelemetryData& tData, const GroundNodeBuffer& gBuffer) {
    bool success = false;
    uint8_t txBuffer[256]; 

    if (_loraEnabled) {
        // 1. Controle de Potência
        if (tData.batteryPercentage < 20.0 || (tData.systemStatus & STATUS_BATTERY_CRIT)) {
            _lora.setTxPower(10); 
        } else {
            _lora.setTxPower(LORA_TX_POWER);
        }

        // 2. Payload Satélite (Downlink de Telemetria)
        int satLen = _payload.createSatellitePayload(tData, txBuffer);
        if (satLen > 0) {
            if (_lora.send(txBuffer, satLen)) success = true;
        }

        // 3. Payload Relay (Store-and-Forward)
        std::vector<uint16_t> relayedNodes;
        int relayLen = _payload.createRelayPayload(tData, gBuffer, txBuffer, relayedNodes);
        
        if (relayLen > 0) {
            delay(200); 
            if (_lora.send(txBuffer, relayLen)) {
                // Ao enviar com sucesso, marcamos o momento exato (Retransmission Timestamp)
                // Usamos tData.timestamp que é sincronizado com o RTC/GPS
                _payload.markNodesAsForwarded(const_cast<GroundNodeBuffer&>(gBuffer), relayedNodes, tData.timestamp);
            }
        }
    }
    
    // HTTP Backup
    if (_httpEnabled && _wifi.isConnected()) {
        String json = _payload.createTelemetryJSON(tData, gBuffer);
        _http.postJson(json);
    }

    return success;
}

void CommunicationManager::enableLoRa(bool enable) { _loraEnabled = enable; }
void CommunicationManager::enableHTTP(bool enable) { _httpEnabled = enable; }