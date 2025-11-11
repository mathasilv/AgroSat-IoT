/**
 * @file SensorManager.h
 * @brief Gerenciamento de sensores PION - VERSÃO OTIMIZADA
 * @version 4.0.0
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

#include <MPU9250_WE.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Si7021.h>
#include <Adafruit_CCS811.h>

extern TwoWire I2C_Sensors;

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    
    // Getters
    float getTemperature();
    float getTemperatureSI7021();
    float getPressure();
    float getAltitude();
    float getGyroX();
    float getGyroY();
    float getGyroZ();
    float getAccelX();
    float getAccelY();
    float getAccelZ();
    float getAccelMagnitude();
    float getMagX();
    float getMagY();
    float getMagZ();
    float getHumidity();
    float getCO2();
    float getTVOC();
    
    // Status
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSI7021Online();
    bool isCCS811Online();
    bool isCalibrated();
    
    // Utilidades
    void scanI2C();
    void printSensorStatus();
    bool calibrateIMU();
    void resetAll();
    
    // Compatibilidade
    void getRawData(float& gx, float& gy, float& gz, 
                   float& ax, float& ay, float& az);
    bool isMPU6050Online() { return false; }

private:
    // Instâncias dos sensores
    MPU9250_WE _mpu9250;
    Adafruit_BMP280 _bmp280;
    Adafruit_Si7021 _si7021;
    Adafruit_CCS811 _ccs811;

    // Dados dos sensores
    float _temperature, _pressure, _altitude, _temperatureSI;
    float _humidity, _co2Level, _tvoc;
    float _seaLevelPressure;
    
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;
    
    // Calibração magnetômetro
    float _magOffsetX, _magOffsetY, _magOffsetZ;
    
    // Status
    bool _mpu9250Online;
    bool _bmp280Online;
    bool _si7021Online;
    bool _ccs811Online;
    bool _calibrated;
    
    // Controle de timing
    uint32_t _lastReadTime;
    uint32_t _lastCCS811Read;
    uint32_t _lastSI7021Read;
    uint32_t _lastHealthCheck;
    uint8_t _consecutiveFailures;
    
    // Filtro de média móvel
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    float _sumAccelX, _sumAccelY, _sumAccelZ;
    
    // Métodos privados
    bool _initMPU9250();
    bool _initBMP280();
    bool _initSI7021();
    bool _initCCS811();
    
    bool _validateMPUReadings(float gx, float gy, float gz, 
                             float ax, float ay, float az,
                             float mx, float my, float mz);
    bool _validateBMPReadings(float temperature, float pressure);
    bool _validateSI7021Readings(float temperature, float humidity);
    bool _validateCCSReadings(float co2, float tvoc);
    
    void _performHealthCheck();
    bool _calibrateMPU9250();
    float _applyFilter(float newValue, float* buffer, float& sum);
    float _calculateAltitude(float pressure);
    
    void _updateIMU();
    void _updateBMP280();
    void _updateSI7021();
    void _updateCCS811();
};

#endif // SENSORMANAGER_H
