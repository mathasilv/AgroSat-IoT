#ifndef BMP280_HAL_H
#define BMP280_HAL_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "config.h"

// Driver mínimo do BMP280 usando HAL::I2C (sem depender de Wire/Adafruit_BMP280)
// - Temperatura em °C
// - Pressão em Pa (igual Adafruit_BMP280::readPressure)
class BMP280Hal {
public:
    explicit BMP280Hal(HAL::I2C& i2c);

    // Inicializa o sensor no endereço dado (0x76 ou 0x77)
    // Retorna true se o ID for o esperado e calibração/configuração forem aplicadas.
    bool begin(uint8_t addr);

    // Leitura de alta-nível (equivalente à lib Adafruit):
    //  - Temperatura em graus Celsius
    //  - Pressão em Pascal (Pa)
    float readTemperature();
    float readPressure();

    bool isInitialized() const { return _initialized; }

private:
    HAL::I2C* _i2c;
    uint8_t _addr;
    bool _initialized;

    // Coeficientes de calibração (datasheet BMP280, seção 3.11.2)
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    int32_t  t_fine;  // usado na compensação de pressão

    bool _readCalibration();
    bool _readRaw(int32_t& adc_T, int32_t& adc_P);

    bool _readBytes(uint8_t reg, uint8_t* buf, size_t len);
    bool _write8(uint8_t reg, uint8_t value);
};

#endif // BMP280_HAL_H
