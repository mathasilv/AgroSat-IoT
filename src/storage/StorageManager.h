/**
 * @file StorageManager.h
 * @brief Gerenciador de Armazenamento SD (Otimizado para Zero Fragmentação)
 * @version 2.3.0
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
    
    // Métodos internos
    void _attemptRecovery();
    bool _checkFileSize(const char* path);

    // --- MÉTODOS OTIMIZADOS (Zero Allocation) ---
    // Formatam diretamente no buffer para evitar criação de Strings temporárias
    
    /**
     * @brief Formata dados de telemetria diretamente no buffer fornecido
     * @param data Estrutura de dados
     * @param buffer Ponteiro para o buffer de destino
     * @param len Tamanho máximo do buffer
     */
    void _formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len);

    /**
     * @brief Formata dados da missão diretamente no buffer fornecido
     * @param data Estrutura de dados
     * @param buffer Ponteiro para o buffer de destino
     * @param len Tamanho máximo do buffer
     */
    void _formatMissionToCSV(const MissionData& data, char* buffer, size_t len);
};

#endif