/**
 * @file SensorManager.h
 * @brief Gerenciamento de sensores com auto-detecção - VERSÃO CORRIGIDA MPU6050_light
 * @version 2.2.0
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// === BIBLIOTECAS DE SENSORES ===
// MPU6050 - BIBLIOTECA TROCADA
#include <MPU6050_light.h>

// BMP280
#include <Adafruit_BMP280.h>

// Opcionais (descomentar se necessário)
#ifdef USE_MPU9250
    #include <MPU9250_WE.h>
#endif

#ifdef USE_SHT20
    #include <SHT2x.h>
#endif

#ifdef USE_CCS811
    #include <Adafruit_CCS811.h>
#endif

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    
    // Getters principais
    float getTemperature();
    float getPressure();
    float getAltitude();
    float getGyroX();
    float getGyroY();
    float getGyroZ();
    float getAccelX();
    float getAccelY();
    float getAccelZ();
    float getAccelMagnitude();
    
    // Getters expandidos
    float getHumidity();
    float getCO2();
    float getTVOC();
    float getMagX();
    float getMagY();
    float getMagZ();
    
    // Status
    bool isMPU6050Online();
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSHT20Online();
    bool isCCS811Online();
    bool isCalibrated();
    
    // Utilidades
    void scanI2C();
    void printSensorStatus();
    bool calibrateIMU();
    void resetMPU6050();
    void resetBMP280();
    void resetAll();
    
    // Compatibilidade
    void getRawData(float& gx, float& gy, float& gz, 
                   float& ax, float& ay, float& az);

private:
    // Instâncias dos sensores
    MPU6050 _mpu6050;  // BIBLIOTECA NOVA - sem argumentos no construtor
    Adafruit_BMP280 _bmp280;
    
#ifdef USE_MPU9250
    MPU9250_WE _mpu9250;
#endif

#ifdef USE_SHT20
    SHT2x _sht20;
#endif

#ifdef USE_CCS811
    Adafruit_CCS811 _ccs811;
#endif
    
    // Dados dos sensores
    float _temperature, _pressure, _altitude;
    float _humidity, _co2Level, _tvoc;
    float _seaLevelPressure;
    
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;
    
    // Offsets de calibração (não usado com MPU6050_light)
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    float _accelOffsetX, _accelOffsetY, _accelOffsetZ;
    
    // Status
    bool _mpu6050Online, _mpu9250Online;
    bool _bmp280Online, _sht20Online, _ccs811Online;
    bool _calibrated;
    
    // Controle de timing
    uint32_t _lastReadTime;
    uint32_t _lastCCS811Read;
    uint32_t _lastSHT20Read;
    uint32_t _lastHealthCheck;
    uint8_t _consecutiveFailures;
    
    // Filtro de média móvel
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    
    // Métodos privados - Inicialização
    bool _initMPU6050();
    bool _initBMP280();
#ifdef USE_MPU9250
    bool _initMPU9250();
#endif
#ifdef USE_SHT20
    bool _initSHT20();
#endif
#ifdef USE_CCS811
    bool _initCCS811();
#endif
    
    // Métodos privados - Validação
    bool _validateMPUReadings(float gx, float gy, float gz, 
                             float ax, float ay, float az);
    bool _validateBMPReadings(float temperature, float pressure);
    bool _validateSHTReadings(float temperature, float humidity);
    bool _validateCCSReadings(float co2, float tvoc);
    
    // Métodos privados - Auxiliares
    void _performHealthCheck();
    bool _calibrateMPU6050();
    float _applyFilter(float newValue, float* buffer);
    float _calculateAltitude(float pressure);
};

#endif // SENSORMANAGER_H
