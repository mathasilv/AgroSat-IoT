#ifndef ENV_SERVICE_H
#define ENV_SERVICE_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "BMP280Hal.h"
#include "SI7021Hal.h"
#include "config.h"

// Serviço de ambiente: BMP280 + SI7021 (temp, pressão, altitude, umidade)
class EnvService {
public:
    EnvService(BMP280Hal& bmp, SI7021Hal& si7021, HAL::I2C& i2c);

    // Inicializa BMP280 + SI7021 com método robusto
    bool begin();

    // Atualiza leituras (chamar em ~SENSOR_READ_INTERVAL)
    void update();

    // Status
    bool isBmpOnline()    const { return _bmpOnline; }
    bool isSiOnline()     const { return _siOnline; }
    bool isBmpTempValid() const { return _bmpTempValid; }
    bool isSiTempValid()  const { return _siTempValid; }

    // Valores individuais
    float getBmpTemperature() const { return _temperatureBMP; }
    float getSiTemperature()  const { return _temperatureSI; }
    float getPressure()       const { return _pressure; }
    float getAltitude()       const { return _altitude; }
    float getHumidity()       const { return _humidity; }

    // Temperatura “oficial” com redundância (SI7021 > BMP280)
    float getTemperature()    const { return _temperature; }

private:
    HAL::I2C*  _i2c;
    BMP280Hal& _bmp;
    SI7021Hal& _si;

    // STATUS
    bool _bmpOnline;
    bool _siOnline;

    // DADOS
    float _temperature;      // escolhida (redundância)
    float _temperatureBMP;
    float _temperatureSI;
    float _pressure;
    float _altitude;
    float _humidity;
    float _seaLevelPressure;

    bool    _bmpTempValid;
    bool    _siTempValid;
    uint8_t _bmpTempFailures;
    uint8_t _siTempFailures;
    uint8_t _bmpFailCount;

    // CONTROLE DE TEMPO / HISTÓRICO (BMP280)
    unsigned long _warmupStartTime;
    unsigned long _lastUpdateTime;
    float         _pressureHistory[5];
    float         _altitudeHistory[5];
    float         _tempHistory[5];
    uint8_t       _historyIndex;
    bool          _historyFull;
    float         _lastPressureRead;
    uint8_t       _identicalReadings;

    // SI7021
    unsigned long _lastSiRead;

    // Limite de falhas consecutivas de temperatura (SI7021)
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;

    // MÉTODOS INTERNOS
    bool  _initBMP280();
    bool  _initSI7021();
    void  _updateBMP280();
    void  _updateSI7021();

    bool  _waitForBMP280Measurement();
    bool  _validateBMP280Reading();
    float _getMedian(float* values, uint8_t count);
    bool  _isOutlier(float value, float* history, uint8_t count);

    bool  _validateReading(float value, float minValid, float maxValid);
    void  _updateTemperatureRedundancy();
    float _calculateAltitude(float pressure);
};

#endif // ENV_SERVICE_H
