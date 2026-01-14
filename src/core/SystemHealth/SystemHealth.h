/**
 * @file SystemHealth.h
 * @brief Monitor de saúde do sistema em tempo real
 * 
 * @details Sistema de monitoramento que rastreia:
 *          - Uso de memória heap (com níveis de alerta)
 *          - Temperatura interna do ESP32
 *          - Contagem e razão de resets
 *          - Erros de CRC e I2C
 *          - Status do watchdog
 *          - Uptime do sistema
 *          - Persistência de dados críticos na NVS
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Níveis de Heap
 * | Status   | Heap Livre  | Ação                    |
 * |----------|-------------|-------------------------|
 * | OK       | > 50 KB     | Operação normal         |
 * | LOW      | 30-50 KB    | Alerta, reduzir buffers |
 * | CRITICAL | 15-30 KB    | Entrar em SAFE_MODE     |
 * | FATAL    | < 15 KB     | Reiniciar sistema       |
 * 
 * ## Razões de Reset (ESP32)
 * | Código | Razão                    |
 * |--------|-------------------------|
 * | 1      | Power-on reset          |
 * | 3      | Software reset          |
 * | 4      | Watchdog reset (legacy) |
 * | 5      | Deep sleep reset        |
 * | 6      | Brownout reset          |
 * | 7      | Task watchdog reset     |
 * 
 * @note Dados persistidos na NVS: resetCount, minFreeHeap
 * @see MissionController para controle de estados de missão
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "config.h"

/**
 * @struct HealthTelemetryExtended
 * @brief Estrutura estendida com dados de saúde do sistema
 */
struct HealthTelemetryExtended {
    uint32_t uptime;
    uint16_t resetCount;
    uint8_t resetReason;
    uint32_t minFreeHeap;
    uint32_t currentFreeHeap;
    float cpuTemp;
    uint8_t sdCardStatus;
    uint16_t crcErrors;
    uint16_t i2cErrors;
    uint16_t watchdogResets;
    uint8_t currentMode;
    float batteryVoltage;
};

/**
 * @class SystemHealth
 * @brief Monitor de saúde do sistema com watchdog e persistência
 */
class SystemHealth {
public:
    /**
     * @brief Construtor padrão
     */
    SystemHealth();

    /**
     * @enum HeapStatus
     * @brief Níveis de status da memória heap
     */
    enum class HeapStatus { 
        HEAP_OK,       ///< > 50KB livre
        HEAP_LOW,      ///< 30-50KB livre
        HEAP_CRITICAL, ///< 15-30KB livre
        HEAP_FATAL     ///< < 15KB livre
    };

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o monitor de saúde
     * @return true se inicializado com sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza verificações de saúde
     * @note Chamar periodicamente (a cada 1-10 segundos)
     */
    void update();
    
    /**
     * @brief Alimenta o watchdog timer
     * @warning Deve ser chamado antes do timeout configurado
     */
    void feedWatchdog();

    /**
     * @brief Reconfigura timeout do watchdog dinamicamente
     * @param seconds Novo timeout em segundos
     */
    void setWatchdogTimeout(uint32_t seconds);

    /**
     * @brief Registra erro no sistema
     * @param errorCode Código do erro (ver SystemStatusErrors)
     * @param description Descrição textual do erro
     */
    void reportError(uint8_t errorCode, const String& description);
    
    //=========================================================================
    // GETTERS DE MÉTRICAS
    //=========================================================================
    
    /** @brief Retorna temperatura interna do ESP32 em °C */
    float getCPUTemperature();
    
    /** @brief Retorna heap livre atual em bytes */
    uint32_t getFreeHeap();
    
    /** @brief Retorna uptime em milissegundos */
    unsigned long getUptime();
    
    /** @brief Retorna contagem de erros */
    uint16_t getErrorCount() const { return _errorCount; }
    
    /** @brief Retorna flags de status do sistema */
    uint8_t getSystemStatus() const { return _systemStatus; }
    
    /** @brief Retorna status atual do heap */
    HeapStatus getHeapStatus() const { return _heapStatus; }

    /**
     * @brief Retorna estrutura completa de telemetria de saúde
     * @return HealthTelemetryExtended com todos os dados
     */
    HealthTelemetryExtended getHealthTelemetry();
    
    //=========================================================================
    // CONTADORES DE ERRO
    //=========================================================================
    
    /** @brief Incrementa contador de erros de CRC */
    void incrementCRCError() { _crcErrors++; }
    
    /** @brief Incrementa contador de erros I2C */
    void incrementI2CError() { _i2cErrors++; }
    
    /**
     * @brief Define ou limpa flag de erro do sistema
     * @param errorFlag Flag de erro (ver SystemStatusErrors)
     * @param active true para ativar, false para limpar
     */
    void setSystemError(uint8_t errorFlag, bool active);

    //=========================================================================
    // SETTERS DE STATUS EXTERNO
    //=========================================================================
    
    /** @brief Define status do SD Card */
    void setSDCardStatus(bool ok) { 
        _sdCardStatus = ok ? 0 : 1; 
        setSystemError(STATUS_SD_ERROR, !ok);
    }

    /** @brief Define modo de operação atual */
    void setCurrentMode(uint8_t mode) { _currentMode = mode; }
    
    /** @brief Define tensão da bateria para telemetria */
    void setBatteryVoltage(float voltage) { _batteryVoltage = voltage; }

private:
    //=========================================================================
    // ESTADO GERAL
    //=========================================================================
    bool _healthy;               ///< Sistema saudável?
    uint8_t _systemStatus;       ///< Flags de status (bitmask)
    uint16_t _errorCount;        ///< Contador total de erros
    
    //=========================================================================
    // MONITORAMENTO DE HEAP
    //=========================================================================
    uint32_t _minFreeHeap;       ///< Menor heap livre registrado
    HeapStatus _heapStatus;      ///< Status atual do heap
    uint32_t _lastHeapCheck;     ///< Timestamp última verificação

    //=========================================================================
    // TIMING
    //=========================================================================
    unsigned long _bootTime;     ///< Timestamp de boot
    uint32_t _lastWatchdogFeed;  ///< Última alimentação do WDT
    uint32_t _lastHealthCheck;   ///< Última verificação de saúde
    uint32_t _currentWdtTimeout; ///< Timeout atual do WDT (s)
    
    //=========================================================================
    // CONTADORES PERSISTENTES
    //=========================================================================
    uint16_t _resetCount;        ///< Contagem de resets
    uint8_t _resetReason;        ///< Razão do último reset
    uint16_t _crcErrors;         ///< Erros de CRC
    uint16_t _i2cErrors;         ///< Erros de I2C
    uint16_t _watchdogResets;    ///< Resets por watchdog
    
    //=========================================================================
    // STATUS EXTERNO
    //=========================================================================
    uint8_t _sdCardStatus;       ///< Status do SD Card
    uint8_t _currentMode;        ///< Modo de operação atual
    float _batteryVoltage;       ///< Tensão da bateria
    
    Preferences _prefs;          ///< Handle para NVS

    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    void _checkResources();      ///< Verifica recursos do sistema
    float _readInternalTemp();   ///< Lê temperatura interna
    void _loadPersistentData();  ///< Carrega dados da NVS
    void _savePersistentData();  ///< Salva dados na NVS
    void _incrementResetCount(); ///< Incrementa contador de resets
};

#endif