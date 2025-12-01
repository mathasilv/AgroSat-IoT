/**
 * @file SI7021.cpp
 * @brief Driver nativo SI7021 - Implementação
 */

#include "SI7021.h"

// Se não houver macros de debug definidas em outro lugar, define padrão
#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(x)      Serial.println(x)
#define DEBUG_PRINTF(...)     Serial.printf(__VA_ARGS__)
#endif

// Timeouts e delays
static constexpr uint16_t SI7021_MEASURE_TIMEOUT_MS = 20;  // Conversão típica ~12ms
static constexpr uint8_t  SI7021_RESET_DELAY_MS     = 15;
static constexpr uint8_t  SI7021_INIT_DELAY_MS      = 50;

SI7021::SI7021(TwoWire& wire)
    : _wire(&wire),
      _addr(SI7021_I2C_ADDR),
      _online(false),
      _deviceID(0)
{
}

bool SI7021::begin(uint8_t addr)
{
    _addr   = addr;
    _online = false;

    DEBUG_PRINTLN("[SI7021] ========================================");
    DEBUG_PRINTLN("[SI7021] Inicializando driver nativo...");

    // Verificar presença no barramento (ACK no endereço)
    if (!_verifyPresence()) {
        DEBUG_PRINTLN("[SI7021] Sensor não detectado no endereço I2C informado");
        return false;
    }

    // Soft reset para garantir estado conhecido
    reset();
    delay(SI7021_INIT_DELAY_MS);

    // Ler User Register (validação simples de comunicação)
    uint8_t userReg = 0;
    if (!_readRegister(SI7021_CMD_READ_USER_REG, &userReg, 1)) {
        DEBUG_PRINTLN("[SI7021] Falha ao ler User Register (0xE7)");
        return false;
    }

    // Armazena esse valor em _deviceID apenas como referência simples
    _deviceID = userReg;
    DEBUG_PRINTF("[SI7021] User Register: 0x%02X\n", _deviceID);

    // Teste de leitura inicial de umidade
    float testHum = 0.0f;
    if (!readHumidity(testHum)) {
        DEBUG_PRINTLN("[SI7021] Falha no teste inicial de umidade");
        return false;
    }

    if (testHum < 0.0f || testHum > 100.0f) {
        DEBUG_PRINTF("[SI7021] Leitura inicial inválida de umidade: %.1f%%\n", testHum);
        return false;
    }

    _online = true;
    DEBUG_PRINTF("[SI7021] Inicializado com sucesso! RH=%.1f%%\n", testHum);
    DEBUG_PRINTLN("[SI7021] ========================================");

    return true;
}

bool SI7021::readHumidity(float& humidity) {
    // Iniciar conversão
    Wire.beginTransmission(_addr);
    Wire.write(SI7021_CMD_MEASURE_RH_NOHOLD);
    if (Wire.endTransmission() != 0) {
        humidity = -999.0;
        return false;
    }
    
    // ✅ AGUARDAR CONVERSÃO COMPLETA (umidade demora ~12-20ms)
    delay(50);  // Aumentado de 15ms para 30ms para evitar timeout
    
    // ✅ TENTAR LER COM RETRY (até 5 tentativas)
    uint8_t retries = 5;  
    while (retries-- > 0) {
        uint8_t received = Wire.requestFrom(_addr, (uint8_t)2);
        
        if (received == 2) {
            // Leitura bem-sucedida
            uint16_t rawHum = Wire.read() << 8 | Wire.read();
            humidity = ((125.0 * rawHum) / 65536.0) - 6.0;
            
            // Validar range (conforme datasheet)
            if (humidity < 0.0) humidity = 0.0;
            if (humidity > 100.0) humidity = 100.0;
            
            return true;
        }
        
        // Sensor ainda processando, aguardar mais
        delay(10);
    }
    
    // Falhou após todas as tentativas
    humidity = -999.0;
    return false;
}


bool SI7021::readTemperature(float &temperature) {
    // Iniciar conversão
    Wire.beginTransmission(_addr);
    Wire.write(SI7021_CMD_MEASURE_T_NOHOLD);
    if (Wire.endTransmission() != 0) {
        temperature = -999.0;
        return false;
    }
    
    // ✅ AGUARDAR CONVERSÃO (temperatura demora ~10ms)
    delay(50);  // Margem de segurança
    
    // ✅ TENTAR LER COM RETRY
    uint8_t retries = 5;
    while (retries-- > 0) {
        uint8_t received = Wire.requestFrom(_addr, (uint8_t)2);
        
        if (received == 2) {
            // Leitura bem-sucedida
            uint16_t rawTemp = Wire.read() << 8 | Wire.read();
            temperature = ((175.72 * rawTemp) / 65536.0) - 46.85;
            
            // Validar range típico (-40°C a +125°C)
            if (temperature < -40.0) temperature = -40.0;
            if (temperature > 125.0) temperature = 125.0;
            
            return true;
        }
        
        delay(10);
    }
    
    // Falhou
    temperature = -999.0;
    return false;
}


void SI7021::reset()
{
    DEBUG_PRINTLN("[SI7021] Executando soft reset...");
    _writeCommand(SI7021_CMD_SOFT_RESET);
    delay(SI7021_RESET_DELAY_MS);
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

bool SI7021::_verifyPresence()
{
    _wire->beginTransmission(_addr);
    const uint8_t error = _wire->endTransmission();
    return (error == 0);
}

bool SI7021::_writeCommand(uint8_t cmd)
{
    _wire->beginTransmission(_addr);
    _wire->write(cmd);
    const uint8_t error = _wire->endTransmission();
    return (error == 0);
}

bool SI7021::_readBytes(uint8_t* buf, size_t len)
{
    if (buf == nullptr || len == 0) {
        return false;
    }

    uint8_t attempts = 0;
    static constexpr uint8_t MAX_ATTEMPTS = 5;

    while (attempts++ < MAX_ATTEMPTS) {
        const size_t received = _wire->requestFrom(_addr, static_cast<uint8_t>(len));
        if (received == len) {
            for (size_t i = 0; i < len; i++) {
                buf[i] = _wire->read();
            }
            return true;
        }

        // Pequeno delay antes de tentar novamente
        delay(5);
    }

    return false;
}

bool SI7021::_readRegister(uint8_t cmd, uint8_t* data, size_t len)
{
    if (!_writeCommand(cmd)) {
        return false;
    }

    // Pequeno delay para o sensor preparar o dado
    delay(5);

    return _readBytes(data, len);
}
