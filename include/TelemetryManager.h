/**
 * @file TelemetryManager.h
 * @brief Gerenciador central de telemetria - VERSÃO COMPATÍVEL
 * @version 2.0.0
 * @date 2025-12-01
 */

#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include "config.h"

// ========================================
// INCLUDES COMPLETOS
// ========================================
#include "SensorManager.h"
#include "PowerManager.h"
#include "SystemHealth.h"
#include "DisplayManager.h"
#include "RTCManager.h"
#include "ButtonHandler.h"
#include "StorageManager.h"
#include "CommunicationManager.h"

class TelemetryManager {
public:
    TelemetryManager();
    
    bool begin();
    void loop();
    
    // Comandos externos
    bool handleCommand(const String& cmd);
    
    // Acesso aos subsistemas
    SensorManager& getSensorManager() { return _sensors; }
    
    // Controle de missão
    void startMission();
    void stopMission();
    
    OperationMode getMode();
    void applyModeConfig(uint8_t modeIndex);
    
    // Métodos de teste/debug (se usados no .cpp)
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();
    
private:
    // ========================================
    // SUBSISTEMAS
    // ========================================
    SensorManager _sensors;
    PowerManager _power;           // ← SEM asterisco
    SystemHealth _systemHealth;     // ← SEM asterisco
    DisplayManager _display;
    RTCManager _rtc;                // ← SEM asterisco
    ButtonHandler _button;          // ← SEM asterisco
    StorageManager _storage;        // ← SEM asterisco
    CommunicationManager _comm;     // ← SEM asterisco
    
    // ========================================
    // ESTADO
    // ========================================
    OperationMode _mode;
    OperationMode _prevMode;
    bool _missionActive;
    
    // Telemetria
    TelemetryData _telemetryData;  // ← CORRIGIDO: usar TelemetryData em vez de TelemetryPacket
    GroundNodeBuffer _groundNodeBuffer;
    
    unsigned long _lastTelemetryTime;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _missionStartTime;
    unsigned long _lastSensorReset;
    
    uint32_t _minHeapSeen;
    uint8_t _consecutiveCommFailures;
    
    bool _useNewDisplay;
    DisplayManager _displayMgr;  // ← Se usado no .cpp
    
    // ========================================
    // MÉTODOS PRIVADOS
    // ========================================
    bool _initI2CBus();
    void _initSubsystems();
    void _updateStateMachine();
    void _collectTelemetryData();
    void _transmitTelemetry();
    void _sendTelemetry();  // ← ADICIONAR
    void _saveToStorage();  // ← ADICIONAR
    void _updateDisplay();
    void _handleButtonEvents();
    void _monitorSystemHealth();
    void _checkOperationalConditions();
    void _updateLEDIndicator(unsigned long currentTime);  // ← ADICIONAR
    void _monitorHeapUsage(unsigned long currentTime);    // ← ADICIONAR
    
    void _updateGroundNode(const MissionData& data);          // ← ADICIONAR
    void _replaceLowestPriorityNode(const MissionData& data); // ← ADICIONAR
    void _cleanupStaleNodes(unsigned long maxAge);            // ← ADICIONAR
    
    void _printHelpMenu();
};

#endif // TELEMETRYMANAGER_H
