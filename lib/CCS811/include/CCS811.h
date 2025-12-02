/**
 * @file CCS811.h
 * @brief Driver Nativo CCS811 (Air Quality)
 * @details I2C robusto com suporte a Clock Stretching
 */

#ifndef CCS811_H
#define CCS811_H

#include <Arduino.h>
#include <Wire.h>

class CCS811 {
public:
    static constexpr uint8_t ADDR_5A = 0x5A;
    static constexpr uint8_t ADDR_5B = 0x5B;

    enum class DriveMode : uint8_t {
        IDLE = 0x00,
        MODE_1SEC = 0x10,  // Corrigido para bit position correto (Mode 1)
        MODE_10SEC = 0x20, // Mode 2
        MODE_60SEC = 0x30, // Mode 3
        MODE_250MS = 0x40  // Mode 4
    };

    explicit CCS811(TwoWire& wire = Wire);

    bool begin(uint8_t addr = ADDR_5A);
    void reset();

    // Controle
    bool setDriveMode(DriveMode mode);
    bool setEnvironmentalData(float humidity, float temperature);
    
    // Leitura
    bool available();
    bool readData();
    
    uint16_t geteCO2() const { return _eco2; }
    uint16_t getTVOC() const { return _tvoc; }
    uint8_t getError();

    // Baseline (Calibração)
    bool getBaseline(uint16_t& baseline);
    bool setBaseline(uint16_t baseline);

private:
    TwoWire* _wire;
    uint8_t _addr;
    uint16_t _eco2;
    uint16_t _tvoc;

    // Registradores
    static constexpr uint8_t REG_STATUS = 0x00;
    static constexpr uint8_t REG_MEAS_MODE = 0x01;
    static constexpr uint8_t REG_ALG_RESULT_DATA = 0x02;
    static constexpr uint8_t REG_ENV_DATA = 0x05;
    static constexpr uint8_t REG_BASELINE = 0x11;
    static constexpr uint8_t REG_HW_ID = 0x20;
    static constexpr uint8_t REG_ERROR_ID = 0xE0;
    static constexpr uint8_t REG_APP_START = 0xF4;
    static constexpr uint8_t REG_SW_RESET = 0xFF;

    static constexpr uint8_t HW_ID_CODE = 0x81;

    bool _writeRegister(uint8_t reg, uint8_t value);
    bool _writeRegisters(uint8_t reg, uint8_t* data, uint8_t len);
    bool _readRegister(uint8_t reg, uint8_t* val);
    bool _readRegisters(uint8_t reg, uint8_t* buf, uint8_t len);
};

#endif