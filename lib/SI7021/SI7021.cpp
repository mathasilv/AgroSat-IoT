/**
 * @file SI7021.cpp
 * @brief Implementação do Driver SI7021 com proteção de Polling I2C
 */

#include "SI7021.h"

SI7021::SI7021(TwoWire& wire) : _wire(&wire) {}

bool SI7021::begin() {
    if (!isSensorPresent()) {
        return false;
    }
    reset();
    return true;
}

void SI7021::reset() {
    _writeCommand(CMD_RESET);
    delay(50); // Datasheet: Powerup/Reset time ~15ms
}

bool SI7021::readHumidity(float& humidity) {
    uint16_t raw = _readSensorData(CMD_MEASURE_RH_NOHOLD);
    if (raw == 0xFFFF) return false;

    // Fórmula Datasheet: %RH = ((125 * Code) / 65536) - 6
    float val = (125.0f * raw) / 65536.0f - 6.0f;
    
    // Clamp 0-100%
    if (val < 0.0f) val = 0.0f;
    if (val > 100.0f) val = 100.0f;
    
    humidity = val;
    return true;
}

bool SI7021::readTemperature(float& temperature) {
    uint16_t raw = _readSensorData(CMD_MEASURE_TEMP_NOHOLD);
    if (raw == 0xFFFF) return false;

    // Fórmula Datasheet: Temp = ((175.72 * Code) / 65536) - 46.85
    temperature = (175.72f * raw) / 65536.0f - 46.85f;
    return true;
}

bool SI7021::isSensorPresent() {
    _wire->beginTransmission(I2C_ADDR);
    return (_wire->endTransmission() == 0);
}

uint8_t SI7021::getDeviceID() {
    // Leitura simplificada do User Register para confirmar comunicação
    _wire->beginTransmission(I2C_ADDR);
    _wire->write(CMD_READ_USER_REG);
    if (_wire->endTransmission() != 0) return 0;

    _wire->requestFrom(I2C_ADDR, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    return 0;
}

// === Métodos Privados ===

bool SI7021::_writeCommand(uint8_t cmd) {
    _wire->beginTransmission(I2C_ADDR);
    _wire->write(cmd);
    return (_wire->endTransmission() == 0);
}

uint16_t SI7021::_readSensorData(uint8_t cmd) {
    // 1. Enviar comando de leitura (No Hold)
    if (!_writeCommand(cmd)) return 0xFFFF;

    // 2. Aguardar conversão (Datasheet Max: 12ms para RH, 10.8ms para Temp)
    delay(25); 

    // 3. Polling seguro: "Ping" antes de ler para evitar travar o barramento
    // Tenta até 10 vezes verificar se o sensor já terminou (ACK)
    bool ready = false;
    for (uint8_t i = 0; i < 10; i++) {
        _wire->beginTransmission(I2C_ADDR);
        if (_wire->endTransmission() == 0) {
            ready = true;
            break;
        }
        delay(5);
    }

    if (!ready) return 0xFFFF;

    // 4. Ler dados (2 bytes: MSB, LSB) - ignorando Checksum por enquanto
    if (_wire->requestFrom(I2C_ADDR, (uint8_t)2) != 2) return 0xFFFF;
    
    uint8_t msb = _wire->read();
    uint8_t lsb = _wire->read();

    return (msb << 8) | lsb;
}