/**
 * @file SensorManager.h
 * @brief Gerenciador centralizado de sensores
 * @version 2.0.0
 * @date 2025-12-01
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include "MPU9250Manager.h"
#include "BMP280Manager.h"
#include "SI7021Manager.h"
#include "CCS811Manager.h"
#include "config.h"

class SensorManager {
public:
    SensorManager();
    
    // Inicialização e controle
    bool begin();
    void update();
    void reset();
    void resetAll();  // ← ADICIONAR
    
    // ========================================
    // Comandos de calibração
    // ========================================
    bool recalibrateMagnetometer();
    void clearMagnetometerCalibration();
    void printMagnetometerCalibration() const;
    void getMagnetometerOffsets(float& x, float& y, float& z) const;
    
    bool applyCCS811EnvironmentalCompensation(float temperature, float humidity);
    bool saveCCS811Baseline();
    bool restoreCCS811Baseline();
    
    // ========================================
    // Status e diagnóstico
    // ========================================
    uint8_t getSensorCount() const { return _sensorCount; }
    uint8_t getOnlineSensors() const;
    
    bool isMPU9250Online() const { return _mpu9250.isOnline(); }
    bool isBMP280Online() const { return _bmp280.isOnline(); }
    bool isSI7021Online() const { return _si7021.isOnline(); }
    bool isCCS811Online() const { return _ccs811.isOnline(); }


    bool isMagnetometerCalibrated() const { return _mpu9250.isCalibrated(); }
    bool isCCS811WarmupComplete() const { return _ccs811.isWarmupComplete(); }
    bool isCCS811DataReliable() const { return _ccs811.isDataReliable(); }
    
    void printStatus() const;
    void printDetailedStatus() const;
    void printSensorStatus() const;  // ← ADICIONAR
    void scanI2C();  // ← ADICIONAR
    
    // ========================================
    // Getters de dados
    // ========================================
    
    // MPU9250 (9-DOF)
    float getAccelX() const { return _mpu9250.getAccelX(); }
    float getAccelY() const { return _mpu9250.getAccelY(); }
    float getAccelZ() const { return _mpu9250.getAccelZ(); }
    float getGyroX() const { return _mpu9250.getGyroX(); }
    float getGyroY() const { return _mpu9250.getGyroY(); }
    float getGyroZ() const { return _mpu9250.getGyroZ(); }
    float getMagX() const { return _mpu9250.getMagX(); }
    float getMagY() const { return _mpu9250.getMagY(); }
    float getMagZ() const { return _mpu9250.getMagZ(); }
    
    // BMP280
    float getTemperature() const { return _bmp280.getTemperature(); }
    float getTemperatureBMP280() const { return _bmp280.getTemperature(); }  // ← ADICIONAR (compatibilidade)
    float getPressure() const { return _bmp280.getPressure(); }
    float getAltitude() const { return _bmp280.getAltitude(); }
    
    // SI7021
    float getHumidity() const { return _si7021.getHumidity(); }
    float getTempSI7021() const { return _si7021.getTemperature(); }
    float getTemperatureSI7021() const { return _si7021.getTemperature(); }  // ← ADICIONAR (compatibilidade)
    
    // CCS811
    uint16_t geteCO2() const { return _ccs811.geteCO2(); }
    uint16_t getCO2() const { return _ccs811.geteCO2(); }  // ← ADICIONAR (compatibilidade)
    uint16_t getTVOC() const { return _ccs811.getTVOC(); }
    
    // RAW DATA (MPU9250)
    void getRawData(float& gx, float& gy, float& gz, 
                    float& ax, float& ay, float& az,
                    float& mx, float& my, float& mz) const;  // ← ADICIONAR
    
    // Acesso direto aos managers (se necessário)
    MPU9250Manager& getMPU9250() { return _mpu9250; }
    BMP280Manager& getBMP280() { return _bmp280; }
    SI7021Manager& getSI7021() { return _si7021; }
    CCS811Manager& getCCS811() { return _ccs811; }
    
private:
    MPU9250Manager _mpu9250;
    BMP280Manager _bmp280;
    SI7021Manager _si7021;
    CCS811Manager _ccs811;
    
    uint8_t _sensorCount;
    unsigned long _lastEnvCompensation;
    unsigned long _lastHealthCheck;  // ← ADICIONAR
    uint8_t _consecutiveFailures;    // ← ADICIONAR

    float _temperature; 

    static constexpr unsigned long ENV_COMPENSATION_INTERVAL = 60000;  // 60s
    static constexpr unsigned long HEALTH_CHECK_INTERVAL = 30000;      // 30s ← ADICIONAR
    static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 10;  // ← ADICIONAR ESTA LINHA

    // Métodos privados
    void _autoApplyEnvironmentalCompensation();
    void _updateTemperatureRedundancy();  // ← ADICIONAR
    void _performHealthCheck();           // ← ADICIONAR
};

#endif // SENSORMANAGER_H
