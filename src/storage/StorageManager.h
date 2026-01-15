/**
 * @file StorageManager.h
 * @brief Gerenciador de armazenamento em SD Card com integridade de dados
 * 
 * @details Sistema de armazenamento robusto com suporte a:
 *          - Logging de telemetria em formato CSV
 *          - Verificação de integridade via CRC-16 CCITT
 *          - Rotação automática de arquivos por tamanho
 *          - Recuperação automática de falhas do SD
 *          - Integração com RTC para timestamps precisos
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 3.4.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Arquivos Gerados
 * | Arquivo          | Conteúdo                    | Formato |
 * |------------------|-----------------------------|--------|
 * | telemetry.csv    | Dados de sensores           | CSV+CRC |
 * | mission.csv      | Dados de ground nodes       | CSV+CRC |
 * | system.log       | Logs do sistema             | TXT+CRC |
 * 
 * ## Integridade de Dados
 * - **CRC-16 CCITT**: Cada linha tem checksum anexado
 * 
 * ## Pinagem SD Card (HSPI)
 * | Pino ESP32 | Função SD |
 * |------------|----------|
 * | GPIO 13    | CS       |
 * | GPIO 14    | CLK      |
 * | GPIO 15    | MOSI     |
 * | GPIO 2     | MISO     |
 * 
 * @note Usa HSPI para não conflitar com LoRa (VSPI)
 * @warning Tamanho máximo de arquivo: 5MB (rotação automática)
 */

#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

// Forward declarations
class RTCManager;
class SystemHealth;

/**
 * @class StorageManager
 * @brief Gerenciador de SD Card com CRC
 */
class StorageManager {
public:
    /**
     * @brief Construtor padrão
     */
    StorageManager();
    
    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o SD Card via HSPI
     * @return true se SD detectado e montado
     * @return false se falha na inicialização
     */
    bool begin();
    
    /**
     * @brief Define referência ao RTCManager para timestamps
     * @param rtcManager Ponteiro para instância do RTCManager
     */
    void setRTCManager(RTCManager* rtcManager);
    
    /**
     * @brief Define referência ao SystemHealth para reportar erros
     * @param systemHealth Ponteiro para instância do SystemHealth
     */
    void setSystemHealth(SystemHealth* systemHealth);
    
    //=========================================================================
    // LOGGING
    //=========================================================================
    
    /**
     * @brief Salva mensagem de log no arquivo de sistema
     * @param message Mensagem a ser logada
     * @return true se salvo com sucesso
     */
    bool saveLog(const String& message);
    
    /**
     * @brief Salva dados de telemetria em CSV
     * @param data Estrutura TelemetryData com dados dos sensores
     * @return true se salvo com sucesso
     */
    bool saveTelemetry(const TelemetryData& data);
    
    /**
     * @brief Salva dados de missão (ground nodes) em CSV
     * @param data Estrutura MissionData com dados do nó
     * @return true se salvo com sucesso
     */
    bool saveMissionData(const MissionData& data);
    
    //=========================================================================
    // GERENCIAMENTO DE ARQUIVOS
    //=========================================================================
    
    /** @brief Cria arquivo de telemetria com cabeçalho CSV */
    bool createTelemetryFile();
    
    /** @brief Cria arquivo de missão com cabeçalho CSV */
    bool createMissionFile();
    
    /** @brief Cria arquivo de log do sistema */
    bool createLogFile();
    
    //=========================================================================
    // STATUS
    //=========================================================================
    
    /** @brief SD Card disponível e montado? */
    bool isAvailable() const { return _available; }
    
private:
    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _available;             ///< SD Card montado?
    RTCManager* _rtcManager;     ///< Referência ao RTC para timestamps
    SystemHealth* _systemHealth; ///< Referência para reportar erros
    
    //=========================================================================
    // CONTROLE DE RECUPERAÇÃO
    //=========================================================================
    unsigned long _lastInitAttempt;  ///< Timestamp última tentativa de init
    static constexpr unsigned long REINIT_INTERVAL = 5000;  ///< Intervalo entre tentativas
    
    //=========================================================================
    // CONTADORES DE INTEGRIDADE
    //=========================================================================
    uint16_t _totalWrites;       ///< Total de escritas
    
    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /** @brief Tenta recuperar SD Card após falha */
    void _attemptRecovery();
    
    /** @brief Verifica se arquivo excedeu tamanho máximo */
    bool _checkFileSize(const char* path);

    /** @brief Formata TelemetryData para linha CSV */
    void _formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len);
    
    /** @brief Formata MissionData para linha CSV */
    void _formatMissionToCSV(const MissionData& data, char* buffer, size_t len);
    
    /**
     * @brief Calcula CRC-16 CCITT
     * @param data Ponteiro para dados
     * @param length Tamanho em bytes
     * @return CRC-16 calculado
     */
    uint16_t _calculateCRC16(const uint8_t* data, size_t length);
};

#endif
