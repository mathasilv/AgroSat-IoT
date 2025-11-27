#pragma once

/**
 * @file SensorManager.h
 * @brief SensorManager com HAL I2C (SI7021 migrado)
 * @version 4.1.0
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <MPU9250_WE.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h>
#include "hal/hal.h"
#include "config.h"

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    void resetAll();
    
    // Getters de temperatura
    float getTemperature();
    float getTemperatureSI7021();
    float getTemperatureBMP280();
    
    // Getters de sensores
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
    
    // Status dos sensores
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSI7021Online();
    bool isCCS811Online();
    bool isCalibrated();
    
    // Utilitários
    void getRawData(float& gx, float& gy, float& gz,
                    float& ax, float& ay, float& az);
    void printSensorStatus();
    bool calibrateIMU();
    void scanI2C();
    void forceReinitBMP280();

private:
    // OBJETOS DE HARDWARE
    MPU9250_WE _mpu9250{MPU9250_ADDRESS};
    Adafruit_BMP280 _bmp280;
    Adafruit_CCS811 _ccs811;
    
    // DADOS DOS SENSORES
    float _temperature, _temperatureBMP, _temperatureSI;
    float _pressure, _altitude;
    float _humidity, _co2Level, _tvoc;
    float _seaLevelPressure{1013.25};
    float _gyroX{0.0}, _gyroY{0.0}, _gyroZ{0.0};
    float _accelX{0.0}, _accelY{0.0}, _accelZ{0.0};
    float _magX{0.0}, _magY{0.0}, _magZ{0.0};
    float _magOffsetX{0.0}, _magOffsetY{0.0}, _magOffsetZ{0.0};
    
    // STATUS DOS SENSORES
    bool _mpu9250Online{false}, _bmp280Online{false};
    bool _si7021Online{false}, _ccs811Online{false};
    bool _calibrated{false};
    bool _si7021TempValid{false}, _bmp280TempValid{false};
    uint8_t _si7021TempFailures{0}, _bmp280TempFailures{0};
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;
    
    // CONTROLE DE TEMPO
    unsigned long _lastReadTime{0}, _lastCCS811Read{0}, _lastSI7021Read{0};
    unsigned long _lastHealthCheck{0};
    uint8_t _consecutiveFailures{0};
    
    // FILTRO DE MÉDIA MÓVEL
    float _accelXBuffer[CUSTOM_FILTER_SIZE]{0};
    float _accelYBuffer[CUSTOM_FILTER_SIZE]{0};
    float _accelZBuffer[CUSTOM_FILTER_SIZE]{0};
    uint8_t _filterIndex{0};
    float _sumAccelX{0}, _sumAccelY{0}, _sumAccelZ{0};
    
    // CORREÇÃO BMP280
    unsigned long _lastBMP280Reinit{0};
    uint8_t _bmp280FailCount{0};
    float _pressureHistory[5]{1013.25};
    float _altitudeHistory[5]{0.0};
    float _tempHistory[5]{20.0};
    uint8_t _historyIndex{0};
    bool _historyFull{false};
    unsigned long _lastUpdateTime{0};
    float _lastPressureRead{0.0};
    uint8_t _identicalReadings{0};
    unsigned long _warmupStartTime{0};
    
    // MÉTODOS INTERNOS
    bool _initMPU9250();
    bool _initBMP280();
    bool _initSI7021();
    bool _initCCS811();
    
    void _updateIMU();
    void _updateBMP280();
    void _updateSI7021();
    void _updateCCS811();
    void _updateTemperatureRedundancy();
    
    bool _reinitBMP280();
    bool _validateBMP280Reading();
    bool _waitForBMP280Measurement();
    float _getMedian(float* values, uint8_t count);
    bool _isOutlier(float value, float* history, uint8_t count);
    
    bool _validateReading(float value, float minValid, float maxValid);
    bool _validateMPUReadings(float gx, float gy, float gz,
                              float ax, float ay, float az,
                              float mx, float my, float mz);
    bool _validateCCSReadings(float co2, float tvoc);
    
    void _performHealthCheck();
    bool _calibrateMPU9250();
    float _applyFilter(float newValue, float* buffer, float& sum);
    float _calculateAltitude(float pressure);
    bool _softResetBMP280();
};

#endif