/**
 * @file TelemetryCollector.cpp
 * @brief Implementação do coletor de telemetria
 */

#include "TelemetryCollector.h"

// Includes Reais
#include "sensors/SensorManager/SensorManager.h"
#include "sensors/GPSManager/GPSManager.h"
#include "core/PowerManager/PowerManager.h"
#include "core/SystemHealth/SystemHealth.h"
#include "core/RTCManager/RTCManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"
#include "app/MissionController/MissionController.h"

TelemetryCollector::TelemetryCollector(
    SensorManager& sensors,
    GPSManager& gps,
    PowerManager& power,
    SystemHealth& health,
    RTCManager& rtc,
    GroundNodeManager& nodes,
    MissionController& mission // <--- Injeção
) :
    _sensors(sensors),
    _gps(gps),
    _power(power),
    _health(health),
    _rtc(rtc),
    _nodes(nodes),
    _mission(mission)
{
}

void TelemetryCollector::collect(TelemetryData& data) {
    _collectTimestamp(data);
    _collectPowerAndSystem(data);
    _collectCoreSensors(data);
    _collectGPS(data);
    _collectAndValidateSI7021(data);
    _collectAndValidateCCS811(data);
    _collectAndValidateMagnetometer(data);
    _generateNodeSummary(data);
}

void TelemetryCollector::_collectTimestamp(TelemetryData& data) {
    if (_rtc.isInitialized()) {
        data.timestamp = _rtc.getUnixTime();
    } else {
        data.timestamp = millis() / 1000;
    }
}

void TelemetryCollector::_collectPowerAndSystem(TelemetryData& data) {
    // CORRIGIDO: Pega o tempo do MissionController, não do SystemHealth
    data.missionTime = _mission.getDuration();
    
    data.batteryVoltage = _power.getVoltage();
    data.batteryPercentage = _power.getPercentage();
    
    data.systemStatus = _health.getSystemStatus();
    data.errorCount = _health.getErrorCount();
}

void TelemetryCollector::_collectCoreSensors(TelemetryData& data) {
    data.temperature = _sensors.getTemperature();
    data.temperatureBMP = _sensors.getTemperatureBMP280();
    data.pressure = _sensors.getPressure();
    data.altitude = _sensors.getAltitude();
    
    data.gyroX = _sensors.getGyroX();
    data.gyroY = _sensors.getGyroY();
    data.gyroZ = _sensors.getGyroZ();
    data.accelX = _sensors.getAccelX();
    data.accelY = _sensors.getAccelY();
    data.accelZ = _sensors.getAccelZ();
    
    data.temperatureSI = NAN;
    data.humidity = NAN;
    data.co2 = NAN;
    data.tvoc = NAN;
    data.magX = NAN;
    data.magY = NAN;
    data.magZ = NAN;
}

void TelemetryCollector::_collectGPS(TelemetryData& data) {
    data.latitude = _gps.getLatitude();
    data.longitude = _gps.getLongitude();
    data.gpsAltitude = _gps.getAltitude();
    data.satellites = _gps.getSatellites();
    data.gpsFix = _gps.hasFix();
}

void TelemetryCollector::_collectAndValidateSI7021(TelemetryData& data) {
    if (!_sensors.isSI7021Online()) return;
    
    float tempSI = _sensors.getTemperatureSI7021();
    if (!isnan(tempSI) && tempSI >= TEMP_MIN_VALID && tempSI <= TEMP_MAX_VALID) {
        data.temperatureSI = tempSI;
    }
    
    float humidity = _sensors.getHumidity();
    if (!isnan(humidity) && humidity >= HUMIDITY_MIN_VALID && humidity <= HUMIDITY_MAX_VALID) {
        data.humidity = humidity;
    }
}

void TelemetryCollector::_collectAndValidateCCS811(TelemetryData& data) {
    if (!_sensors.isCCS811Online()) return;
    
    float co2 = _sensors.getCO2();
    if (!isnan(co2) && co2 >= CO2_MIN_VALID && co2 <= CO2_MAX_VALID) {
        data.co2 = co2;
    }
    
    float tvoc = _sensors.getTVOC();
    if (!isnan(tvoc) && tvoc >= TVOC_MIN_VALID && tvoc <= TVOC_MAX_VALID) {
        data.tvoc = tvoc;
    }
}

void TelemetryCollector::_collectAndValidateMagnetometer(TelemetryData& data) {
    if (!_sensors.isMPU9250Online()) return;
    
    float magX = _sensors.getMagX();
    float magY = _sensors.getMagY();
    float magZ = _sensors.getMagZ();
    
    if (isnan(magX) || isnan(magY) || isnan(magZ)) return;
    
    if (magX >= MAG_MIN_VALID && magX <= MAG_MAX_VALID &&
        magY >= MAG_MIN_VALID && magY <= MAG_MAX_VALID &&
        magZ >= MAG_MIN_VALID && magZ <= MAG_MAX_VALID) {
        data.magX = magX;
        data.magY = magY;
        data.magZ = magZ;
    }
}

void TelemetryCollector::_generateNodeSummary(TelemetryData& data) {
    const GroundNodeBuffer& buf = _nodes.buffer();
    
    if (buf.activeNodes > 0) {
        snprintf(data.payload, PAYLOAD_MAX_SIZE,
                "Nodes:%d", buf.activeNodes);
    } else {
        data.payload[0] = '\0';
    }
}