#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include <mutex>
#include "config.h"
#include <HAL/interface/I2C.h>      // <-- novo
#include "SensorManager.h"
#include "CommunicationManager.h"
#include "StorageManager.h"
#include "PowerManager.h"
#include "SystemHealth.h"
#include "DisplayManager.h"
#include "RTCManager.h"
#include "ButtonHandler.h"

class TelemetryManager {
public:
    // Novo: injeção do HAL I2C
    explicit TelemetryManager(HAL::I2C& i2c);
    
    bool begin();
    void loop();
    
    void startMission();
    void stopMission();
    
    OperationMode getMode();
    void applyModeConfig(OperationMode mode);
    
    void testLoRaTransmission();
    void sendCustomLoRa(const String& message);
    void printLoRaStats();

private:
    // Ponteiro para o barramento I2C (mesmo passado para SensorManager)
    HAL::I2C* _i2c;

    // Subsistemas
    SensorManager _sensors;
    CommunicationManager _comm;
    StorageManager _storage;
    PowerManager _power;
    SystemHealth _health;
    DisplayManager _displayMgr;
    RTCManager _rtc;
    ButtonHandler _button;
    
    // Estado do sistema
    OperationMode _mode;
    bool _missionActive;
    unsigned long _missionStartTime;
    
    // Telemetria
    TelemetryData _telemetryData;
    GroundNodeBuffer _groundNodeBuffer;
    std::mutex _bufferMutex;
    
    // Controle de tempo
    unsigned long _lastTelemetrySend;
    unsigned long _lastStorageSave;
    unsigned long _lastDisplayUpdate;
    unsigned long _lastHeapCheck;
    unsigned long _lastSensorReset;
    
    // Monitoramento
    uint32_t _minHeapSeen;
    bool _useNewDisplay;
    
    // Métodos internos
    bool _initI2CBus();
    
    void _collectTelemetryData();
    void _sendTelemetry();
    void _saveToStorage();
    void _checkOperationalConditions();
    
    void _updateGroundNode(const MissionData& data);
    void _replaceLowestPriorityNode(const MissionData& newData);
    void _cleanupStaleNodes(unsigned long maxAge);
    
    void _handleButtonEvents();
    
    void _logHeapUsage(const String& component);
    void _monitorHeap();
    
    void _updateLEDIndicator(unsigned long currentTime);
    void _monitorHeapUsage(unsigned long currentTime);
};

#endif // TELEMETRYMANAGER_H
