/**
 * @file TelemetryCollector.h
 * @brief Coletor e validador de dados de telemetria
 */

#ifndef TELEMETRY_COLLECTOR_H
#define TELEMETRY_COLLECTOR_H

#include <Arduino.h>
#include "config.h"

// Forward Declarations
class SensorManager;
class GPSManager;
class PowerManager;
class SystemHealth;
class RTCManager;
class GroundNodeManager;

class TelemetryCollector {
public:
    TelemetryCollector(
        SensorManager& sensors,
        GPSManager& gps,
        PowerManager& power,
        SystemHealth& health,
        RTCManager& rtc,
        GroundNodeManager& nodes
    );
    
    void collect(TelemetryData& data);
    
private:
    SensorManager& _sensors;
    GPSManager&    _gps;
    PowerManager&  _power;
    SystemHealth&  _health;
    RTCManager&    _rtc;
    GroundNodeManager& _nodes;
    
    void _collectTimestamp(TelemetryData& data);
    void _collectPowerAndSystem(TelemetryData& data);
    void _collectCoreSensors(TelemetryData& data);
    void _collectGPS(TelemetryData& data);
    void _collectAndValidateSI7021(TelemetryData& data);
    void _collectAndValidateCCS811(TelemetryData& data);
    void _collectAndValidateMagnetometer(TelemetryData& data);
    void _generateNodeSummary(TelemetryData& data);
};

#endif