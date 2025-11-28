#include "MPU9250Hal.h"

// Registradores principais do MPU9250 (acc+gyro) [web:257]
static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
static constexpr uint8_t REG_CONFIG       = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_ACCEL_CONFIG2= 0x1D;
static constexpr uint8_t REG_INT_PIN_CFG  = 0x37;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
static constexpr uint8_t REG_GYRO_XOUT_H  = 0x43;
static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
static constexpr uint8_t REG_WHO_AM_I     = 0x75;

// WHO_AM_I esperado = 0x71 para MPU9250 [web:262]
static constexpr uint8_t MPU9250_WHOAMI   = 0x71;

// Magnetômetro AK8963 (interno ao MPU9250) [web:257][web:263]
static constexpr uint8_t AK8963_ADDR      = 0x0C;
static constexpr uint8_t AK8963_REG_WIA   = 0x00;
static constexpr uint8_t AK8963_REG_ST1   = 0x02;
static constexpr uint8_t AK8963_REG_HXL   = 0x03;
static constexpr uint8_t AK8963_REG_ST2   = 0x09;
static constexpr uint8_t AK8963_REG_CNTL1 = 0x0A;

// WHO_AM_I do AK8963 = 0x48 [web:263]
static constexpr uint8_t AK8963_WHOAMI    = 0x48;

// INT_PIN_CFG: habilitar bypass I2C para falar direto com AK8963 [web:257][web:263]
static constexpr uint8_t BYPASS_EN        = 0x02;

// CNTL1: modo contínuo 100 Hz, 16-bit (0x16) [web:257]
static constexpr uint8_t AK8963_MODE_CONT_100HZ_16BIT = 0x16;

MPU9250Hal::MPU9250Hal(HAL::I2C& i2c, uint8_t addr)
    : _i2c(&i2c),
      _addr(addr),
      _magInitialized(false),
      _accelScale(1.0f/4096.0f),    // ±8g -> 4096 LSB/g [web:257]
      _gyroScale(1.0f/65.5f),       // ±500 dps -> 65.5 LSB/(°/s)
      _magScale(0.15f)             // aprox. 0.15 µT/LSB para 16-bit AK8963 [web:257]
{
}

bool MPU9250Hal::begin() {
    if (!_i2c) return false;

    // Verificar WHO_AM_I
    uint8_t who = _read8(REG_WHO_AM_I);
    if (who != MPU9250_WHOAMI) {
        DEBUG_PRINTF("[MPU9250Hal] WHO_AM_I=0x%02X (esperado 0x%02X)\n",
                     who, MPU9250_WHOAMI);
        return false;
    }

    // Reset e wake-up [web:257]
    if (!_write8(REG_PWR_MGMT_1, 0x80)) { // reset
        return false;
    }
    delay(100);
    // Clock source: PLL X axis gyro, tirar do sleep
    if (!_write8(REG_PWR_MGMT_1, 0x01)) {
        return false;
    }
    delay(10);

    // Sample rate: 1 kHz (SMPLRT_DIV = 0) com DLPF ligado [web:257]
    _write8(REG_SMPLRT_DIV, 0x00);

    // Filtro DLPF para gyro (CONFIG), por exemplo DLPF=6 (~5 Hz) [web:257]
    _write8(REG_CONFIG, 0x06);

    // Gyro ±500 dps (bits [4:3] = 01) [web:257]
    _write8(REG_GYRO_CONFIG, 0x08);

    // Accel ±8g (bits [4:3] = 10) [web:257]
    _write8(REG_ACCEL_CONFIG, 0x10);

    // Accel DLPF ~41 Hz (ACCEL_CONFIG2, DLPF_CFG=3) [web:257]
    _write8(REG_ACCEL_CONFIG2, 0x03);

    delay(100);

    // Teste rápido de leitura
    xyzFloat a = _readAccelRaw();
    return !isnan(a.x);
}

