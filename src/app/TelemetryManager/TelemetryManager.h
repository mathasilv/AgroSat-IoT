/**
 * @file TelemetryManager.h
 * @brief Gerenciador Central com Beacon SAFE e Link Budget
 * @version 10.0.0 (5.4 Beacon SAFE + 4.2 Link Budget + 5.2 Adaptive SF)
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
#include "comm/LinkBudgetCalculator.h"  // NOVO 4.2

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
    
    // NOVO 4.2: Link Budget Calculator
    LinkBudgetCalculator _linkBudget;

    OperationMode  _mode;
    bool           _missionActive;
    TelemetryData  _telemetryData;

    // Timers
    unsigned long  _lastTelemetrySend;
    unsigned long  _lastStorageSave;
    unsigned long  _missionStartTime;
    unsigned long  _lastSensorReset;
    unsigned long  _lastBeaconTime;      // NOVO 5.4
    unsigned long  _lastLinkBudgetCalc;  // NOVO 4.2

    // Helpers de Inicialização
    void _initModeDefaults();
    void _initSubsystems(uint8_t& subsystemsOk, bool& success);
    void _syncNTPIfAvailable();
    void _logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap);

    // Helpers de Loop
    void _handleIncomingRadio();   
    void _maintainGroundNetwork(); 
    void _sendTelemetry();
    void _saveToStorage();
    void _checkOperationalConditions();
    void _handleButtonEvents();
    void _updateLEDIndicator(unsigned long currentTime);
    
    // NOVO 5.4: Beacon Automático em Modo SAFE
    void _sendSafeBeacon();
    
    // NOVO 4.2 + 5.2: Link Budget e Adaptive SF
    void _updateLinkBudget();
    void _applyAdaptiveSF();
};

#endif