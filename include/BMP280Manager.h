/**
 * @file BMP280Manager.h
 * @brief Gerenciador dedicado do sensor BMP280 - Módulo isolado
 * @version 1.0.0
 * @date 2025-11-30
 * 
 * CARACTERÍSTICAS:
 * - Usa biblioteca BMP280 nativa (sem Adafruit)
 * - Validação avançada com histórico e detecção de outliers
 * - Reinicialização automática em caso de falhas
 * - Detecção de sensor travado
 * - Warm-up com período de estabilização
 * - Interface limpa para integração com SensorManager
 */

#ifndef BMP280MANAGER_H
#define BMP280MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "BMP280.h"
#include "config.h"

class BMP280Manager {
public:
    BMP280Manager();
    
    // Inicialização e controle
    bool begin();
    void update();
    void forceReinit();
    void reset();
    
    // Getters principais
    float getTemperature() const { return _temperature; }
    float getPressure() const { return _pressure; }
    float getAltitude() const { return _altitude; }
    
    // Status
    bool isOnline() const { return _online; }
    bool isTempValid() const { return _tempValid; }
    uint8_t getFailCount() const { return _failCount; }
    
    // Diagnóstico
    void printStatus() const;
    
private:
    // Hardware
    BMP280 _bmp280;
    
    // Dados atuais
    float _temperature;
    float _pressure;
    float _altitude;
    
    // Estado
    bool _online;
    bool _tempValid;
    uint8_t _failCount;
    uint8_t _tempFailures;
    
    // Histórico para validação (5 últimas leituras)
    static constexpr uint8_t HISTORY_SIZE = 5;
    float _pressureHistory[HISTORY_SIZE];
    float _altitudeHistory[HISTORY_SIZE];
    float _tempHistory[HISTORY_SIZE];
    uint8_t _historyIndex;
    bool _historyFull;
    
    // Controle de reinicialização
    unsigned long _lastReinitTime;
    static constexpr unsigned long REINIT_COOLDOWN = 10000; // 10s
    
    // Warm-up
    unsigned long _warmupStartTime;
    static constexpr unsigned long WARMUP_DURATION = 30000; // 30s
    
    // Detecção de travamento
    float _lastPressureRead;
    uint8_t _identicalReadings;
    static constexpr uint8_t MAX_IDENTICAL_READINGS = 50;
    
    // Limites de validação (do config.h)
    static constexpr float PRESSURE_MIN = PRESSURE_MIN_VALID;
    static constexpr float PRESSURE_MAX = PRESSURE_MAX_VALID;
    static constexpr float TEMP_MIN = TEMP_MIN_VALID;
    static constexpr float TEMP_MAX = TEMP_MAX_VALID;
    
    // Taxa máxima de mudança
    static constexpr float MAX_PRESSURE_RATE = 20.0;  // hPa/s
    static constexpr float MAX_ALTITUDE_RATE = 150.0; // m/s
    static constexpr float MAX_TEMP_RATE = 0.1;       // °C/s
    
    unsigned long _lastUpdateTime;
    
    // Métodos internos - Inicialização
    bool _initSensor();
    bool _softReset();
    bool _waitForMeasurement();
    
    // Métodos internos - Leitura
    bool _readRaw(float& temp, float& press, float& alt);
    bool _validateReading(float temp, float press, float alt);
    
    // Métodos internos - Validação avançada
    bool _checkRateOfChange(float temp, float press, float alt, float deltaTime);
    bool _isOutlier(float value, float* history, uint8_t count) const;
    float _getMedian(float* values, uint8_t count) const;
    bool _isFrozen(float currentPressure);
    
    // Métodos internos - Histórico
    void _updateHistory(float temp, float press, float alt);
    void _initHistory(float temp, float press, float alt);
    
    // Métodos internos - Reinicialização
    bool _needsReinit() const;
    bool _canReinit() const;
    
    // Utilitário
    float _calculateAltitude(float pressure) const;
};

#endif // BMP280MANAGER_H
