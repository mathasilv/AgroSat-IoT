/**
 * @file MissionController.cpp
 * @brief Implementação com recuperação de falhas (NVS)
 */

#include "MissionController.h"
#include "core/RTCManager/RTCManager.h" // Include real
#include "app/GroundNodeManager/GroundNodeManager.h" // Include real

MissionController::MissionController(RTCManager& rtc, GroundNodeManager& nodes) 
    : _rtc(rtc), _nodes(nodes), _active(false), _startTime(0), _startTimestampUTC(0)
{}

bool MissionController::begin() {
    _prefs.begin("mission", true); 
    bool wasActive = _prefs.getBool("active", false);
    uint32_t savedStartUTC = _prefs.getUInt("start_utc", 0);
    _prefs.end();

    if (wasActive && savedStartUTC > 0) {
        _active = true;
        _startTimestampUTC = savedStartUTC;
        
        if (_rtc.isInitialized()) {
            uint32_t currentUnix = _rtc.getUnixTime();
            uint32_t elapsedSec = (currentUnix > _startTimestampUTC) ? (currentUnix - _startTimestampUTC) : 0;
            _startTime = millis() - (elapsedSec * 1000);
        } else {
            _startTime = millis(); 
        }

        DEBUG_PRINTLN("[Mission] ⚠ MISSÃO RECUPERADA DE REINICIALIZAÇÃO!");
        return true;
    }
    
    return false;
}

bool MissionController::start() {
    if (_active) return false;
    
    DEBUG_PRINTLN("[Mission] === INICIANDO MISSÃO ===");
    
    if (_rtc.isInitialized()) {
        _startTimestampUTC = _rtc.getUnixTime();
    } else {
        _startTimestampUTC = 0;
    }
    
    _startTime = millis();
    _active = true;
    
    _saveState();
    return true;
}

bool MissionController::stop() {
    if (!_active) return false;
    
    DEBUG_PRINTLN("[Mission] === ENCERRANDO MISSÃO ===");
    _printStatistics();
    
    _active = false;
    _startTime = 0;
    _startTimestampUTC = 0;
    
    _clearState();
    return true;
}

void MissionController::_saveState() {
    _prefs.begin("mission", false);
    _prefs.putBool("active", true);
    _prefs.putUInt("start_utc", _startTimestampUTC);
    _prefs.end();
    DEBUG_PRINTLN("[Mission] Estado salvo na NVS.");
}

void MissionController::_clearState() {
    _prefs.begin("mission", false);
    _prefs.clear();
    _prefs.end();
    DEBUG_PRINTLN("[Mission] Estado limpo da NVS.");
}

uint32_t MissionController::getDuration() const {
    if (!_active) return 0;
    return millis() - _startTime;
}

void MissionController::_printStatistics() {
    const GroundNodeBuffer& buf = _nodes.buffer();
    DEBUG_PRINTF("[Mission] Nós: %u | Pacotes: %u\n", buf.activeNodes, buf.totalPacketsCollected);
    
    if (buf.activeNodes > 0) {
        int avgRSSI, best, worst;
        float avgSNR, loss;
        _calculateLinkStats(avgRSSI, best, worst, avgSNR, loss);
        DEBUG_PRINTF("[Mission] Link: RSSI %d dBm (Melhor %d), Perda %.1f%%\n", avgRSSI, best, loss);
    }
}

void MissionController::_calculateLinkStats(int& avgRSSI, int& bestRSSI, int& worstRSSI, float& avgSNR, float& packetLossRate) {
    const GroundNodeBuffer& buf = _nodes.buffer();
    long totalRSSI = 0;
    float totalSNR = 0;
    bestRSSI = -200; worstRSSI = 0;
    uint32_t totalLost = 0, totalRx = 0;

    for (int i = 0; i < buf.activeNodes; i++) {
        const MissionData& n = buf.nodes[i];
        totalRSSI += n.rssi;
        totalSNR += n.snr;
        if (n.rssi > bestRSSI) bestRSSI = n.rssi;
        if (n.rssi < worstRSSI) worstRSSI = n.rssi;
        totalLost += n.packetsLost;
        totalRx += n.packetsReceived;
    }

    if (buf.activeNodes > 0) {
        avgRSSI = totalRSSI / buf.activeNodes;
        avgSNR = totalSNR / buf.activeNodes;
    } else {
        avgRSSI = 0; avgSNR = 0;
    }
    
    uint32_t totalPkts = totalRx + totalLost;
    packetLossRate = (totalPkts > 0) ? ((float)totalLost / totalPkts * 100.0) : 0.0;
}