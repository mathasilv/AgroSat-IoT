/**
 * @file MPU9250.cpp - HAL → Arduino Framework (FUNCIONA!)
 */
#include "MPU9250.h"

// 🔧 DEBUG LOCAL
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// Registros (iguais ao HAL)
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
static constexpr uint8_t MPU9250_WHOAMI   = 0x71;
static constexpr uint8_t AK8963_ADDR      = 0x0C;
static constexpr uint8_t AK8963_REG_WIA   = 0x00;
static constexpr uint8_t AK8963_REG_ST1   = 0x02;
static constexpr uint8_t AK8963_REG_HXL   = 0x03;
static constexpr uint8_t AK8963_WHOAMI    = 0x48;
static constexpr uint8_t BYPASS_EN        = 0x02;
static constexpr uint8_t AK8963_MODE_CONT_100HZ_16BIT = 0x16;

MPU9250::MPU9250(uint8_t addr) 
    : _addr(addr), _magInitialized(false),
      _accelScale(1.0f/4096.0f), _gyroScale(1.0f/65.5f), _magScale(0.15f) {}

bool MPU9250::begin() {
    // Verificar WHO_AM_I
    uint8_t who = _read8(REG_WHO_AM_I);
    if (who != MPU9250_WHOAMI) {
        DEBUG_PRINTF("[MPU9250] WHO_AM_I=0x%02X (esperado 0x%02X)\n", who, MPU9250_WHOAMI);
        return false;
    }
    DEBUG_PRINTF("[MPU9250] WHO_AM_I OK: 0x%02X\n", who);

    // Reset e wake-up
    if (!_write8(REG_PWR_MGMT_1, 0x80)) return false;
    delay(100);
    if (!_write8(REG_PWR_MGMT_1, 0x01)) return false;  // PLL X gyro
    delay(10);

    // Configurações PRODUÇÃO (iguais HAL)
    _write8(REG_SMPLRT_DIV, 0x00);    // 1kHz
    _write8(REG_CONFIG, 0x06);        // DLPF ~5Hz
    _write8(REG_GYRO_CONFIG, 0x08);   // ±500 dps
    _write8(REG_ACCEL_CONFIG, 0x10);  // ±8g
    _write8(REG_ACCEL_CONFIG2, 0x03); // Accel DLPF

    delay(100);

    // Teste rápido
    xyzFloat a = _readAccelRaw();
    _online = !isnan(a.x);
    DEBUG_PRINTF("[MPU9250] Teste Accel: X=%.0f\n", a.x);
    return _online;
}

// ✅ HAL → ARDUINO: readRegister()
bool MPU9250::readRegister(uint8_t addr, uint8_t reg) {
    uint8_t val = 0xFF;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available()) val = Wire.read();
    return true;  // Sempre retorna true se comunicação OK
}

// ✅ HAL → ARDUINO: write()
bool MPU9250::write(uint8_t addr, const uint8_t* data, size_t len) {
    Wire.beginTransmission(addr);
    for (size_t i = 0; i < len; i++) {
        Wire.write(data[i]);
    }
    return Wire.endTransmission() == 0;
}

// ✅ HAL → ARDUINO: read()
bool MPU9250::read(uint8_t addr, uint8_t* data, size_t len) {
    Wire.requestFrom(addr, (uint8_t)len);
    if (Wire.available() < (int)len) return false;
    for (size_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    return true;
}

bool MPU9250::initMagnetometer() {
    // Bypass I2C
    uint8_t cfg = _read8(REG_INT_PIN_CFG);
    cfg |= BYPASS_EN;
    if (!_write8(REG_INT_PIN_CFG, cfg)) return false;
    delay(10);

    // Verificar AK8963 WHO_AM_I
    uint8_t wia = 0xFF;
    Wire.beginTransmission(AK8963_ADDR);
    Wire.write(AK8963_REG_WIA);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(AK8963_ADDR, (uint8_t)1);
        if (Wire.available()) wia = Wire.read();
    }
    
    if (wia != AK8963_WHOAMI) {
        DEBUG_PRINTF("[MPU9250] AK8963=0x%02X\n", wia);
        return false;
    }

    // Configurar AK8963
    uint8_t data[2] = {AK8963_REG_CNTL1, AK8963_MODE_CONT_100HZ_16BIT};
    Wire.beginTransmission(AK8963_ADDR);
    Wire.write(data, 2);
    if (Wire.endTransmission() != 0) return false;

    delay(20);
    _magInitialized = true;
    return true;
}

// Leituras calibradas (igual HAL)
xyzFloat MPU9250::getGValues() {
    xyzFloat raw = _readAccelRaw();
    return {raw.x * _accelScale, raw.y * _accelScale, raw.z * _accelScale};
}

xyzFloat MPU9250::getGyrValues() {
    xyzFloat raw = _readGyroRaw();
    return {raw.x * _gyroScale, raw.y * _gyroScale, raw.z * _gyroScale};
}

xyzFloat MPU9250::getMagValues() {
    if (!_magInitialized) return {NAN, NAN, NAN};
    xyzFloat raw = _readMagRaw();
    return {raw.x * _magScale, raw.y * _magScale, raw.z * _magScale};
}

// I2C baixo nível (igual HAL)
bool MPU9250::_write8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    Wire.beginTransmission(_addr);
    Wire.write(buf, 2);
    return Wire.endTransmission() == 0;
}

uint8_t MPU9250::_read8(uint8_t reg) {
    uint8_t val = 0xFF;
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return val;
    Wire.requestFrom(_addr, (uint8_t)1);
    if (Wire.available()) val = Wire.read();
    return val;
}

bool MPU9250::_readBytes(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    Wire.requestFrom(_addr, (uint8_t)len);
    if (Wire.available() < (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

xyzFloat MPU9250::_readAccelRaw() {
    uint8_t buf[6] = {0};
    if (!_readBytes(REG_ACCEL_XOUT_H, buf, 6)) return {NAN, NAN, NAN};
    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);
    return {(float)ax, (float)ay, (float)az};
}

xyzFloat MPU9250::_readGyroRaw() {
    uint8_t buf[6] = {0};
    if (!_readBytes(REG_GYRO_XOUT_H, buf, 6)) return {NAN, NAN, NAN};
    int16_t gx = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t gy = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t gz = (int16_t)((buf[4] << 8) | buf[5]);
    return {(float)gx, (float)gy, (float)gz};
}

xyzFloat MPU9250::_readMagRaw() {
    // ST1 Data Ready
    uint8_t st1 = 0;
    Wire.beginTransmission(AK8963_ADDR);
    Wire.write(AK8963_REG_ST1);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(AK8963_ADDR, (uint8_t)1);
        if (Wire.available()) st1 = Wire.read();
    }
    if (!(st1 & 0x01)) return {NAN, NAN, NAN};

    uint8_t buf[7] = {0};
    Wire.beginTransmission(AK8963_ADDR);
    Wire.write(AK8963_REG_HXL);
    if (Wire.endTransmission() != 0) return {NAN, NAN, NAN};
    Wire.requestFrom(AK8963_ADDR, (uint8_t)7);
    if (Wire.available() < 7) return {NAN, NAN, NAN};
    
    for (int i = 0; i < 7; i++) buf[i] = Wire.read();
    
    int16_t mx = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t my = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t mz = (int16_t)((buf[5] << 8) | buf[4]);
    
    return {(float)mx, (float)my, (float)mz};
}
