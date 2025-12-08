/**
 * @file BMP280Manager.h
 * @brief Gerenciador do sensor BMP280 (Pressão e Temperatura)
 * @details Responsável pela inicialização, leitura, validação e detecção de anomalias.
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
    
    // Controle
    bool begin();
    void update();
    void reset();
    void forceReinit();
    
    // Getters
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
    
    // Dados armazenados
    float _temperature;
    float _pressure;
    float _altitude;
    
    // Estado do sistema
    bool _online;
    bool _tempValid;
    uint8_t _failCount;
    unsigned long _lastReinitTime;
    
    // Histórico e Detecção de Erros
    static constexpr uint8_t HISTORY_SIZE = 10;
    float _pressureHistory[HISTORY_SIZE];
    float _altitudeHistory[HISTORY_SIZE];
    float _tempHistory[HISTORY_SIZE];
    uint8_t _historyIndex;
    bool _historyFull;
    unsigned long _lastUpdateTime;
    unsigned long _warmupStartTime;

    // Detecção de sensor travado
    float _lastPressureRead;
    uint16_t _identicalReadings;
    
    // Constantes de Configuração e Limites
    static constexpr unsigned long WARMUP_DURATION = 2000;      // 2s
    static constexpr unsigned long REINIT_COOLDOWN = 10000;     // 10s
    static constexpr uint16_t MAX_IDENTICAL_READINGS = 200;     // Ciclos para considerar travado
    
    // Limites (Datasheet)
    static constexpr float TEMP_MIN = -60.0f;
    static constexpr float TEMP_MAX = 85.0f;
    static constexpr float PRESSURE_MIN = 5.0f;
    static constexpr float PRESSURE_MAX = 1100.0f;
    
    // Taxas máximas de mudança (Física)
    static constexpr float MAX_PRESSURE_RATE = 5.0f;   // hPa/s
    static constexpr float MAX_ALTITUDE_RATE = 50.0f;  // m/s
    static constexpr float MAX_TEMP_RATE = 1.0f;       // °C/s
    
    // Métodos internos
    bool _readRaw(float& temp, float& press, float& alt);
    bool _validateReading(float temp, float press, float alt);
    bool _checkRateOfChange(float temp, float press, float alt, float deltaTime);
    bool _isFrozen(float currentPressure);
    bool _isOutlier(float value, float* history, uint8_t count) const;
    float _getMedian(float* values, uint8_t count) const;
    
    void _updateHistory(float temp, float press, float alt);
    void _initHistoryValues();
    
    bool _canReinit() const;
};

#endif // BMP280MANAGER_H