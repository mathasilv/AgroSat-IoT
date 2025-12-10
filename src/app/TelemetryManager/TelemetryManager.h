/**
 * @file TelemetryManager.h
 * @brief Gerenciador Central (FIX: Redundâncias Removidas)
 * @version 10.5.0
 */

#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include "config.h"

// Subsystems
#include "sensors/SensorManager/SensorManager.h"
#include "sensors/GPSManager/GPSManager.h"
#include "core/PowerManager/PowerManager.h"
#include "core/SystemHealth/SystemHealth.h"
#include "core/RTCManager/RTCManager.h"
#include "core/ButtonHandler/ButtonHandler.h"
#include "storage/StorageManager.h"
#include "comm/CommunicationManager/CommunicationManager.h"

// Controllers
#include "app/GroundNodeManager/GroundNodeManager.h"
#include "app/MissionController/MissionController.h"
#include "app/TelemetryCollector/TelemetryCollector.h"
#include "core/CommandHandler/CommandHandler.h"

class TelemetryManager {
public:
    TelemetryManager();

    bool begin();
    void loop();
    bool handleCommand(const String& cmd);

    // Wrapper do Watchdog (Clean Code)
    void feedWatchdog();

    // Controle de Missão
    void startMission();
    void stopMission();
    OperationMode getMode() { return _mode; }
    void applyModeConfig(uint8_t modeIndex);

    // Compatibilidade legado
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();

private:
    SensorManager        _sensors;
    GPSManager           _gps;
    PowerManager         _power;
    SystemHealth         _systemHealth;
    RTCManager           _rtc;
    ButtonHandler        _button;
    StorageManager       _storage;
    CommunicationManager _comm;
    GroundNodeManager    _groundNodes;

    MissionController    _mission;
    TelemetryCollector   _telemetryCollector;
    CommandHandler       _commandHandler;
    
    OperationMode  _mode;
    // REMOVIDO: bool _missionActive; (Redundante, usar _mission.isActive())
    
    TelemetryData  _telemetryData;

    // Timers
    unsigned long  _lastTelemetrySend;
    unsigned long  _lastStorageSave;
    unsigned long  _missionStartTime;
    unsigned long  _lastSensorReset;
    unsigned long  _lastBeaconTime;

    // Helpers
    void _initModeDefaults();
    void _initSubsystems(uint8_t& subsystemsOk, bool& success);
    void _syncNTPIfAvailable();
    void _logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap);

    void _handleIncomingRadio();   
    void _maintainGroundNetwork(); 
    void _sendTelemetry();
    void _saveToStorage();
    void _checkOperationalConditions();
    void _handleButtonEvents();
    void _updateLEDIndicator(unsigned long currentTime);
    
    void _sendSafeBeacon();
};

#endif