bool MPU9250Hal::initMagnetometer() {
    if (!_i2c) return false;

    // Habilitar bypass I2C para falar com AK8963 no barramento principal [web:257][web:263]
    uint8_t cfg = _read8(REG_INT_PIN_CFG);
    cfg |= BYPASS_EN;
    if (!_write8(REG_INT_PIN_CFG, cfg)) {
        return false;
    }
    delay(10);

    // Verificar WHO_AM_I do AK8963
    uint8_t wia = _i2c->readRegister(AK8963_ADDR, AK8963_REG_WIA);
    if (wia != AK8963_WHOAMI) {
        DEBUG_PRINTF("[MPU9250Hal] AK8963 WHO_AM_I=0x%02X (esperado 0x%02X)\n",
                     wia, AK8963_WHOAMI);
        return false;
    }

    // Configurar modo contínuo 100 Hz, 16-bit [web:257]
    uint8_t data[2] = {AK8963_REG_CNTL1, AK8963_MODE_CONT_100HZ_16BIT};
    if (!_i2c->write(AK8963_ADDR, data, 2)) {
        DEBUG_PRINTLN("[MPU9250Hal] Falha ao configurar AK8963 CNTL1");
        return false;
    }

    delay(20);
    _magInitialized = true;
    return true;
}

xyzFloat MPU9250Hal::getGValues() {
    xyzFloat raw = _readAccelRaw();
    xyzFloat r;
    r.x = raw.x * _accelScale;
    r.y = raw.y * _accelScale;
    r.z = raw.z * _accelScale;
    return r;
}

xyzFloat MPU9250Hal::getGyrValues() {
    xyzFloat raw = _readGyroRaw();
    xyzFloat r;
    r.x = raw.x * _gyroScale;
    r.y = raw.y * _gyroScale;
    r.z = raw.z * _gyroScale;
    return r;
}

xyzFloat MPU9250Hal::getMagValues() {
    if (!_magInitialized) {
        xyzFloat z = {NAN, NAN, NAN};
        return z;
    }
    xyzFloat raw = _readMagRaw();
    xyzFloat r;
    r.x = raw.x * _magScale;
    r.y = raw.y * _magScale;
    r.z = raw.z * _magScale;
    return r;
}

// ==================== Internos ====================

bool MPU9250Hal::_write8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return _i2c && _i2c->write(_addr, buf, 2);
}

uint8_t MPU9250Hal::_read8(uint8_t reg) {
    uint8_t val = 0xFF;
    if (!_i2c) return val;
    if (!_i2c->write(_addr, &reg, 1)) return val;
    _i2c->read(_addr, &val, 1);
    return val;
}

bool MPU9250Hal::_readBytes(uint8_t reg, uint8_t* buf, size_t len) {
    if (!_i2c) return false;
    if (!_i2c->write(_addr, &reg, 1)) return false;
    if (!_i2c->read(_addr, buf, len)) return false;
    return true;
}

xyzFloat MPU9250Hal::_readAccelRaw() {
    uint8_t buf[6] = {0};
    xyzFloat r = {NAN, NAN, NAN};

    if (!_readBytes(REG_ACCEL_XOUT_H, buf, sizeof(buf))) {
        return r;
    }

    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);

    r.x = (float)ax;
    r.y = (float)ay;
    r.z = (float)az;
    return r;
}

xyzFloat MPU9250Hal::_readGyroRaw() {
    uint8_t buf[6] = {0};
    xyzFloat r = {NAN, NAN, NAN};

    if (!_readBytes(REG_GYRO_XOUT_H, buf, sizeof(buf))) {
        return r;
    }

    int16_t gx = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t gy = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t gz = (int16_t)((buf[4] << 8) | buf[5]);

    r.x = (float)gx;
    r.y = (float)gy;
    r.z = (float)gz;
    return r;
}

xyzFloat MPU9250Hal::_readMagRaw() {
    xyzFloat r = {NAN, NAN, NAN};

    // Ler ST1 para checar DRDY [web:257]
    uint8_t st1 = _i2c->readRegister(AK8963_ADDR, AK8963_REG_ST1);
    if (!(st1 & 0x01)) {
        return r; // dado não pronto
    }

    uint8_t buf[7] = {0};
    if (!_i2c->write(AK8963_ADDR, (uint8_t*) &AK8963_REG_HXL, 1)) {
        return r;
    }
    if (!_i2c->read(AK8963_ADDR, buf, sizeof(buf))) {
        return r;
    }

    // Dados little-endian: HXL, HXH, HYL, HYH, HZL, HZH, ST2 [web:257]
    int16_t mx = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t my = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t mz = (int16_t)((buf[5] << 8) | buf[4]);

    r.x = (float)mx;
    r.y = (float)my;
    r.z = (float)mz;
    return r;
}
