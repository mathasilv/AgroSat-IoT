/**
 * @file StorageManager.h
 * @brief Gerenciador de Armazenamento com CRC16 e Redundância Tripla
 * @version 3.0.0 (MODERADO 4.6 + MELHORIA 5.8)
 */

#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

class RTCManager; 

// ============================================================================
// NOVO 5.8: Estrutura de Dados Redundante com CRC
// ============================================================================
struct RedundantData {
    uint32_t timestamp;
    float value;
    uint16_t crc;
} __attribute__((packed));

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
    
    // NOVO 5.8: Salvamento com Redundância Tripla
    bool saveTelemetryRedundant(const TelemetryData& data);
    bool saveMissionDataRedundant(const MissionData& data);
      
    // Gerenciamento de arquivos
    bool createTelemetryFile();
    bool createMissionFile();
    bool createLogFile();
    
    void listFiles();
    
    // Status
    bool isAvailable() const { return _available; }
    uint64_t getFreeSpace();
    uint64_t getUsedSpace();
    
    // NOVO 4.6: Estatísticas de integridade
    uint16_t getCRCErrors() const { return _crcErrors; }
    uint16_t getTotalWrites() const { return _totalWrites; }
    
private:
    bool _available;
    RTCManager* _rtcManager;
    
    // Controle de Recuperação
    unsigned long _lastInitAttempt;
    static constexpr unsigned long REINIT_INTERVAL = 5000;
    
    // NOVO 4.6: Contadores de integridade
    uint16_t _crcErrors;
    uint16_t _totalWrites;
    
    // Métodos internos
    void _attemptRecovery();
    bool _checkFileSize(const char* path);

    // Formatação otimizada
    void _formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len);
    void _formatMissionToCSV(const MissionData& data, char* buffer, size_t len);
    
    // NOVO 4.6: Cálculo de CRC-16 (CCITT)
    uint16_t _calculateCRC16(const uint8_t* data, size_t length);
    
    // NOVO 5.8: Escrita redundante
    bool _writeTripleRedundant(const char* path, const uint8_t* data, size_t len);
    bool _readWithRedundancy(const char* path, uint8_t* data, size_t len);
};

#endif