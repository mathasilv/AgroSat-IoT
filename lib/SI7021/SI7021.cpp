/**
 * @file SI7021.cpp
 * @brief SI7021 Driver - FIX Chip Falso (Error 263)
 */
#include "SI7021.h"

// ============================================================================
// CONSTRUTOR
SI7021::SI7021(TwoWire& wire) : _wire(&wire), _addr(SI7021_I2C_ADDR), _online(false), _deviceID(0) {}

// ============================================================================
// INICIALIZAÇÃO - COM TESTE RIGOROSO
bool SI7021::begin(uint8_t addr) {
    _addr = addr;
    
    #ifdef DEBUG_SI7021
    Serial.printf("[SI7021] Testando 0x%02X...\n", _addr);
    #endif
    
    _online = false;
    
    // PASSO 1: Soft Reset (15ms)
    if (!_writeCommand(SI7021_CMD_SOFT_RESET)) {
        #ifdef DEBUG_SI7021
        Serial.println("[SI7021] FALHA: Reset");
        #endif
        return false;
    }
    delay(20);
    
    // PASSO 2: Verificar presença (simples ACK)
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) {
        #ifdef DEBUG_SI7021
        Serial.printf("[SI7021] FALHA: Não responde (0x%02X)\n", _addr);
        #endif
        return false;
    }
    
    // PASSO 3: TESTE CRÍTICO - Leitura real de umidade (0xF5)
    if (!_testRealHumidityRead()) {
        #ifdef DEBUG_SI7021
        Serial.println("[SI7021] FALHA: Não lê dados reais (CHIP FALSO?)");
        #endif
        return false;
    }
    
    // PASSO 4: Ler Device ID (opcional)
    _readRegister(SI7021_CMD_READ_ID, &_deviceID, 1);
    
    _online = true;
    #ifdef DEBUG_SI7021
    Serial.printf("[SI7021] ONLINE! ID=0x%02X\n", _deviceID);
    #endif
    return true;
}

// ============================================================================
// TESTE CRÍTICO - Leitura real (detecta chip falso)
bool SI7021::_testRealHumidityRead() {
    // Comando umidade NO HOLD (0xF5)
    if (!_writeCommand(SI7021_CMD_MEASURE_RH_NOHOLD)) return false;
    
    // Aguardar conversão (máx 12ms para 12-bit)
    delay(20);
    
    // TENTAR LEITURA 3x
    for (uint8_t retry = 0; retry < 3; retry++) {
        Wire.requestFrom(_addr, (uint8_t)3);
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read();
            uint8_t lsb = Wire.read();
            uint8_t crc = Wire.available() ? Wire.read() : 0;
            
            uint16_t raw = (msb << 8) | lsb;
            
            // VALIDAÇÃO RIGOROSA (não 0x0000 nem 0xFFFF)
            if (raw > 0x0100 && raw < 0xFE00) {
                float hum = ((125.0 * raw) / 65536.0) - 6.0;
                if (hum >= 0.0f && hum <= 100.0f) {
                    #ifdef DEBUG_SI7021
                    Serial.printf("[SI7021] Teste OK: raw=0x%04X hum=%.1f%%\n", raw, hum);
                    #endif
                    return true;
                }
            }
        }
        delay(10);
    }
    return false;
}

// ============================================================================
// LEITURA UMIDADE
bool SI7021::readHumidity(float& humidity) {
    if (!_online) return false;
    
    if (!_writeCommand(SI7021_CMD_MEASURE_RH_NOHOLD)) return false;
    delay(20);
    
    uint8_t buf[3];
    if (!_readBytes(buf, 3)) return false;
    
    uint16_t raw = (buf[0] << 8) | buf[1];
    if (raw <= 0x0100 || raw >= 0xFE00) return false;
    
    humidity = ((125.0 * raw) / 65536.0) - 6.0;
    return (humidity >= 0.0f && humidity <= 100.0f);
}

// ============================================================================
// LEITURA TEMPERATURA
bool SI7021::readTemperature(float& temperature) {
    if (!_online) return false;
    
    if (!_writeCommand(SI7021_CMD_MEASURE_T_NOHOLD)) return false;
    delay(20);
    
    uint8_t buf[3];
    if (!_readBytes(buf, 2)) return false;  // Temp = 2 bytes
    
    uint16_t raw = (buf[0] << 8) | buf[1];
    if (raw <= 0x0100 || raw >= 0xFE00) return false;
    
    temperature = ((175.72 * raw) / 65536.0) - 46.85;
    return (temperature >= -40.0f && temperature <= 125.0f);
}

// ============================================================================
// MÉTODOS BAIXO NÍVEL (robustos)
bool SI7021::_writeCommand(uint8_t cmd) {
    Wire.beginTransmission(_addr);
    Wire.write(cmd);
    return (Wire.endTransmission() == 0);
}

bool SI7021::_readBytes(uint8_t* buf, size_t len) {
    Wire.requestFrom(_addr, (uint8_t)len);
    if (Wire.available() < len) return false;
    
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

bool SI7021::_readRegister(uint8_t cmd, uint8_t* data, size_t len) {
    if (!_writeCommand(cmd)) return false;
    delay(10);
    return _readBytes(data, len);
}

void SI7021::reset() {
    _writeCommand(SI7021_CMD_SOFT_RESET);
    delay(50);
    _online = false;
}
