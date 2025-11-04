/**
 * @file SensorManager.h
 * @brief Gerenciador de sensores com auto-detecção e bibliotecas corrigidas
 * @version 2.1.0
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Bibliotecas obrigatórias
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>

// Bibliotecas opcionais (condicionais)
#ifdef USE_MPU9250
  #include <MPU9250_WE.h>
#endif

#ifdef USE_SHT20
  #include <SHT2x.h>          // robtillaart/SHT2x
#endif

#ifdef USE_CCS811
  #include <Adafruit_CCS811.h>
#endif

class SensorManager {
public:
    SensorManager();
    bool begin();
    void update();
    
    // Calibração
    bool calibrateIMU();
    
    // Getters obrigatórios (compatibilidade OBSAT)
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
    float getHumidity();        // SHT20
    float getCO2();            // CCS811
    float getTVOC();           // CCS811
    float getMagX();           // MPU9250
    float getMagY();           // MPU9250
    float getMagZ();           // MPU9250
    
    // Status dos sensores
    bool isMPU6050Online();
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSHT20Online();
    bool isCCS811Online();
    bool isCalibrated();
    
    // Recuperação
    void resetMPU6050();
    void resetBMP280();
    void resetAll();
    
    // Debug
    void printSensorStatus();
    void scanI2C();
    void getRawData(sensors_event_t& accel, sensors_event_t& gyro, sensors_event_t& temp);

private:
    // Objetos dos sensores obrigatórios
    Adafruit_MPU6050 _mpu6050;
    Adafruit_BMP280 _bmp280;
    
    // Objetos dos sensores opcionais
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
    float _temperature;
    float _pressure;
    float _altitude;
    float _humidity;
    float _co2Level;
    float _tvoc;
    float _seaLevelPressure;
    
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;
    
    // Offsets de calibração
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    float _accelOffsetX, _accelOffsetY, _accelOffsetZ;
    
    // Status dos sensores
    bool _mpu6050Online;
    bool _mpu9250Online;
    bool _bmp280Online;
    bool _sht20Online;
    bool _ccs811Online;
    bool _calibrated;
    
    // Controle de timing
    uint32_t _lastReadTime;
    uint32_t _lastCCS811Read;
    uint32_t _lastSHT20Read;
    uint32_t _lastHealthCheck;
    uint16_t _consecutiveFailures;
    
    // Filtros
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    
    // Métodos privados de inicialização
    bool _initMPU6050();
    bool _initMPU9250();
    bool _initBMP280();
    bool _initSHT20();
    bool _initCCS811();
    
    // Métodos de validação
    bool _validateMPUReadings(const sensors_event_t& accel, const sensors_event_t& gyro);
    bool _validateBMPReadings(float temperature, float pressure);
    bool _validateSHTReadings(float temperature, float humidity);
    bool _validateCCSReadings(float co2, float tvoc);
    
    // Utilitários
    void _performHealthCheck();
    float _applyFilter(float newValue, float* buffer);
    float _calculateAltitude(float pressure);
    bool _calibrateMPU6050();
};

#endif // SENSOR_MANAGER_H
