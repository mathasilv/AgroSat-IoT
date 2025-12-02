/**
 * @file MissionController.cpp
 * @brief Implementação do controlador de missão HAB
 * @version 1.0.0
 * @date 2025-12-01
 */

#include "MissionController.h"

MissionController::MissionController(
    RTCManager& rtc, 
    GroundNodeManager& nodes
) : 
    _rtc(rtc),
    _nodes(nodes),
    _active(false),
    _startTime(0),
    _startTimestampUTC(0)
{
}

bool MissionController::start() {
    if (_active) {
        DEBUG_PRINTLN("[MissionController] Missao ja em andamento");
        return false;
    }
    
    DEBUG_PRINTLN("[MissionController] ==========================================");
    DEBUG_PRINTLN("[MissionController] INICIANDO MISSAO HAB");
    DEBUG_PRINTLN("[MissionController] ==========================================");
    
    // Capturar timestamp UTC
    if (_rtc.isInitialized()) {
        _startTimestampUTC = _rtc.getUnixTime();
        DEBUG_PRINTF("[MissionController] Inicio UTC: %s (unix: %lu)\n", 
                     _rtc.getUTCDateTime().c_str(), 
                     _startTimestampUTC);
        DEBUG_PRINTF("[MissionController] Local: %s\n", 
                     _rtc.getLocalDateTime().c_str());
    } else {
        _startTimestampUTC = millis() / 1000;
        DEBUG_PRINTLN("[MissionController] RTC nao disponivel, usando millis");
    }
    
    // Marcar início
    _startTime = millis();
    _active = true;
    
    DEBUG_PRINTLN("[MissionController] Missao iniciada, coleta ativa");
    DEBUG_PRINTLN("[MissionController] ==========================================");
    
    return true;
}

bool MissionController::stop() {
    if (!_active) {
        DEBUG_PRINTLN("[MissionController] Nenhuma missao ativa");
        return false;
    }
    
    DEBUG_PRINTLN("[MissionController] ==========================================");
    DEBUG_PRINTLN("[MissionController] ENCERRANDO MISSAO HAB");
    DEBUG_PRINTLN("[MissionController] ==========================================");
    
    uint32_t duration = getDuration();
    
    // Timestamp final
    if (_rtc.isInitialized()) {
        DEBUG_PRINTF("[MissionController] Fim UTC: %s (unix: %lu)\n",
                     _rtc.getUTCDateTime().c_str(),
                     _rtc.getUnixTime());
    }
    
    // Duração
    DEBUG_PRINTF("[MissionController] Duracao: %lu min %lu s\n",
                 duration / 60000,
                 (duration % 60000) / 1000);
    
    // Estatísticas dos nós
    _printStatistics();
    
    // Desativar
    _active = false;
    _startTime = 0;
    _startTimestampUTC = 0;
    
    DEBUG_PRINTLN("[MissionController] Missao encerrada");
    DEBUG_PRINTLN("[MissionController] ==========================================");
    
    return true;
}

uint32_t MissionController::getDuration() const {
    if (!_active) {
        return 0;
    }
    return millis() - _startTime;
}

void MissionController::_printStatistics() {
    const GroundNodeBuffer& buf = _nodes.buffer();
    
    DEBUG_PRINTF("[MissionController] Nos coletados: %u\n", buf.activeNodes);
    DEBUG_PRINTF("[MissionController] Pacotes recebidos: %u\n", buf.totalPacketsCollected);
    
    if (buf.activeNodes == 0) {
        DEBUG_PRINTLN("[MissionController] Nenhum no terrestre detectado");
        return;
    }
    
    // Calcular estatísticas de link
    int avgRSSI, bestRSSI, worstRSSI;
    float avgSNR, packetLossRate;
    
    _calculateLinkStats(avgRSSI, bestRSSI, worstRSSI, avgSNR, packetLossRate);
    
    DEBUG_PRINTF("[MissionController] Link RSSI: medio=%d dBm, melhor=%d dBm, pior=%d dBm\n",
                 avgRSSI, bestRSSI, worstRSSI);
    DEBUG_PRINTF("[MissionController] Link SNR: medio=%.1f dB\n", avgSNR);
    DEBUG_PRINTF("[MissionController] Perda de pacotes: %.2f%%\n", packetLossRate);
}

void MissionController::_calculateLinkStats(
    int& avgRSSI, 
    int& bestRSSI, 
    int& worstRSSI,
    float& avgSNR,
    float& packetLossRate
) {
    const GroundNodeBuffer& buf = _nodes.buffer();
    
    avgRSSI = 0;
    avgSNR = 0.0f;
    bestRSSI = -200;
    worstRSSI = 0;
    
    uint16_t totalLost = 0;
    uint16_t totalReceived = 0;
    
    for (uint8_t i = 0; i < buf.activeNodes; i++) {
        const MissionData& node = buf.nodes[i];
        
        avgRSSI += node.rssi;
        avgSNR += node.snr;
        
        if (node.rssi > bestRSSI) bestRSSI = node.rssi;
        if (node.rssi < worstRSSI) worstRSSI = node.rssi;
        
        totalLost += node.packetsLost;
        totalReceived += node.packetsReceived;
    }
    
    // Médias
    if (buf.activeNodes > 0) {
        avgRSSI /= buf.activeNodes;
        avgSNR /= buf.activeNodes;
    }
    
    // Taxa de perda
    uint16_t totalPackets = totalReceived + totalLost;
    packetLossRate = (totalPackets > 0) 
        ? (totalLost * 100.0f) / totalPackets 
        : 0.0f;
}
