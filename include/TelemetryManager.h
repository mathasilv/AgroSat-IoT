/**
 * @file TelemetryManager.h
 * @brief VERSÃO CORRIGIDA - I2C centralizado
 * @version 2.2.0
 */

#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SSD1306Wire.h"  // Display OLED
#include "SystemHealth.h"
#include "PowerManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "PayloadManager.h"
#include "CommunicationManager.h"

enum OperationMode {
    MODE_INIT = 0,
    MODE_PREFLIGHT = 1,
    MODE_FLIGHT = 2,
    MODE_POSTFLIGHT = 3,
    MODE_ERROR = 255
};

class TelemetryManager {
public:
    TelemetryManager();
    
    bool begin();
    void loop();
    
    void startMission();
    void stopMission();
    OperationMode getMode();
    
    void updateDisplay();

private:
    // Display OLED - SEM SDA/SCL no construtor
    SSD1306Wire _display;
    
    // Subsistemas
    SystemHealth _health;
    PowerManager _power;
    SensorManager _sensors;
    StorageManager _storage;
    PayloadManager _payload;
    CommunicationManager _comm;
    
    // Dados
    TelemetryData _telemetryData;
    MissionData _missionData;
    
    // Estado
    OperationMode _mode;
    
    // Timing
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    
    // Monitoramento
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
    
    // NOVO: Inicialização centralizada I2C
    bool _initI2CBus();
};

#endif
