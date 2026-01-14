/**
 * @file CommunicationManager.cpp
 * @brief Implementação (FIX: Removido const_cast perigoso)
 * @version 11.1.0
 */

#include "CommunicationManager.h"
#include "Globals.h" 

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
    if (_httpEnabled) {
        _wifi.begin();
    } else {
        DEBUG_PRINTLN("[CommManager] HTTP desabilitado por configuracao.");
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

// FIX: Agora recebe referência não-const, eliminando const_cast
bool CommunicationManager::sendTelemetry(const TelemetryData& tData, 
                                         GroundNodeBuffer& gBuffer) {
    bool success = false;
    uint8_t txBuffer[256]; 

    // 1. Processamento LoRa
    if (_loraEnabled) {
        if (tData.batteryPercentage < 20.0 || (tData.systemStatus & STATUS_BATTERY_CRIT)) {
            _lora.setTxPower(10);
        } else {
            _lora.setTxPower(LORA_TX_POWER);
        }

        // Payload Satélite
        int satLen = _payload.createSatellitePayload(tData, txBuffer);
        if (satLen > 0) {
            if (_lora.canTransmitNow(satLen)) {
                if (_lora.send(txBuffer, satLen, false)) {
                    success = true;
                }
            } else {
                DEBUG_PRINTLN("[Comm] Duty Cycle cheio. TX adiado.");
            }
        }

        // Payload Relay
        std::vector<uint16_t> relayedNodes;
        int relayLen = _payload.createRelayPayload(tData, gBuffer, txBuffer, relayedNodes);
        
        if (relayLen > 0 && relayedNodes.size() > 0) {
            delay(200); // Pequeno delay entre pacotes
            
            if (_lora.canTransmitNow(relayLen)) {
                if (_lora.send(txBuffer, relayLen, false)) {
                    // FIX: Agora gBuffer é não-const, sem necessidade de const_cast
                    _payload.markNodesAsForwarded(gBuffer, relayedNodes, tData.timestamp);
                    DEBUG_PRINTF("[Comm] Relay: %d nodes\n", relayedNodes.size());
                }
            }
        }
    }
    
    // 2. Processamento HTTP (Async)
    if (_httpEnabled) {
        if (xHttpQueue != NULL) {
            HttpQueueMessage msg;
            msg.data = tData;       
            msg.nodes = gBuffer;    
            
            if (xQueueSend(xHttpQueue, &msg, 0) == pdTRUE) {
                // Sucesso
            } else {
                DEBUG_PRINTLN("[CommManager] Fila HTTP cheia.");
            }
        }
    }

    return success;
}

void CommunicationManager::processHttpQueuePacket(const HttpQueueMessage& packet) {
    if (_wifi.isConnected()) {
        String json = _payload.createTelemetryJSON(packet.data, packet.nodes);
        if (_http.postJson(json)) {
            DEBUG_PRINTLN("[CommManager] Backup HTTP enviado com sucesso (Async).");
        } else {
            DEBUG_PRINTLN("[CommManager] ERRO envio HTTP.");
        }
    }
}

void CommunicationManager::enableLoRa(bool enable) { 
    _loraEnabled = enable; 
}

void CommunicationManager::enableHTTP(bool enable) { 
    _httpEnabled = enable; 
}
