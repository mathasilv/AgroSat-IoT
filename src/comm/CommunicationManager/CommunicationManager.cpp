/**
 * @file CommunicationManager.cpp
 * @brief Implementação Completa do Gerenciador de Comunicação
 */

#include "CommunicationManager.h"

CommunicationManager::CommunicationManager() : 
    _loraEnabled(true), 
    _httpEnabled(true) 
{}

bool CommunicationManager::begin() {
    bool ok = true;
    if (!_lora.begin()) {
        DEBUG_PRINTLN("[CommManager] ERRO: LoRa falhou.");
        ok = false;
    }
    if (!_wifi.begin()) {
        DEBUG_PRINTLN("[CommManager] AVISO: WiFi falhou (não crítico).");
    }
    return ok;
}

void CommunicationManager::update() {
    _wifi.update();
    _payload.update();
}

bool CommunicationManager::isWiFiConnected() { 
    return _wifi.isConnected(); 
}

void CommunicationManager::connectWiFi() { 
    _wifi.begin(); 
}

bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) return false;
    return _lora.send(data);
}

bool CommunicationManager::sendLoRa(const uint8_t* data, size_t len) {
    if (!_loraEnabled) return false;
    return _lora.send(data, len, false);
}

bool CommunicationManager::receiveLoRaPacket(String& packet, int& rssi, float& snr) {
    if (!_loraEnabled) return false;
    return _lora.receive(packet, rssi, snr);
}

bool CommunicationManager::processLoRaPacket(const String& packet, MissionData& data) {
    return _payload.processLoRaPacket(packet, data);
}

// REMOVIDO: calculatePriority (lógica movida para uso direto no GroundNodeManager)

bool CommunicationManager::sendTelemetry(const TelemetryData& tData, 
                                         const GroundNodeBuffer& gBuffer) {
    bool success = false;
    uint8_t txBuffer[256]; 

    if (_loraEnabled) {
        if (tData.batteryPercentage < 20.0 || (tData.systemStatus & STATUS_BATTERY_CRIT)) {
            _lora.setTxPower(10);
            DEBUG_PRINTLN("[CommManager] Bateria baixa: Potência reduzida (10 dBm)");
        } else {
            _lora.setTxPower(LORA_TX_POWER);
        }

        int satLen = _payload.createSatellitePayload(tData, txBuffer);
        if (satLen > 0) {
            if (_lora.canTransmitNow(satLen)) {
                if (_lora.send(txBuffer, satLen, false)) {
                    success = true;
                    DEBUG_PRINTF("[CommManager] Telemetria satélite enviada: %d bytes\n", satLen);
                }
            } else {
                DEBUG_PRINTLN("[CommManager] Duty cycle: Telemetria satélite adiada.");
            }
        }

        std::vector<uint16_t> relayedNodes;
        int relayLen = _payload.createRelayPayload(tData, gBuffer, txBuffer, relayedNodes);
        
        if (relayLen > 0 && relayedNodes.size() > 0) {
            delay(200);
            if (_lora.canTransmitNow(relayLen)) {
                if (_lora.send(txBuffer, relayLen, false)) {
                    _payload.markNodesAsForwarded(
                        const_cast<GroundNodeBuffer&>(gBuffer), 
                        relayedNodes, 
                        tData.timestamp
                    );
                    
                    DEBUG_PRINTF("[CommManager] Relay enviado: %d nós, %d bytes\n", 
                                 relayedNodes.size(), relayLen);
                    
                    uint8_t crit, high, norm, low;
                    _payload.getPriorityStats(gBuffer, crit, high, norm, low);
                    DEBUG_PRINTF("[CommManager] QoS: CRIT=%d HIGH=%d NORM=%d LOW=%d\n",
                                 crit, high, norm, low);
                }
            } else {
                DEBUG_PRINTLN("[CommManager] Duty cycle: Relay adiado.");
            }
        }
    }
    
    if (_httpEnabled && _wifi.isConnected()) {
        String json = _payload.createTelemetryJSON(tData, gBuffer);
        if (_http.postJson(json)) {
            DEBUG_PRINTLN("[CommManager] Backup HTTP enviado com sucesso.");
        } else {
            DEBUG_PRINTLN("[CommManager] ERRO ao enviar backup HTTP.");
        }
    }

    return success;
}

void CommunicationManager::enableLoRa(bool enable) { 
    _loraEnabled = enable; 
    DEBUG_PRINTF("[CommManager] LoRa: %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

void CommunicationManager::enableHTTP(bool enable) { 
    _httpEnabled = enable; 
    DEBUG_PRINTF("[CommManager] HTTP: %s\n", enable ? "HABILITADO" : "DESABILITADO");
}