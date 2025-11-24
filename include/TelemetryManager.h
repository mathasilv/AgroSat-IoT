/**
 * @file TelemetryManager.h
 */
#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>
#include <mutex>
#include "config.h"
#include "CommunicationManager.h"
#include "SensorManager.h"
#include "PowerManager.h"
#include "StorageManager.h"
#include "SystemHealth.h"
#include "DisplayManager.h"
#include "RTCManager.h"
#include "ButtonHandler.h"  // ← NOVO

class TelemetryManager {
public:
    TelemetryManager();
    
    bool begin();
    void loop();
    
    void startMission();
    void stopMission();
    OperationMode getMode();
    
    void applyModeConfig(OperationMode mode);
    void enableLoRa(bool enable) { _comm.enableLoRa(enable); }
    void enableHTTP(bool enable) { _comm.enableHTTP(enable); }
    bool isLoRaEnabled() { return _comm.isLoRaEnabled(); }
    bool isHTTPEnabled() { return _comm.isHTTPEnabled(); }
    
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();
    void updateDisplay();

private:
    CommunicationManager _comm;
    SensorManager _sensors;
    PowerManager _power;
    StorageManager _storage;
    SystemHealth _health;
    DisplayManager _displayMgr;
    RTCManager _rtc;
    ButtonHandler _button;  // ← NOVO
    
    std::mutex _bufferMutex;
    
    OperationMode _mode;
    bool _missionActive;
    unsigned long _missionStartTime;
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    uint32_t _minHeapSeen;
    bool _useNewDisplay;
    
    TelemetryData _telemetryData;
    GroundNodeBuffer _groundNodeBuffer;
    
    bool _initI2CBus();
    void _collectTelemetryData();
    void _sendTelemetry();
    void _saveToStorage();
    void _checkOperationalConditions();
    void _updateGroundNode(const MissionData& data);
    void _replaceLowestPriorityNode(const MissionData& newData);
    void _cleanupStaleNodes(unsigned long maxAge = NODE_TTL_MS);
    void _prepareForward();
    void _handleButtonEvents();  // ← NOVO
    
    void _displayStatus();
    void _displayTelemetry();
    void _displayError(const String& error);
    void _logHeapUsage(const String& component);
    void _monitorHeap();
};

#endif
