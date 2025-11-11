/**
 * @file TelemetryManager.h
 * @brief VERSÃO CORRIGIDA - I2C centralizado + RTC integrado
 * @version 2.2.2
 */
#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SSD1306Wire.h"
#include "SystemHealth.h"
#include "PowerManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "PayloadManager.h"
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
    
    // Aplicação do modo operacional
    void applyModeConfig(OperationMode mode);
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();

private:
    // Subsistemas
    SSD1306Wire _display;
    SystemHealth _health;
    PowerManager _power;
    SensorManager _sensors;
    StorageManager _storage;
    PayloadManager _payload;
    CommunicationManager _comm;
    RTCManager _rtc;
    
    // Dados
    TelemetryData _telemetryData;
    MissionData _missionData;
    
    // Estado
    OperationMode _mode;
    bool _missionActive;              // ← ADICIONAR
    uint32_t _missionStartTime;       // ← ADICIONAR
    
    // Timers
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    uint32_t _minHeapSeen;
    
    // Métodos privados
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
};

#endif // TELEMETRYMANAGER_H
