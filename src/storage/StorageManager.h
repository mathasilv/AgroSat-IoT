/**
 * @file StorageManager.h
 * @brief Gerenciador de armazenamento em SD Card
 * @version 1.2.0
 */

#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

class RTCManager;  // Forward declaration

class StorageManager {
public:
    StorageManager();
    
    bool begin();
    
    // ✅ NOVO: Injeção de dependência
    void setRTCManager(RTCManager* rtcManager);
    
    // Armazenamento
    bool saveTelemetry(const TelemetryData& data);
    bool saveMissionData(const MissionData& data);
    bool logError(const String& errorMsg);
      
    // Gerenciamento de arquivos
    bool createTelemetryFile();
    bool createMissionFile();
    void listFiles();
    
    // Status
    bool isAvailable();
    uint64_t getFreeSpace();
    uint64_t getUsedSpace();
    
    void flush();

private:
    bool _available;
    RTCManager* _rtcManager;  // ✅ Ponteiro para RTCManager
    
    // Métodos privados
    File _openFile(const char* path);
    bool _checkFileSize(const char* path);
    String _telemetryToCSV(const TelemetryData& data);
    String _missionToCSV(const MissionData& data);
};

#endif
