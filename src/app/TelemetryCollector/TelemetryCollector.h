/**
 * @file TelemetryCollector.h
 * @brief Coletor e validador de dados de telemetria
 * @version 1.1.0 (Com GPS)
 */

#ifndef TELEMETRY_COLLECTOR_H
#define TELEMETRY_COLLECTOR_H

#include <Arduino.h>
#include "config.h"
#include "sensors/SensorManager/SensorManager.h"
#include "sensors/GPSManager/GPSManager.h" // [NOVO] Include
#include "core/PowerManager/PowerManager.h"
#include "core/SystemHealth/SystemHealth.h"
#include "core/RTCManager/RTCManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"

class TelemetryCollector {
public:
    /**
     * @brief Construtor
     * @param sensors Referência ao gerenciador de sensores
     * @param gps Referência ao gerenciador de GPS [NOVO]
     * @param power Referência ao gerenciador de energia
     * @param health Referência ao monitor de saúde do sistema
     * @param rtc Referência ao RTC (UTC)
     * @param nodes Referência ao gerenciador de nós terrestres
     */
    TelemetryCollector(
        SensorManager& sensors,
        GPSManager& gps, // [NOVO] Parâmetro
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
    GPSManager&    _gps; // [NOVO] Membro
    PowerManager&  _power;
    SystemHealth&  _health;
    RTCManager&    _rtc;
    GroundNodeManager& _nodes;
    
    // Métodos de coleta internos
    void _collectTimestamp(TelemetryData& data);
    void _collectPowerAndSystem(TelemetryData& data);
    void _collectCoreSensors(TelemetryData& data);
    void _collectGPS(TelemetryData& data); // [NOVO] Coleta dados do GPS
    void _collectAndValidateSI7021(TelemetryData& data);
    void _collectAndValidateCCS811(TelemetryData& data);
    void _collectAndValidateMagnetometer(TelemetryData& data);
    void _generateNodeSummary(TelemetryData& data);
};

#endif // TELEMETRY_COLLECTOR_H