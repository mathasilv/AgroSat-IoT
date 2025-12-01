/**
 * @file TelemetryCollector.cpp
 * @brief Implementação do coletor de telemetria
 * @version 1.0.0
 * @date 2025-12-01
 */

#include "TelemetryCollector.h"

TelemetryCollector::TelemetryCollector(
    SensorManager& sensors,
    PowerManager& power,
    SystemHealth& health,
    RTCManager& rtc,
    GroundNodeManager& nodes
) :
    _sensors(sensors),
    _power(power),
    _health(health),
    _rtc(rtc),
    _nodes(nodes)
{
}

void TelemetryCollector::collect(TelemetryData& data) {
    _collectTimestamp(data);
    _collectPowerAndSystem(data);
    _collectCoreSensors(data);
    _collectAndValidateSI7021(data);
    _collectAndValidateCCS811(data);
    _collectAndValidateMagnetometer(data);
    _generateNodeSummary(data);
}

void TelemetryCollector::_collectTimestamp(TelemetryData& data) {
    if (_rtc.isInitialized()) {
        data.timestamp = _rtc.getUnixTime();  // UTC puro
    } else {
        data.timestamp = millis() / 1000;
    }
}

void TelemetryCollector::_collectPowerAndSystem(TelemetryData& data) {
    data.missionTime = _health.getMissionTime();
    data.batteryVoltage = _power.getVoltage();
    data.batteryPercentage = _power.getPercentage();
    
    data.systemStatus = _health.getSystemStatus();
    data.errorCount = _health.getErrorCount();
}

void TelemetryCollector::_collectCoreSensors(TelemetryData& data) {
    // Temperatura e pressão (BMP280)
    data.temperature = _sensors.getTemperature();
    data.temperatureBMP = _sensors.getTemperatureBMP280();
    data.pressure = _sensors.getPressure();
    data.altitude = _sensors.getAltitude();
    
    // IMU (MPU9250)
    data.gyroX = _sensors.getGyroX();
    data.gyroY = _sensors.getGyroY();
    data.gyroZ = _sensors.getGyroZ();
    data.accelX = _sensors.getAccelX();
    data.accelY = _sensors.getAccelY();
    data.accelZ = _sensors.getAccelZ();
    
    // Inicializar campos opcionais como NAN
    data.temperatureSI = NAN;
    data.humidity = NAN;
    data.co2 = NAN;
    data.tvoc = NAN;
    data.magX = NAN;
    data.magY = NAN;
    data.magZ = NAN;
}

void TelemetryCollector::_collectAndValidateSI7021(TelemetryData& data) {
    if (!_sensors.isSI7021Online()) {
        return;
    }
    
    // Temperatura SI7021
    float tempSI = _sensors.getTemperatureSI7021();
    if (!isnan(tempSI) && tempSI >= TEMP_MIN_VALID && tempSI <= TEMP_MAX_VALID) {
        data.temperatureSI = tempSI;
    }
    
    // Umidade
    float humidity = _sensors.getHumidity();
    if (!isnan(humidity) && humidity >= HUMIDITY_MIN_VALID && humidity <= HUMIDITY_MAX_VALID) {
        data.humidity = humidity;
    }
}

void TelemetryCollector::_collectAndValidateCCS811(TelemetryData& data) {
    if (!_sensors.isCCS811Online()) {
        return;
    }
    
    // CO2
    float co2 = _sensors.getCO2();
    if (!isnan(co2) && co2 >= CO2_MIN_VALID && co2 <= CO2_MAX_VALID) {
        data.co2 = co2;
    }
    
    // TVOC
    float tvoc = _sensors.getTVOC();
    if (!isnan(tvoc) && tvoc >= TVOC_MIN_VALID && tvoc <= TVOC_MAX_VALID) {
        data.tvoc = tvoc;
    }
}

void TelemetryCollector::_collectAndValidateMagnetometer(TelemetryData& data) {
    if (!_sensors.isMPU9250Online()) {
        return;
    }
    
    float magX = _sensors.getMagX();
    float magY = _sensors.getMagY();
    float magZ = _sensors.getMagZ();
    
    // Validar que todos os eixos são válidos
    if (isnan(magX) || isnan(magY) || isnan(magZ)) {
        return;
    }
    
    // Validar ranges
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
