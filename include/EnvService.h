#ifndef ENV_SERVICE_H
#define ENV_SERVICE_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "BMP280Hal.h"
#include "SI7021Hal.h"
#include "config.h"  // ✅ Usa as definições de config.h

// Serviço de ambiente: BMP280 + SI7021
class EnvService {
public:
    EnvService(BMP280Hal& bmp, SI7021Hal& si7021, HAL::I2C& i2c);

    bool begin();
    void update();

    // Status
    bool isBmpOnline()    const { return _bmpOnline; }
    bool isSiOnline()     const { return _siOnline; }
    bool isBmpTempValid() const { return _bmpTempValid; }
    bool isSiTempValid()  const { return _siTempValid; }

    // Valores
    float getBmpTemperature() const { return _temperatureBMP; }
    float getSiTemperature()  const { return _temperatureSI; }
    float getPressure()       const { return _pressure; }
    float getAltitude()       const { return _altitude; }
    float getHumidity()       const { return _humidity; }
    float getTemperature()    const { return _temperature; }

    // ✅ Getters adicionais
    uint8_t getBmpAddress()   const { return _bmpAddress; }
    uint8_t getBmpMode()      const { return _bmpMode; }

private:
    HAL::I2C*  _i2c;
    BMP280Hal& _bmp;
    SI7021Hal& _si;

    // STATUS
    bool _bmpOnline;
    bool _siOnline;

    // DADOS
    float _temperature;
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

    // ✅ Novos membros
    uint8_t _bmpAddress;
    uint8_t _bmpMode;

    // CONTROLE
    unsigned long _warmupStartTime;
    unsigned long _lastUpdateTime;
    float         _pressureHistory[5];
    float         _altitudeHistory[5];
    float         _tempHistory[5];
    uint8_t       _historyIndex;
    bool          _historyFull;
    float         _lastPressureRead;
    uint8_t       _identicalReadings;
    unsigned long _lastSiRead;

    // ========================================
    // ✅ CONSTANTES APENAS DO BMP280 (não definidas em config.h)
    // ========================================
    
    // Registradores BMP280
    static constexpr uint8_t BMP280_REGISTER_CONTROL = 0xF4;
    static constexpr uint8_t BMP280_REGISTER_STATUS  = 0xF3;
    
    // Bits do registrador de status
    static constexpr uint8_t BMP280_STATUS_MEASURING = 0x08;
    static constexpr uint8_t BMP280_STATUS_IM_UPDATE = 0x01;
    
    // Modos de operação
    static constexpr uint8_t BMP280_MODE_SLEEP  = 0x00;
    static constexpr uint8_t BMP280_MODE_FORCED = 0x01;
    static constexpr uint8_t BMP280_MODE_NORMAL = 0x03;

    // ========================================
    // ✅ CONSTANTES ESPECÍFICAS (não em config.h)
    // ========================================
    
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;

    // ========================================
    // MÉTODOS INTERNOS
    // ========================================
    
    // BMP280
    bool  _initBMP280();
    void  _updateBMP280();
    bool  _forceBMP280Measurement();
    bool  _isBMP280Ready();
    bool  _waitForBMP280Measurement();
    bool  _validateBMP280Reading();
    float _getMedian(float* values, uint8_t count);
    bool  _isOutlier(float value, float* history, uint8_t count);

    // SI7021
    bool  _initSI7021();
    void  _updateSI7021();

    // Comum
    bool  _validateReading(float value, float minValid, float maxValid);
    void  _updateTemperatureRedundancy();
    float _calculateAltitude(float pressure);
};

#endif // ENV_SERVICE_H
