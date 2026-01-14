/**
 * @file TelemetryCollector.h
 * @brief Coletor centralizado de dados de telemetria
 * 
 * @details Responsável por agregar dados de todos os subsistemas
 *          em uma única estrutura TelemetryData:
 *          - Timestamps (RTC e missão)
 *          - Dados de bateria e sistema
 *          - Sensores ambientais (BMP280, SI7021, CCS811)
 *          - IMU (MPU9250)
 *          - GPS
 *          - Resumo de ground nodes
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Fluxo de Coleta
 * ```
 * collect()
 * ├── _collectTimestamp()         // RTC + missionTime
 * ├── _collectPowerAndSystem()    // Bateria + health
 * ├── _collectCoreSensors()       // BMP280 + IMU
 * ├── _collectGPS()               // Posição
 * ├── _collectAndValidateSI7021() // Umidade (com validação)
 * ├── _collectAndValidateCCS811() // CO2/TVOC (com warmup check)
 * ├── _collectAndValidateMagnetometer() // Mag (com calibração check)
 * └── _generateNodeSummary()      // Resumo dos ground nodes
 * ```
 * 
 * ## Validações Aplicadas
 * - Range check em todos os sensores
 * - Verificação de warmup do CCS811
 * - Verificação de calibração do magnetômetro
 * - Valores inválidos substituídos por NAN
 * 
 * @see TelemetryData para estrutura de saída
 * @see TelemetryManager para orquestração
 */

#ifndef TELEMETRY_COLLECTOR_H
#define TELEMETRY_COLLECTOR_H

#include <Arduino.h>
#include "config.h"

// Forward declarations
class SensorManager;
class GPSManager;
class PowerManager;
class SystemHealth;
class RTCManager;
class GroundNodeManager;
class MissionController;

/**
 * @class TelemetryCollector
 * @brief Agregador de dados de telemetria de todos os subsistemas
 */
class TelemetryCollector {
public:
    /**
     * @brief Construtor com injeção de todas as dependências
     * @param sensors Referência ao SensorManager
     * @param gps Referência ao GPSManager
     * @param power Referência ao PowerManager
     * @param health Referência ao SystemHealth
     * @param rtc Referência ao RTCManager
     * @param nodes Referência ao GroundNodeManager
     * @param mission Referência ao MissionController
     */
    TelemetryCollector(
        SensorManager& sensors,
        GPSManager& gps,
        PowerManager& power,
        SystemHealth& health,
        RTCManager& rtc,
        GroundNodeManager& nodes,
        MissionController& mission
    );
    
    /**
     * @brief Coleta dados de todos os subsistemas
     * @param[out] data Estrutura TelemetryData a ser preenchida
     * @note Thread-safe se chamado com mutex externo
     */
    void collect(TelemetryData& data);
    
private:
    //=========================================================================
    // DEPENDÊNCIAS
    //=========================================================================
    SensorManager& _sensors;       ///< Gerenciador de sensores
    GPSManager&    _gps;           ///< Gerenciador de GPS
    PowerManager&  _power;         ///< Gerenciador de energia
    SystemHealth&  _health;        ///< Monitor de saúde
    RTCManager&    _rtc;           ///< Gerenciador de RTC
    GroundNodeManager& _nodes;     ///< Gerenciador de ground nodes
    MissionController& _mission;   ///< Controlador de missão
    
    //=========================================================================
    // MÉTODOS DE COLETA
    //=========================================================================
    
    /** @brief Coleta timestamps (RTC + missão) */
    void _collectTimestamp(TelemetryData& data);
    
    /** @brief Coleta dados de bateria e saúde do sistema */
    void _collectPowerAndSystem(TelemetryData& data);
    
    /** @brief Coleta dados do BMP280 e IMU */
    void _collectCoreSensors(TelemetryData& data);
    
    /** @brief Coleta dados do GPS */
    void _collectGPS(TelemetryData& data);
    
    /** @brief Coleta e valida dados do SI7021 (umidade) */
    void _collectAndValidateSI7021(TelemetryData& data);
    
    /** @brief Coleta e valida dados do CCS811 (CO2/TVOC) */
    void _collectAndValidateCCS811(TelemetryData& data);
    
    /** @brief Coleta e valida dados do magnetômetro */
    void _collectAndValidateMagnetometer(TelemetryData& data);
    
    /** @brief Gera resumo dos ground nodes para payload */
    void _generateNodeSummary(TelemetryData& data);
};

#endif // TELEMETRY_COLLECTOR_H