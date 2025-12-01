/**
 * @file TelemetryManager.h
 * @brief Gerenciador central de telemetria - HAB + UTC
 * @version 2.0.0
 * @date 2025-12-01
 */

#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include "config.h"

// Subsystems / services
#include "SensorManager.h"
#include "PowerManager.h"
#include "SystemHealth.h"
#include "DisplayManager.h"
#include "RTCManager.h"
#include "ButtonHandler.h"
#include "StorageManager.h"
#include "CommunicationManager.h"

// Higher-level controllers
#include "app/GroundNodeManager/GroundNodeManager.h"
#include "core/MissionController/MissionController.h"
#include "core/TelemetryCollector/TelemetryCollector.h"
#include "core/CommandHandler/CommandHandler.h"
#include "comm/LoRaService/LoRaService.h"

class TelemetryManager {
public:
    TelemetryManager();

    bool begin();
    void loop();

    // Comandos externos (via Serial/LoRa)
    bool handleCommand(const String& cmd);

    // Acesso aos subsistemas (se precisar expor)
    SensorManager& getSensorManager() { return _sensors; }

    // Controle de missão
    void startMission();
    void stopMission();

    OperationMode getMode();
    void applyModeConfig(uint8_t modeIndex);

    // Métodos de teste/debug
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();

private:
    // ========================================
    // SUBSISTEMAS / CONTROLADORES
    // ========================================
    SensorManager        _sensors;
    PowerManager         _power;
    SystemHealth         _systemHealth;
    RTCManager           _rtc;
    ButtonHandler        _button;
    StorageManager       _storage;
    CommunicationManager _comm;
    DisplayManager       _displayMgr;
    GroundNodeManager    _groundNodes;

    MissionController    _mission;
    TelemetryCollector   _telemetryCollector;
    CommandHandler       _commandHandler;

    // ========================================
    // ESTADO
    // ========================================
    OperationMode  _mode;
    bool           _missionActive;

    TelemetryData  _telemetryData;

    unsigned long  _lastTelemetrySend;
    unsigned long  _lastStorageSave;
    unsigned long  _missionStartTime;
    unsigned long  _lastSensorReset;

    bool           _useNewDisplay;

    // ========================================
    // MÉTODOS PRIVADOS (helpers)
    // ========================================
    bool _initI2CBus();

    // Helpers de inicialização
    void _initModeDefaults();
    void _initDisplay(bool& displayOk);
    void _initSubsystems(bool displayOk, uint8_t& subsystemsOk, bool& success);
    void _syncNTPIfAvailable(bool displayOk);
    void _logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap);

    // Operação
    void _sendTelemetry();
    void _saveToStorage();
    void _handleButtonEvents();
    void _checkOperationalConditions();
    void _updateLEDIndicator(unsigned long currentTime);
};

#endif // TELEMETRYMANAGER_H
