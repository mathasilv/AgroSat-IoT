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

bool SI7021::readHumidity(float& humidity)
{
    if (!_online) {
        humidity = -999.0f;
        return false;
    }

    // Enviar comando de medição (NO HOLD MODE)
    if (!_writeCommand(SI7021_CMD_MEASURE_RH_NOHOLD)) {
        DEBUG_PRINTLN("[SI7021] Erro ao enviar comando de leitura de umidade (0xF5)");
        humidity = -999.0f;
        return false;
    }

    // Aguardar conversão (datasheet: tipicamente ~12 ms para 12-bit)
    delay(SI7021_MEASURE_TIMEOUT_MS);

    // Ler 2 bytes de dados (ignorando CRC)
    uint8_t data[2] = {0, 0};
    if (!_readBytes(data, 2)) {
        DEBUG_PRINTLN("[SI7021] Timeout/erro ao ler bytes de umidade");
        humidity = -999.0f;
        return false;
    }

    // Conversão conforme datasheet:
    // RH = (125 * raw / 65536) - 6
    const uint16_t raw = static_cast<uint16_t>((data[0] << 8) | data[1]);
    humidity = (125.0f * static_cast<float>(raw) / 65536.0f) - 6.0f;

    // Limita para faixa física 0..100 %
    if (humidity < 0.0f)  humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    return true;
}

bool SI7021::readTemperature(float& temperature)
{
    if (!_online) {
        temperature = -999.0f;
        return false;
    }

    // Enviar comando de medição (NO HOLD MODE)
    if (!_writeCommand(SI7021_CMD_MEASURE_T_NOHOLD)) {
        DEBUG_PRINTLN("[SI7021] Erro ao enviar comando de leitura de temperatura (0xF3)");
        temperature = -999.0f;
        return false;
    }

    // Aguardar conversão
    delay(SI7021_MEASURE_TIMEOUT_MS);

    // Ler 2 bytes de dados (ignorando CRC)
    uint8_t data[2] = {0, 0};
    if (!_readBytes(data, 2)) {
        DEBUG_PRINTLN("[SI7021] Timeout/erro ao ler bytes de temperatura");
        temperature = -999.0f;
        return false;
    }

    // Conversão conforme datasheet:
    // T = (175.72 * raw / 65536) - 46.85
    const uint16_t raw = static_cast<uint16_t>((data[0] << 8) | data[1]);
    temperature = (175.72f * static_cast<float>(raw) / 65536.0f) - 46.85f;

    // Validação básica: faixa típica do sensor
    if (temperature < -40.0f || temperature > 125.0f) {
        DEBUG_PRINTF("[SI7021] Temperatura fora da faixa esperada: %.1f°C\n", temperature);
        temperature = -999.0f;
        return false;
    }

    return true;
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
