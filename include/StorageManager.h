/**
 * @file StorageManager.h
 * @brief Gerenciamento de armazenamento em SD Card
 * @version 1.0.0
 * 
 * Responsável por:
 * - Inicialização do SD Card
 * - Gravação de logs de telemetria
 * - Gravação de dados da missão
 * - Registro de erros
 * - Rotação de arquivos
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

class StorageManager {
public:
    /**
     * @brief Construtor
     */
    StorageManager();
    
    /**
     * @brief Inicializa SD Card
     * @return true se inicializado com sucesso
     */
    bool begin();
    
    /**
     * @brief Salva dados de telemetria no SD
     */
    bool saveTelemetry(const TelemetryData& data);
    
    /**
     * @brief Salva dados da missão no SD
     */
    bool saveMissionData(const MissionData& data);
    
    /**
     * @brief Registra erro no log
     */
    bool logError(const String& errorMsg);
    
    /**
     * @brief Cria header CSV se arquivo não existe
     */
    bool createTelemetryFile();
    bool createMissionFile();
    
    /**
     * @brief Verifica se SD Card está disponível
     */
    bool isAvailable();
    
    /**
     * @brief Retorna espaço livre no SD (bytes)
     */
    uint64_t getFreeSpace();
    
    /**
     * @brief Retorna espaço usado no SD (bytes)
     */
    uint64_t getUsedSpace();
    
    /**
     * @brief Lista arquivos no SD
     */
    void listFiles();
    
    /**
     * @brief Flush dos dados (força escrita)
     */
    void flush();

private:
    bool _available;
    File _telemetryFile;
    File _missionFile;
    File _errorFile;
    
    /**
     * @brief Abre arquivo para escrita (append)
     */
    File _openFile(const char* path);
    
    /**
     * @brief Verifica tamanho do arquivo e rotaciona se necessário
     */
    bool _checkFileSize(const char* path);
    
    /**
     * @brief Converte TelemetryData para linha CSV
     */
    String _telemetryToCSV(const TelemetryData& data);
    
    /**
     * @brief Converte MissionData para linha CSV
     */
    String _missionToCSV(const MissionData& data);
};

#endif // STORAGE_MANAGER_H
