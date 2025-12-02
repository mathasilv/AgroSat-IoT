/**
 * @file MissionController.h
 * @brief Controlador de ciclo de vida de missão (Com Persistência)
 */

#ifndef MISSION_CONTROLLER_H
#define MISSION_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "core/RTCManager/RTCManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"

class MissionController {
public:
    MissionController(RTCManager& rtc, GroundNodeManager& nodes);
    
    bool begin(); // Novo: Carrega estado anterior
    bool start();
    bool stop();
    
    bool isActive() const { return _active; }
    uint32_t getDuration() const;
    uint32_t getStartTimestamp() const { return _startTimestampUTC; }
    
private:
    RTCManager& _rtc;
    GroundNodeManager& _nodes;
    Preferences _prefs; // Para salvar estado na Flash
    
    bool _active;
    unsigned long _startTime;    // millis() do início (recalculado no boot)
    uint32_t _startTimestampUTC; // Unix timestamp fixo do início
    
    void _printStatistics();
    void _saveState();   // Salva na NVS
    void _clearState();  // Limpa da NVS
    
    void _calculateLinkStats(int& avgRSSI, int& bestRSSI, int& worstRSSI,
                             float& avgSNR, float& packetLossRate);
};

#endif