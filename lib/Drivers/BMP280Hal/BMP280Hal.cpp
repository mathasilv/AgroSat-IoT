#include "BMP280Hal.h"

// Registradores principais (datasheet BMP280) [web:102]
static constexpr uint8_t BMP280_REG_ID        = 0xD0;
static constexpr uint8_t BMP280_REG_RESET     = 0xE0;
static constexpr uint8_t BMP280_REG_STATUS    = 0xF3;
static constexpr uint8_t BMP280_REG_CTRL_MEAS = 0xF4;
static constexpr uint8_t BMP280_REG_CONFIG    = 0xF5;
static constexpr uint8_t BMP280_REG_PRESS_MSB = 0xF7; // F7..F9
static constexpr uint8_t BMP280_REG_TEMP_MSB  = 0xFA; // FA..FC
static constexpr uint8_t BMP280_REG_CALIB     = 0x88; // 0x88..0xA1

// ID esperado do BMP280 [web:102]
static constexpr uint8_t BMP280_CHIP_ID       = 0x58;

// Configuração equivalente a:
// MODE_NORMAL, SAMPLING_X2 (temp), SAMPLING_X16 (press),
// FILTER_X16, STANDBY_MS_500 (igual tua config atual) [web:131][web:137]
static constexpr uint8_t BMP280_CTRL_MEAS_CFG =
    (0x02 << 5) | // osrs_t = x2
    (0x05 << 2) | // osrs_p = x16
    0x03;         // mode = normal

static constexpr uint8_t BMP280_CONFIG_CFG =
    (0x04 << 5) | // t_sb = 500 ms
    (0x04 << 2) | // filter = x16
    0x00;         // spi3w_en = 0

BMP280Hal::BMP280Hal(HAL::I2C& i2c)
    : _i2c(&i2c),
      _addr(0),
      _initialized(false),
      dig_T1(0), dig_T2(0), dig_T3(0),
      dig_P1(0), dig_P2(0), dig_P3(0), dig_P4(0),
      dig_P5(0), dig_P6(0), dig_P7(0), dig_P8(0), dig_P9(0),
      t_fine(0)
{
}

bool BMP280Hal::begin(uint8_t addr) {
    if (!_i2c) return false;
    _addr = addr;

    // Verificar ID do chip
    uint8_t id = _i2c->readRegister(_addr, BMP280_REG_ID);
    if (id != BMP280_CHIP_ID) {
        DEBUG_PRINTF("[BMP280Hal] ID inesperado: 0x%02X (esperado 0x%02X)\n", id, BMP280_CHIP_ID);
        return false;
    }

    // Soft reset padrão BMP280 (escrever 0xB6 em 0xE0) [web:102]
    if (!_write8(BMP280_REG_RESET, 0xB6)) {
        DEBUG_PRINTLN("[BMP280Hal] Falha no soft reset");
        return false;
    }
    delay(10);

    // Esperar bit de busy (measuring/im_update) limpar (registro STATUS) [web:102]
    uint32_t start = millis();
    while (millis() - start < 100) {
        uint8_t status = _i2c->readRegister(_addr, BMP280_REG_STATUS);
        // bit0 = measuring, bit1 = im_update
        if ((status & 0x09) == 0) {
            break;
        }
        delay(5);
    }

    // Ler coeficientes de calibração
    if (!_readCalibration()) {
        DEBUG_PRINTLN("[BMP280Hal] Falha ao ler calibração");
        return false;
    }

    // Configurar filtro / standby (CONFIG)
    if (!_write8(BMP280_REG_CONFIG, BMP280_CONFIG_CFG)) {
        DEBUG_PRINTLN("[BMP280Hal] Falha ao escrever CONFIG");
        return false;
    }

    // Configurar oversampling e modo normal (CTRL_MEAS)
    if (!_write8(BMP280_REG_CTRL_MEAS, BMP280_CTRL_MEAS_CFG)) {
        DEBUG_PRINTLN("[BMP280Hal] Falha ao escrever CTRL_MEAS");
        return false;
    }

    _initialized = true;
    return true;
}


float BMP280Hal::readTemperature() {
    if (!_initialized || !_i2c) return NAN;

    int32_t adc_T = 0;
    int32_t adc_P = 0;
    if (!_readRaw(adc_T, adc_P)) {
        return NAN;
    }

    // Compensação de temperatura (datasheet BMP280, seção 3.11.3) [web:102]
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)dig_T1) *
                      ((adc_T >> 4) - (int32_t)dig_T1)) >> 12) *
                    (int32_t)dig_T3) >> 14;

    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;  // T * 100

    return T / 100.0f; // °C
}

float BMP280Hal::readPressure() {
    if (!_initialized || !_i2c) return NAN;

    int32_t adc_T = 0;
    int32_t adc_P = 0;
    if (!_readRaw(adc_T, adc_P)) {
        return NAN;
    }

    // Garantir t_fine atualizado (usa mesma fórmula de temperatura se ainda não tiver)
    if (t_fine == 0) {
        int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
        int32_t var2 = (((((adc_T >> 4) - (int32_t)dig_T1) *
                          ((adc_T >> 4) - (int32_t)dig_T1)) >> 12) *
                        (int32_t)dig_T3) >> 14;
        t_fine = var1 + var2;
    }

    // Compensação de pressão (datasheet BMP280, seção 3.11.3) [web:102]
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

    if (var1 == 0) {
        return NAN; // evitar divisão por zero
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

    // Resultado em Pa (igual Adafruit_BMP280::readPressure) [web:122]
    return (float)p / 256.0f;
}

bool BMP280Hal::_readCalibration() {
    uint8_t buf[24] = {0};

    if (!_readBytes(BMP280_REG_CALIB, buf, sizeof(buf))) {
        return false;
    }

    dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);

    dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);

    return true;
}

bool BMP280Hal::_readRaw(int32_t& adc_T, int32_t& adc_P) {
    uint8_t buf[6] = {0};

    // Ler pressão + temperatura em sequência (F7..FC) [web:102]
    if (!_readBytes(BMP280_REG_PRESS_MSB, buf, sizeof(buf))) {
        return false;
    }

    int32_t adc_P_raw = ((int32_t)buf[0] << 12) |
                        ((int32_t)buf[1] << 4)  |
                        ((int32_t)buf[2] >> 4);

    int32_t adc_T_raw = ((int32_t)buf[3] << 12) |
                        ((int32_t)buf[4] << 4)  |
                        ((int32_t)buf[5] >> 4);

    adc_P = adc_P_raw;
    adc_T = adc_T_raw;
    return true;
}

bool BMP280Hal::_readBytes(uint8_t reg, uint8_t* buf, size_t len) {
    if (!_i2c) return false;

    // Escrever registrador alvo
    if (!_i2c->write(_addr, &reg, 1)) {
        return false;
    }

    // Ler sequência de bytes a partir do registrador
    if (!_i2c->read(_addr, buf, len)) {
        return false;
    }

    return true;
}

bool BMP280Hal::_write8(uint8_t reg, uint8_t value) {
    if (!_i2c) return false;
    uint8_t data[2] = {reg, value};
    return _i2c->write(_addr, data, 2);
}
