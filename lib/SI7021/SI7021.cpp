/**
 * @file SI7021.cpp
 * @brief Driver nativo SI7021 - CORREÇÃO DE POLL (PING ANTES DE LER)
 */

#include "SI7021.h"

#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#endif

// Delay aumentado para garantir conversão
static constexpr uint8_t SI7021_INIT_DELAY_MS = 50;

SI7021::SI7021(TwoWire& wire) : _wire(&wire), _addr(SI7021_I2C_ADDR), _online(false), _deviceID(0) { }

bool SI7021::begin(uint8_t addr) {
    _addr = addr;
    _online = false;
    DEBUG_PRINTLN("[SI7021] Inicializando...");

    if (!_verifyPresence()) return false;

    reset();
    delay(SI7021_INIT_DELAY_MS);

    uint8_t userReg = 0;
    if (!_readRegister(SI7021_CMD_READ_USER_REG, &userReg, 1)) return false;
    _deviceID = userReg;

    float testHum = 0.0f;
    if (!readHumidity(testHum)) return false;

    _online = true;
    DEBUG_PRINTF("[SI7021] OK! RH=%.1f%%\n", testHum);
    return true;
}

bool SI7021::readHumidity(float& humidity) {
    _wire->beginTransmission(_addr);
    _wire->write(SI7021_CMD_MEASURE_RH_NOHOLD);
    if (_wire->endTransmission() != 0) {
        humidity = -999.0;
        return false;
    }
    
    // Aguarda tempo mínimo de conversão
    delay(50); 

    // Tenta ler com polling inteligente
    uint8_t retries = 10;
    while (retries-- > 0) {
        // [FIX CRÍTICO] Verificar se o sensor está pronto (ACK) ANTES de pedir dados
        // Isso evita que o requestFrom gere o Error 263 no log
        _wire->beginTransmission(_addr);
        if (_wire->endTransmission() == 0) {
            // Sensor respondeu (ACK), está pronto!
            if (_wire->requestFrom(_addr, (uint8_t)2) == 2) {
                uint16_t rawHum = (_wire->read() << 8) | _wire->read();
                humidity = ((125.0 * rawHum) / 65536.0) - 6.0;
                if (humidity < 0.0) humidity = 0.0;
                if (humidity > 100.0) humidity = 100.0;
                return true;
            }
        }
        // Se deu NACK, espera um pouco e tenta de novo
        delay(10);
    }
    
    humidity = -999.0;
    return false;
}

bool SI7021::readTemperature(float& temperature) {
    _wire->beginTransmission(_addr);
    _wire->write(SI7021_CMD_MEASURE_T_NOHOLD);
    if (_wire->endTransmission() != 0) {
        temperature = -999.0;
        return false;
    }
    
    delay(50); 

    uint8_t retries = 10;
    while (retries-- > 0) {
        // [FIX CRÍTICO] Ping antes de ler
        _wire->beginTransmission(_addr);
        if (_wire->endTransmission() == 0) {
            if (_wire->requestFrom(_addr, (uint8_t)2) == 2) {
                uint16_t rawTemp = (_wire->read() << 8) | _wire->read();
                temperature = ((175.72 * rawTemp) / 65536.0) - 46.85;
                return true;
            }
        }
        delay(10);
    }
    
    temperature = -999.0;
    return false;
}

void SI7021::reset() {
    _writeCommand(SI7021_CMD_SOFT_RESET);
    delay(20);
}

bool SI7021::_verifyPresence() {
    _wire->beginTransmission(_addr);
    return (_wire->endTransmission() == 0);
}

bool SI7021::_writeCommand(uint8_t cmd) {
    _wire->beginTransmission(_addr);
    _wire->write(cmd);
    return (_wire->endTransmission() == 0);
}

bool SI7021::_readBytes(uint8_t* buf, size_t len) {
    if (_wire->requestFrom(_addr, (uint8_t)len) == len) {
        for (size_t i = 0; i < len; i++) buf[i] = _wire->read();
        return true;
    }
    return false;
}

bool SI7021::_readRegister(uint8_t cmd, uint8_t* data, size_t len) {
    if (!_writeCommand(cmd)) return false;
    delay(10);
    return _readBytes(data, len);
}