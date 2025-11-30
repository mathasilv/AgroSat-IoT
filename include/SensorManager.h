/**
 * @file SensorManager.h
 * @brief SensorManager V6.0.0 - Arquitetura Modular Completa
 * @version 6.0.0
 * @date 2025-11-30
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "MPU9250Manager.h"
#include "BMP280Manager.h"
#include "SI7021Manager.h"
#include "CCS811Manager.h"

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    void resetAll();
    
    // TEMPERATURA (Redundância automática)
    float getTemperature() const { return _temperature; }
    float getTemperatureSI7021() const { return _si7021Manager.getTemperature(); }
    float getTemperatureBMP280() const { return _bmp280Manager.getTemperature(); }
    
    // PRESSÃO/ALTITUDE
    float getPressure() const { return _bmp280Manager.getPressure(); }
    float getAltitude() const { return _bmp280Manager.getAltitude(); }
    
    // IMU (MPU9250Manager)
    float getGyroX() const { return _mpu9250Manager.getGyroX(); }
    float getGyroY() const { return _mpu9250Manager.getGyroY(); }
    float getGyroZ() const { return _mpu9250Manager.getGyroZ(); }
    float getAccelX() const { return _mpu9250Manager.getAccelX(); }
    float getAccelY() const { return _mpu9250Manager.getAccelY(); }
    float getAccelZ() const { return _mpu9250Manager.getAccelZ(); }
    float getAccelMagnitude() const { return _mpu9250Manager.getAccelMagnitude(); }
    float getMagX() const { return _mpu9250Manager.getMagX(); }
    float getMagY() const { return _mpu9250Manager.getMagY(); }
    float getMagZ() const { return _mpu9250Manager.getMagZ(); }
    
    // AMBIENTAL (CCS811Manager CORRIGIDO)
    float getHumidity() const { return _si7021Manager.getHumidity(); }
    float getCO2() const { return _ccs811Manager.geteCO2(); }      // ← CORRIGIDO
    float getTVOC() const { return _ccs811Manager.getTVOC(); }
    
    // STATUS
    bool isMPU9250Online() const { return _mpu9250Manager.isOnline(); }
    bool isBMP280Online() const { return _bmp280Manager.isOnline(); }
    bool isSI7021Online() const { return _si7021Manager.isOnline(); }
    bool isCCS811Online() const { return _ccs811Manager.isOnline(); }
    bool isCalibrated() const { return _mpu9250Manager.isCalibrated(); }
    
    // UTILITÁRIOS
    void getRawData(float& gx, float& gy, float& gz,
                    float& ax, float& ay, float& az,
                    float& mx, float& my, float& mz) const;
    void printSensorStatus() const;
    void scanI2C();
    
private:
    MPU9250Manager _mpu9250Manager;
    BMP280Manager _bmp280Manager;
    SI7021Manager _si7021Manager;
    CCS811Manager _ccs811Manager;
    
    mutable float _temperature;
    uint32_t _lastHealthCheck;
    uint8_t _consecutiveFailures;
    
    void _updateTemperatureRedundancy();
    void _performHealthCheck();
    
    static constexpr uint32_t HEALTH_CHECK_INTERVAL = 30000;
    static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 10;
};

#endif // SENSORMANAGER_H  ← FINAL CORRETO
