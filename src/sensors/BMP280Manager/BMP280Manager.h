/**
 * @file BMP280Manager.h
 * @brief Gerenciador BMP280 - Sensor de Pressão e Temperatura
 * @version 2.0.0
 * @date 2025-12-01
 * 
 * CARACTERÍSTICAS:
 * - Detecção automática de endereço I2C (0x76 ou 0x77)
 * - Validação rigorosa de leituras baseada no datasheet
 * - Detecção de sensor travado com tolerância ajustada
 * - Histórico para detecção de outliers
 * - Reinicialização automática após falhas
 * - Filtro de taxa de mudança
 * 
 * DATASHEET RANGES:
 * - Temperatura: -40°C a +85°C
 * - Pressão: 300 hPa a 1100 hPa
 * - Resolução: 0.01 hPa (pressão), 0.01°C (temperatura)
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
    void reset();
    void forceReinit();
    
    // Getters principais
    float getTemperature() const { return _temperature; }  // °C
    float getPressure() const { return _pressure; }        // hPa
    float getAltitude() const { return _altitude; }        // metros
    
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
    float _temperature;  // °C
    float _pressure;     // hPa
    float _altitude;     // metros
    
    // Estado
    bool _online;
    bool _tempValid;
    uint8_t _failCount;
    uint8_t _tempFailures;
    
    // Warm-up
    unsigned long _warmupStartTime;
    static constexpr unsigned long WARMUP_DURATION = 2000;  // 2 segundos
    
    // Reinicialização
    unsigned long _lastReinitTime;
    static constexpr unsigned long REINIT_COOLDOWN = 10000;  // 10 segundos
    
    // Histórico para detecção de outliers
    static constexpr uint8_t HISTORY_SIZE = 10;
    float _pressureHistory[HISTORY_SIZE];
    float _altitudeHistory[HISTORY_SIZE];
    float _tempHistory[HISTORY_SIZE];
    uint8_t _historyIndex;
    bool _historyFull;
    unsigned long _lastUpdateTime;
    
    // Detecção de sensor travado
    float _lastPressureRead;
    uint16_t _identicalReadings;
    static constexpr uint16_t MAX_IDENTICAL_READINGS = 200;  // ← CORRIGIDO de 50 para 200
    
    // Limites de validação (datasheet BMP280)
    static constexpr float TEMP_MIN = -40.0f;      // °C
    static constexpr float TEMP_MAX = 85.0f;       // °C
    static constexpr float PRESSURE_MIN = 300.0f;  // hPa
    static constexpr float PRESSURE_MAX = 1100.0f; // hPa
    
    // Taxa máxima de mudança por segundo
    static constexpr float MAX_PRESSURE_RATE = 5.0f;   // hPa/s (mais realista)
    static constexpr float MAX_ALTITUDE_RATE = 50.0f;  // m/s
    static constexpr float MAX_TEMP_RATE = 1.0f;       // °C/s
    
    // Métodos internos - Leitura
    bool _readRaw(float& temp, float& press, float& alt);
    bool _readRawFast();
    
    // Métodos internos - Validação
    bool _validateReading(float temp, float press, float alt);
    bool _checkRateOfChange(float temp, float press, float alt, float deltaTime);
    bool _isFrozen(float currentPressure);
    bool _isOutlier(float value, float* history, uint8_t count) const;
    float _getMedian(float* values, uint8_t count) const;
    
    // Métodos internos - Histórico
    void _updateHistory(float temp, float press, float alt);
    void _initHistory(float temp, float press, float alt);
    
    // Métodos internos - Reinicialização
    bool _needsReinit() const;
    bool _canReinit() const;
    bool _softReset();
    bool _waitForMeasurement();
    
    // Métodos internos - Cálculos
    float _calculateAltitude(float pressure) const;
};

#endif // BMP280MANAGER_H
