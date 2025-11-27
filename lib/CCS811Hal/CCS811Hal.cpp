#include "CCS811Hal.h"

// Registradores/endereços principais (Programming Guide) [web:108][web:202]
static constexpr uint8_t CCS811_REG_STATUS        = 0x00;
static constexpr uint8_t CCS811_REG_MEAS_MODE     = 0x01;
static constexpr uint8_t CCS811_REG_ALG_RESULT    = 0x02;
static constexpr uint8_t CCS811_REG_ENV_DATA      = 0x05;
static constexpr uint8_t CCS811_REG_HW_ID         = 0x20;
static constexpr uint8_t CCS811_REG_APP_START     = 0xF4;

// Constantes de identificação e modo [web:108][web:205]
static constexpr uint8_t CCS811_HW_ID_EXPECTED    = 0x81;
static constexpr uint8_t CCS811_MEAS_MODE_1SEC    = 0x10; // drive mode = 1s (bits [4:2])

// Bits de STATUS [web:108]
static constexpr uint8_t CCS811_STATUS_ERROR      = 0x01;
static constexpr uint8_t CCS811_STATUS_DATA_READY = 0x08;
static constexpr uint8_t CCS811_STATUS_APP_VALID  = 0x10;
static constexpr uint8_t CCS811_STATUS_FW_MODE    = 0x80;

CCS811Hal::CCS811Hal(HAL::I2C& i2c)
    : _i2c(&i2c),
      _addr(CCS811_ADDR_1),
      _initialized(false),
      _eco2(0),
      _tvoc(0)
{
}

bool CCS811Hal::begin(uint8_t addr) {
    if (!_i2c) return false;
    _addr = addr;

    // Verificar HW_ID (deve ser 0x81) [web:205]
    uint8_t hwid = _i2c->readRegister(_addr, CCS811_REG_HW_ID);
    if (hwid != CCS811_HW_ID_EXPECTED) {
        DEBUG_PRINTF("[CCS811Hal] HW_ID inesperado: 0x%02X (esperado 0x%02X)\n",
                     hwid, CCS811_HW_ID_EXPECTED);
        return false;
    }

    // Verificar se há aplicação válida [web:108]
    uint8_t status = _i2c->readRegister(_addr, CCS811_REG_STATUS);
    if (!(status & CCS811_STATUS_APP_VALID)) {
        DEBUG_PRINTLN("[CCS811Hal] APP_VALID=0, firmware de aplicação ausente");
        return false;
    }

    // APP_START: escrever 0xF4 sem dados para sair de BOOT e entrar em APP mode [web:108][web:205]
    if (!_writeCommand(CCS811_REG_APP_START)) {
        DEBUG_PRINTLN("[CCS811Hal] Falha ao enviar APP_START");
        return false;
    }
    delay(100);

    // Confirmar que FW_MODE está setado [web:108]
    status = _i2c->readRegister(_addr, CCS811_REG_STATUS);
    if (!(status & CCS811_STATUS_FW_MODE)) {
        DEBUG_PRINTF("[CCS811Hal] FW_MODE=0 após APP_START (STATUS=0x%02X)\n", status);
        return false;
    }

    // Configurar modo de medição: 1 leitura por segundo [web:108][web:202]
    if (!_write8(CCS811_REG_MEAS_MODE, CCS811_MEAS_MODE_1SEC)) {
        DEBUG_PRINTLN("[CCS811Hal] Falha ao escrever MEAS_MODE");
        return false;
    }

    _initialized = true;
    return true;
}

bool CCS811Hal::available() {
    if (!_initialized || !_i2c) return false;
    uint8_t status = _i2c->readRegister(_addr, CCS811_REG_STATUS);
    return (status & CCS811_STATUS_DATA_READY) != 0;
}

uint8_t CCS811Hal::readData() {
    if (!_initialized || !_i2c) return 1;

    uint8_t buf[8] = {0};
    if (!_read(CCS811_REG_ALG_RESULT, buf, sizeof(buf))) {
        return 1;
    }

    // Formato ALG_RESULT_DATA: eCO2(2) TVOC(2) STATUS(1) ERROR_ID(1) RAW_DATA(2) [web:108][web:252]
    uint16_t eco2     = (uint16_t(buf[0]) << 8) | buf[1];
    uint16_t tvoc     = (uint16_t(buf[2]) << 8) | buf[3];
    uint8_t  status   = buf[4];
    uint8_t  error_id = buf[5];

    if (status & CCS811_STATUS_ERROR) {
        DEBUG_PRINTF("[CCS811Hal] Erro reportado, ERROR_ID=0x%02X\n", error_id);
        return 2;
    }

    _eco2 = eco2;
    _tvoc = tvoc;
    return 0;
}

void CCS811Hal::setEnvironmentalData(float humidity, float temperature) {
    if (!_initialized || !_i2c) return;

    // Limitar faixa física
    if (humidity < 0.0f)   humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    // Conversão para formato Q9.7 do registrador ENV_DATA [web:245][web:246]
    // Humidade em passos de 1/512 %RH
    uint16_t hum_reg = (uint16_t)(humidity * 512.0f + 0.5f);
    // Temperatura em passos de 1/512 °C com offset +25°C
    uint16_t temp_reg = (uint16_t)((temperature + 25.0f) * 512.0f + 0.5f);

    uint8_t buf[4];
    buf[0] = uint8_t(hum_reg >> 8);
    buf[1] = uint8_t(hum_reg & 0xFF);
    buf[2] = uint8_t(temp_reg >> 8);
    buf[3] = uint8_t(temp_reg & 0xFF);

    if (!_write(CCS811_REG_ENV_DATA, buf, sizeof(buf))) {
        DEBUG_PRINTLN("[CCS811Hal] Falha ao escrever ENV_DATA");
    }
}

// ==================== Helpers ====================

bool CCS811Hal::_write8(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return _i2c && _i2c->write(_addr, data, 2);
}

bool CCS811Hal::_write(uint8_t reg, const uint8_t* data, size_t len) {
    if (!_i2c) return false;
    uint8_t buf[1 + 4]; // suficiente para ENV_DATA (4 bytes)
    if (len > 4) return false;
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return _i2c->write(_addr, buf, 1 + len);
}

bool CCS811Hal::_read(uint8_t reg, uint8_t* data, size_t len) {
    if (!_i2c) return false;
    if (!_i2c->write(_addr, &reg, 1)) return false;
    if (!_i2c->read(_addr, data, len)) return false;
    return true;
}

bool CCS811Hal::_writeCommand(uint8_t cmd) {
    if (!_i2c) return false;
    uint8_t c = cmd;
    return _i2c->write(_addr, &c, 1);
}
