/**
 * @file StorageManager.h
 * @brief Gerenciador de armazenamento SD - COM SUPORTE RTC
 * @version 1.1.0
 * @date 2025-11-10
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include <SD.h>
#include <SPI.h>

class StorageManager {
public:
    StorageManager();
    
    bool begin();
    
    // Salvamento de dados
    bool saveTelemetry(const TelemetryData& data);
    bool saveMissionData(const MissionData& data);
    bool logError(const String& errorMsg);
    
    // Criação de arquivos
    bool createTelemetryFile();
    bool createMissionFile();
    
    // Status
    bool isAvailable();
    uint64_t getFreeSpace();
    uint64_t getUsedSpace();
    void listFiles();
    void flush();
    
    void setRTCManager(class RTCManager* rtc) { _rtcManager = rtc; }

private:
    bool _available;
    class RTCManager* _rtcManager;
    
    File _openFile(const char* path);
    bool _checkFileSize(const char* path);
    String _telemetryToCSV(const TelemetryData& data);
    String _missionToCSV(const MissionData& data);
};

#endif // STORAGE_MANAGER_H
