#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <MPU9250_WE.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h>
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
    bool isMPU6050Online();
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
    MPU9250_WE _mpu9250;
    Adafruit_BMP280 _bmp280;
    Adafruit_CCS811 _ccs811;
    
    // DADOS DOS SENSORES
    float _temperature, _temperatureBMP, _temperatureSI;
    float _pressure, _altitude;
    float _humidity, _co2Level, _tvoc;
    float _seaLevelPressure;
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;
    float _magOffsetX, _magOffsetY, _magOffsetZ;
    
    // STATUS DOS SENSORES
    bool _mpu9250Online, _bmp280Online, _si7021Online, _ccs811Online;
    bool _calibrated;
    bool _si7021TempValid, _bmp280TempValid;
    uint8_t _si7021TempFailures, _bmp280TempFailures;
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;
    
    // CONTROLE DE TEMPO
    unsigned long _lastReadTime, _lastCCS811Read, _lastSI7021Read;
    unsigned long _lastHealthCheck;
    uint8_t _consecutiveFailures;
    
    // FILTRO DE MÉDIA MÓVEL
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    float _sumAccelX, _sumAccelY, _sumAccelZ;
    
    // CORREÇÃO BMP280
    unsigned long _lastBMP280Reinit;
    uint8_t _bmp280FailCount;
    float _pressureHistory[5];
    float _altitudeHistory[5];
    float _tempHistory[5];
    uint8_t _historyIndex;
    bool _historyFull;
    unsigned long _lastUpdateTime;
    float _lastPressureRead;
    uint8_t _identicalReadings;
    unsigned long _warmupStartTime;
    
    // MÉTODOS INTERNOS - INICIALIZAÇÃO
    bool _initMPU9250();
    bool _initBMP280();
    bool _initSI7021();
    bool _initCCS811();
    
    // MÉTODOS INTERNOS - ATUALIZAÇÃO
    void _updateIMU();
    void _updateBMP280();
    void _updateSI7021();
    void _updateCCS811();
    void _updateTemperatureRedundancy();
    
    // MÉTODOS INTERNOS - BMP280
    bool _reinitBMP280();
    bool _validateBMP280Reading();
    bool _waitForBMP280Measurement();
    float _getMedian(float* values, uint8_t count);
    bool _isOutlier(float value, float* history, uint8_t count);
    
    // MÉTODOS INTERNOS - VALIDAÇÃO
    bool _validateReading(float value, float minValid, float maxValid);
    bool _validateMPUReadings(float gx, float gy, float gz,
                              float ax, float ay, float az,
                              float mx, float my, float mz);
    bool _validateCCSReadings(float co2, float tvoc);
    
    // OUTROS MÉTODOS INTERNOS
    void _performHealthCheck();
    bool _calibrateMPU9250();
    float _applyFilter(float newValue, float* buffer, float& sum);
    float _calculateAltitude(float pressure);
    bool _softResetBMP280();  // ✅ ADICIONAR

};

#endif // SENSORMANAGER_H
