/**
 * @file TelemetryCollector.h
 * @brief Coletor e validador de dados de telemetria
 * @version 1.0.0
 * @date 2025-12-01
 * 
 * Responsabilidades:
 * - Coletar dados de todos os sensores
 * - Validar ranges de valores
 * - Preencher struct TelemetryData
 * - Adicionar timestamp UTC
 */

#ifndef TELEMETRY_COLLECTOR_H
#define TELEMETRY_COLLECTOR_H

#include <Arduino.h>
#include "config.h"
#include "sensors/SensorManager/SensorManager.h"
#include "PowerManager.h"
#include "SystemHealth.h"
#include "RTCManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"

class TelemetryCollector {
public:
    /**
     * @brief Construtor
     * @param sensors Referência ao gerenciador de sensores
     * @param power Referência ao gerenciador de energia
     * @param health Referência ao monitor de saúde do sistema
     * @param rtc Referência ao RTC (UTC)
     * @param nodes Referência ao gerenciador de nós terrestres
     */
    TelemetryCollector(
        SensorManager& sensors,
        PowerManager& power,
        SystemHealth& health,
        RTCManager& rtc,
        GroundNodeManager& nodes
    );
    
    /**
     * @brief Coleta todos os dados e preenche a estrutura de telemetria
     * @param data Estrutura a ser preenchida com dados coletados
     */
    void collect(TelemetryData& data);
    
private:
    SensorManager& _sensors;
    PowerManager& _power;
    SystemHealth& _health;
    RTCManager& _rtc;
    GroundNodeManager& _nodes;
    
    /**
     * @brief Coleta timestamp UTC ou millis
     * @param data Estrutura de telemetria
     */
    void _collectTimestamp(TelemetryData& data);
    
    /**
     * @brief Coleta dados de energia e sistema
     * @param data Estrutura de telemetria
     */
    void _collectPowerAndSystem(TelemetryData& data);
    
    /**
     * @brief Coleta dados do BMP280 e MPU9250 (core sensors)
     * @param data Estrutura de telemetria
     */
    void _collectCoreSensors(TelemetryData& data);
    
    /**
     * @brief Coleta e valida dados do SI7021 (temp + umidade)
     * @param data Estrutura de telemetria
     */
    void _collectAndValidateSI7021(TelemetryData& data);
    
    /**
     * @brief Coleta e valida dados do CCS811 (CO2 + TVOC)
     * @param data Estrutura de telemetria
     */
    void _collectAndValidateCCS811(TelemetryData& data);
    
    /**
     * @brief Coleta e valida dados do magnetômetro MPU9250
     * @param data Estrutura de telemetria
     */
    void _collectAndValidateMagnetometer(TelemetryData& data);
    
    /**
     * @brief Gera resumo dos nós terrestres no payload
     * @param data Estrutura de telemetria
     */
    void _generateNodeSummary(TelemetryData& data);
};

#endif // TELEMETRY_COLLECTOR_H
