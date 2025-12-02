/**
 * @file SI7021.h
 * @brief Driver I2C para Sensor de Temperatura e Umidade SI7021
 * @details Implementação de baixo nível. Foca apenas na comunicação I2C.
 */

#ifndef SI7021_H
#define SI7021_H

#include <Arduino.h>
#include <Wire.h>

class SI7021 {
public:
    static constexpr uint8_t I2C_ADDR = 0x40;

    // Comandos (Datasheet)
    static constexpr uint8_t CMD_MEASURE_RH_HOLD = 0xE5;
    static constexpr uint8_t CMD_MEASURE_RH_NOHOLD = 0xF5;
    static constexpr uint8_t CMD_MEASURE_TEMP_HOLD = 0xE3;
    static constexpr uint8_t CMD_MEASURE_TEMP_NOHOLD = 0xF3;
    static constexpr uint8_t CMD_READ_TEMP_PREV = 0xE0;
    static constexpr uint8_t CMD_RESET = 0xFE;
    static constexpr uint8_t CMD_WRITE_USER_REG = 0xE6;
    static constexpr uint8_t CMD_READ_USER_REG = 0xE7;
    static constexpr uint8_t CMD_READ_ID_1 = 0xFA;
    static constexpr uint8_t CMD_READ_ID_2 = 0xFC;

    explicit SI7021(TwoWire& wire = Wire);

    bool begin();
    void reset();

    // Retorna true se leitura ok. Valores em humidity (%) e temperature (°C)
    bool readHumidity(float& humidity);
    bool readTemperature(float& temperature);

    // Utilitários
    uint8_t getDeviceID();
    bool isSensorPresent();

private:
    TwoWire* _wire;
    
    bool _writeCommand(uint8_t cmd);
    uint16_t _readSensorData(uint8_t cmd);
};

#endif // SI7021_H