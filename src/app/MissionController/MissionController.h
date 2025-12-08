/**
 * @file MissionController.h
 * @brief Controlador de ciclo de vida de missão
 */

#ifndef MISSION_CONTROLLER_H
#define MISSION_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Forward declarations (evita include circular)
class RTCManager;
class GroundNodeManager;

class MissionController {
public:
    MissionController(RTCManager& rtc, GroundNodeManager& nodes);
    
    bool begin(); 
    bool start();
    bool stop();
    
    bool isActive() const { return _active; }
    uint32_t getDuration() const;
    uint32_t getStartTimestamp() const { return _startTimestampUTC; }
    
private:
    RTCManager& _rtc;
    GroundNodeManager& _nodes;
    Preferences _prefs;
    
    bool _active;
    unsigned long _startTime;
    uint32_t _startTimestampUTC;
    
    void _printStatistics();
    void _saveState();
    void _clearState();
    
    void _calculateLinkStats(int& avgRSSI, int& bestRSSI, int& worstRSSI,
                             float& avgSNR, float& packetLossRate);
};

#endif