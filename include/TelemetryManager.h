/**
 * @file TelemetryManager.h
 * @brief VERSÃO COM STORE-AND-FORWARD LEO INTEGRADO
 * @version 6.0.0
 */
#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SSD1306Wire.h"
#include "DisplayManager.h"
#include "SystemHealth.h"
#include "PowerManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "CommunicationManager.h"
#include "RTCManager.h"

class TelemetryManager {
public:
    TelemetryManager();
    bool begin();
    void loop();
    void startMission();
    void stopMission();
    OperationMode getMode();
    void updateDisplay();

    void applyModeConfig(OperationMode mode);
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();
    void enableLoRa(bool enable) { _comm.enableLoRa(enable); }
    void enableHTTP(bool enable) { _comm.enableHTTP(enable); }
    bool isLoRaEnabled() const { return _comm.isLoRaEnabled(); }
    bool isHTTPEnabled() const { return _comm.isHTTPEnabled(); }
    CommunicationManager& getCommunicationManager() { return _comm; }

private:
    SSD1306Wire _display;            
    DisplayManager _displayMgr;      
    SystemHealth _health;
    PowerManager _power;
    SensorManager _sensors;
    StorageManager _storage;
    CommunicationManager _comm;
    RTCManager _rtc;
    
    TelemetryData _telemetryData;
    GroundNodeBuffer _groundNodeBuffer;
    
    OperationMode _mode;
    bool _missionActive;
    uint32_t _missionStartTime;
    
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    uint32_t _minHeapSeen;
    
    bool _useNewDisplay;           

    void _collectTelemetryData();
    void _sendTelemetry();
    void _saveToStorage();
    void _checkOperationalConditions();
    void _displayStatus();            
    void _displayTelemetry();         
    void _displayError(const String& error);
    void _logHeapUsage(const String& component);
    void _monitorHeap();
    bool _initI2CBus();
    
    void _updateGroundNode(const MissionData& data);
    void _cleanupStaleNodes(unsigned long maxAge = NODE_TTL_MS);
    void _manageOrbitalPass();
    void _prepareForward();
    void _replaceLowestPriorityNode(const MissionData& newData);
};

#endif
