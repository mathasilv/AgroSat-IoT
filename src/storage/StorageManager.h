/**
 * @file StorageManager.h
 * @brief Gerenciador de Armazenamento SD (Com Log de Sistema)
 * @version 2.2.1 (Fix Macro Conflict)
 */

#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

class RTCManager; 

class StorageManager {
public:
    StorageManager();
    
    bool begin();
    void setRTCManager(RTCManager* rtcManager);
    
    // Salvar Log de Texto
    bool saveLog(const String& message);
    
    // Métodos de Telemetria
    bool saveTelemetry(const TelemetryData& data);
    bool saveMissionData(const MissionData& data);
    bool logError(const String& errorMsg);
      
    // Gerenciamento de arquivos
    bool createTelemetryFile();
    bool createMissionFile();
    bool createLogFile();
    
    void listFiles();
    
    // Status
    bool isAvailable() const { return _available; }
    uint64_t getFreeSpace();
    uint64_t getUsedSpace();
    
private:
    bool _available;
    RTCManager* _rtcManager;
    
    // Controle de Recuperação
    unsigned long _lastInitAttempt;
    static constexpr unsigned long REINIT_INTERVAL = 5000;
    
    // CORREÇÃO: Variável removida pois já existe #define SD_SYSTEM_LOG em config.h
    // const char* SD_SYSTEM_LOG = "/system.log"; 

    // Métodos internos
    void _attemptRecovery();
    bool _checkFileSize(const char* path);
    String _telemetryToCSV(const TelemetryData& data);
    String _missionToCSV(const MissionData& data);
    String _getTimestampStr();
};

#endif