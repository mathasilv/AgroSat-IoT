/**
 * @file SI7021.h
 * @brief Driver nativo SI7021 - Umidade + Temperatura (Arduino Framework)
 * @author AgroSat-IoT Team
 * @version 1.0.0
 */
#ifndef SI7021_H
#define SI7021_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// ============================================================================
// CONSTANTES (Datasheet Si7021-A20)
static constexpr uint8_t SI7021_I2C_ADDR = 0x40;
static constexpr uint8_t SI7021_CMD_MEASURE_RH_NOHOLD = 0xF5;
static constexpr uint8_t SI7021_CMD_MEASURE_T_NOHOLD = 0xF3;
static constexpr uint8_t SI7021_CMD_SOFT_RESET = 0xFE;
static constexpr uint8_t SI7021_CMD_READ_USER_REG = 0xE7;
static constexpr uint8_t SI7021_CMD_READ_ID = 0xFC;
static constexpr uint8_t SI7021_CMD_ID_1 = 0x80;

// ============================================================================
// CLASSE PRINCIPAL
class SI7021 {
public:
    explicit SI7021(TwoWire& wire = Wire);
    
    bool begin(uint8_t addr = SI7021_I2C_ADDR);
    bool readHumidity(float& humidity);
    bool readTemperature(float& temperature);
    bool isOnline() const { return _online; }
    uint8_t getDeviceID() const { return _deviceID; }
    
    void reset();
    
private:
    TwoWire* _wire;
    uint8_t _addr;
    bool _online;
    uint8_t _deviceID;
    
    bool _writeCommand(uint8_t cmd);
    bool _readBytes(uint8_t* buf, size_t len);
    bool _readRegister(uint8_t cmd, uint8_t* data, size_t len);
    bool _verifyPresence();
};

#endif // SI7021_H
