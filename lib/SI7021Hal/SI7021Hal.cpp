#include "SI7021Hal.h"

// Comandos principais (datasheet Si7021) [web:107][web:113]
static constexpr uint8_t SI7021_CMD_MEASURE_RH_NOHOLD = 0xF5;
static constexpr uint8_t SI7021_CMD_MEASURE_T_NOHOLD  = 0xF3;
static constexpr uint8_t SI7021_CMD_SOFT_RESET        = 0xFE;
static constexpr uint8_t SI7021_CMD_WRITE_USER_REG    = 0xE6;
static constexpr uint8_t SI7021_CMD_READ_USER_REG     = 0xE7;

SI7021Hal::SI7021Hal(HAL::I2C& i2c)
    : _i2c(&i2c),
      _addr(SI7021_ADDRESS),
      _initialized(false)
{
}

bool SI7021Hal::begin(uint8_t addr) {
    if (!_i2c) return false;
    _addr = addr;

    // Soft reset
    if (!_writeCommand(SI7021_CMD_SOFT_RESET)) {
        DEBUG_PRINTLN("[SI7021Hal] Falha no soft reset");
        return false;
    }
    delay(50);

    // Configurar User Register: resolução padrão
    uint8_t cfg[2] = {SI7021_CMD_WRITE_USER_REG, 0x00};
    if (!_i2c->write(_addr, cfg, 2)) {
        DEBUG_PRINTLN("[SI7021Hal] Falha ao escrever User Register");
        return false;
    }
    delay(20);

    // Teste opcional de leitura (não derruba o init se falhar)
    float humTest = NAN;
    if (readHumidity(humTest)) {
        DEBUG_PRINTF("[SI7021Hal] Umidade inicial = %.1f%%\n", humTest);
    } else {
        DEBUG_PRINTLN("[SI7021Hal] Aviso: não conseguiu ler umidade no teste inicial");
    }

    _initialized = true;
    return true;
}

bool SI7021Hal::readHumidity(float& humidity) {
    if (!_initialized || !_i2c) return false;

    // Enviar comando de medição de umidade (no hold master)
    if (!_writeCommand(SI7021_CMD_MEASURE_RH_NOHOLD)) {
        return false;
    }

    // Datasheet: até ~23 ms para RH+Temp; usar margem [web:107][web:113]
    const uint8_t MAX_RETRIES = 5;
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        delay(25);  // 25 ms por tentativa

        uint8_t buf[3] = {0};
        if (!_readBytes(buf, sizeof(buf))) {
            continue; // tenta de novo
        }

        uint16_t rawHum = (buf[0] << 8) | buf[1];
        if (rawHum == 0xFFFF || rawHum == 0x0000) {
            continue;
        }

        float hum = ((125.0f * rawHum) / 65536.0f) - 6.0f; // datasheet

        // Apenas retorna sucesso; quem valida faixa é o SensorManager
        humidity = hum;
        return true;
    }

    return false;
}

bool SI7021Hal::readTemperature(float& temperature) {
    if (!_initialized || !_i2c) return false;

    // Medição de temperatura sem hold (0xF3) [web:113]
    if (!_writeCommand(SI7021_CMD_MEASURE_T_NOHOLD)) {
        return false;
    }

    // Tempo típico máx ~11 ms; usar margem
    delay(80);

    uint8_t buf[2] = {0};
    if (!_readBytes(buf, sizeof(buf))) {
        return false;
    }

    uint16_t rawTemp = (buf[0] << 8) | buf[1];

    if (rawTemp == 0xFFFF || rawTemp == 0x0000) {
        return false;
    }

    // Fórmula do datasheet: T = ((175.72 * raw) / 65536) - 46.85 [web:113]
    float temp = ((175.72f * rawTemp) / 65536.0f) - 46.85f;

    temperature = temp;
    return true;
}

bool SI7021Hal::_writeCommand(uint8_t cmd) {
    if (!_i2c) return false;
    uint8_t data[1] = {cmd};
    return _i2c->write(_addr, data, 1);
}

bool SI7021Hal::_readBytes(uint8_t* buf, size_t len) {
    if (!_i2c) return false;
    return _i2c->read(_addr, buf, len);
}
