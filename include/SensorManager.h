/**
 * @file SensorManager.h
 * @brief Gerenciamento de todos os sensores do CubeSat
 * @version 1.0.0
 * 
 * Responsável por:
 * - MPU6050 (Giroscópio e Acelerômetro)
 * - BMP280 (Pressão e Temperatura)
 * - Calibração e filtragem de dados
 * - Detecção de falhas e redundância
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include "config.h"

class SensorManager {
public:
    /**
     * @brief Construtor
     */
    SensorManager();
    
    /**
     * @brief Inicializa todos os sensores
     * @return true se todos inicializados com sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras de todos os sensores
     */
    void update();
    
    /**
     * @brief Calibra o MPU6050 (executar em solo, nivelado)
     */
    bool calibrateMPU6050();
    
    /**
     * @brief Retorna temperatura do BMP280 (°C)
     */
    float getTemperature();
    
    /**
     * @brief Retorna pressão atmosférica (hPa)
     */
    float getPressure();
    
    /**
     * @brief Retorna altitude calculada (metros)
     */
    float getAltitude();
    
    /**
     * @brief Retorna giroscópio X (rad/s)
     */
    float getGyroX();
    
    /**
     * @brief Retorna giroscópio Y (rad/s)
     */
    float getGyroY();
    
    /**
     * @brief Retorna giroscópio Z (rad/s)
     */
    float getGyroZ();
    
    /**
     * @brief Retorna acelerômetro X (m/s²)
     */
    float getAccelX();
    
    /**
     * @brief Retorna acelerômetro Y (m/s²)
     */
    float getAccelY();
    
    /**
     * @brief Retorna acelerômetro Z (m/s²)
     */
    float getAccelZ();
    
    /**
     * @brief Retorna magnitude da aceleração (m/s²)
     */
    float getAccelMagnitude();
    
    /**
     * @brief Retorna status dos sensores
     */
    bool isMPU6050Online();
    bool isBMP280Online();
    
    /**
     * @brief Reinicia sensor com problema
     */
    void resetMPU6050();
    void resetBMP280();
    
    /**
     * @brief Retorna dados brutos para debug
     */
    void getRawData(sensors_event_t& accel, sensors_event_t& gyro, sensors_event_t& temp);

private:
    // Objetos dos sensores
    Adafruit_MPU6050 _mpu;
    Adafruit_BMP280 _bmp;
    
    // Dados dos sensores
    float _temperature;
    float _pressure;
    float _altitude;
    float _seaLevelPressure;
    
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    
    // Offsets de calibração
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    float _accelOffsetX, _accelOffsetY, _accelOffsetZ;
    
    // Status
    bool _mpuOnline;
    bool _bmpOnline;
    bool _calibrated;
    
    // Controle de tempo
    uint32_t _lastReadTime;
    
    // Filtros (média móvel simples)
    static const uint8_t FILTER_SIZE = 5;
    float _accelXBuffer[FILTER_SIZE];
    float _accelYBuffer[FILTER_SIZE];
    float _accelZBuffer[FILTER_SIZE];
    uint8_t _filterIndex;
    
    /**
     * @brief Aplica filtro de média móvel
     */
    float _applyFilter(float newValue, float* buffer);
    
    /**
     * @brief Calcula altitude a partir da pressão
     */
    float _calculateAltitude(float pressure);
};

#endif // SENSOR_MANAGER_H
