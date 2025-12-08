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
        // Não marca como erro crítico, WiFi é opcional
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

// ============================================================================
// ENVIO LoRa (String - Legado)
// ============================================================================
bool CommunicationManager::sendLoRa(const String& data) {
    if (!_loraEnabled) return false;
    return _lora.send(data);
}

// ============================================================================
// ENVIO LoRa (Binário - NOVO)
// ============================================================================
bool CommunicationManager::sendLoRa(const uint8_t* data, size_t len) {
    if (!_loraEnabled) return false;
    
    // Usa o método binário do LoRaService (com duty cycle check)
    return _lora.send(data, len, false);  // false = sem criptografia por padrão
}

// ============================================================================
// RECEPÇÃO LoRa
// ============================================================================
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

// ============================================================================
// ENVIO DE TELEMETRIA (Satélite + Store-and-Forward)
// ============================================================================
bool CommunicationManager::sendTelemetry(const TelemetryData& tData, 
                                         const GroundNodeBuffer& gBuffer) {
    bool success = false;
    uint8_t txBuffer[256]; 

    if (_loraEnabled) {
        // 1. Controle de Potência Dinâmico (economia de energia)
        if (tData.batteryPercentage < 20.0 || (tData.systemStatus & STATUS_BATTERY_CRIT)) {
            _lora.setTxPower(10);  // Reduz para 10 dBm
            DEBUG_PRINTLN("[CommManager] Bateria baixa: Potência reduzida (10 dBm)");
        } else {
            _lora.setTxPower(LORA_TX_POWER);  // Potência normal (20 dBm)
        }

        // 2. Payload Satélite (Downlink de Telemetria própria)
        int satLen = _payload.createSatellitePayload(tData, txBuffer);
        if (satLen > 0) {
            // Verificar duty cycle antes de enviar
            if (_lora.canTransmitNow(satLen)) {
                if (_lora.send(txBuffer, satLen, false)) {
                    success = true;
                    DEBUG_PRINTF("[CommManager] Telemetria satélite enviada: %d bytes\n", satLen);
                }
            } else {
                DEBUG_PRINTLN("[CommManager] Duty cycle: Telemetria satélite adiada.");
            }
        }

        // 3. Payload Relay (Store-and-Forward) com QoS Priority
        std::vector<uint16_t> relayedNodes;
        int relayLen = _payload.createRelayPayload(tData, gBuffer, txBuffer, relayedNodes);
        
        if (relayLen > 0 && relayedNodes.size() > 0) {
            delay(200);  // Pequeno delay entre transmissões
            
            // Verificar duty cycle
            if (_lora.canTransmitNow(relayLen)) {
                if (_lora.send(txBuffer, relayLen, false)) {
                    // Marcar nós como retransmitidos (com timestamp)
                    _payload.markNodesAsForwarded(
                        const_cast<GroundNodeBuffer&>(gBuffer), 
                        relayedNodes, 
                        tData.timestamp
                    );
                    
                    DEBUG_PRINTF("[CommManager] Relay enviado: %d nós, %d bytes\n", 
                                 relayedNodes.size(), relayLen);
                    
                    // Log de priorização (NOVO 5.3)
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
    
    // 4. HTTP Backup (se WiFi disponível)
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

// ============================================================================
// CONTROLE DE LINKS
// ============================================================================
void CommunicationManager::enableLoRa(bool enable) { 
    _loraEnabled = enable; 
    DEBUG_PRINTF("[CommManager] LoRa: %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

void CommunicationManager::enableHTTP(bool enable) { 
    _httpEnabled = enable; 
    DEBUG_PRINTF("[CommManager] HTTP: %s\n", enable ? "HABILITADO" : "DESABILITADO");
}