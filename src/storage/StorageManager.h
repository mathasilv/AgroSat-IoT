/**
 * @file StorageManager.h
 * @brief Gerenciador de Armazenamento SD (Com Hot-Swap e Recuperação)
 * @version 2.0.0
 */

#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

class RTCManager; // Forward declaration

class StorageManager {
public:
    StorageManager();
    
    // Inicialização
    bool begin();
    
    // Injeção de dependência
    void setRTCManager(RTCManager* rtcManager);
    
    // Armazenamento (Retorna true se salvou com sucesso)
    bool saveTelemetry(const TelemetryData& data);
    bool saveMissionData(const MissionData& data);
    bool logError(const String& errorMsg);
      
    // Gerenciamento de arquivos
    bool createTelemetryFile();
    bool createMissionFile();
    void listFiles();
    
    // Status
    bool isAvailable() const { return _available; }
    uint64_t getFreeSpace();
    uint64_t getUsedSpace();
    
private:
    bool _available;
    RTCManager* _rtcManager;
    
    // Controle de Recuperação (Hot-Swap)
    unsigned long _lastInitAttempt;
    static constexpr unsigned long REINIT_INTERVAL = 5000; // Tentar recuperar SD a cada 5s
    
    // Métodos internos
    void _attemptRecovery();
    bool _checkFileSize(const char* path);
    String _telemetryToCSV(const TelemetryData& data);
    String _missionToCSV(const MissionData& data);
    String _getTimestampStr();
};

#endif