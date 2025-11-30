/**
 * @file MPU9250.h - AgroSat-IoT CubeSat (TTGO-LoRa32-v21)
 * @version 1.0.3 - HAL → Arduino Framework (100% FUNCIONANDO)
 */
#ifndef MPU9250_H
#define MPU9250_H

#include <Arduino.h>
#include <Wire.h>
// REMOVIDO: "config.h" - 100% independente

struct xyzFloat {
    float x, y, z;
    xyzFloat() : x(NAN), y(NAN), z(NAN) {}
    xyzFloat(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

class MPU9250 {
public:
    MPU9250(uint8_t addr = 0x69);  // ✅ 0x69 hardcoded (TTGO LoRa32)
    
    bool begin();
    bool initMagnetometer();
    
    // Leituras calibradas
    xyzFloat getGValues();      // Acelerômetro (g)
    xyzFloat getGyrValues();    // Giroscópio (°/s)
    xyzFloat getMagValues();    // Magnetômetro (µT)
    
    bool isOnline() const { return _online; }
    bool isMagOnline() const { return _magInitialized; }
    
    // ✅ MÉTODOS HAL (para compatibilidade)
    bool readRegister(uint8_t addr, uint8_t reg);
    bool write(uint8_t addr, const uint8_t* data, size_t len);
    bool read(uint8_t addr, uint8_t* data, size_t len);
    
    // Configurações MPU9250
    void setAccRange(uint8_t range);   // 0=±2g, 1=±4g, 2=±8g, 3=±16g
    void setGyrRange(uint8_t range);   // 0=±250, 1=±500, 2=±1000, 3=±2000
    void setGyrDLPF(uint8_t bw);       // 0-6 (260Hz a 5Hz)
    void enableGyrDLPF(bool enable = true);
    
    void printStatus() const;
    
private:
    uint8_t _addr;
    bool _online;
    bool _magInitialized;
    float _accelScale;
    float _gyroScale;
    float _magScale;
    
    // Registros MPU9250 (hardcoded - PRODUÇÃO)
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
    static constexpr uint8_t REG_CONFIG       = 0x1A;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_ACCEL_CONFIG2= 0x1D;
    static constexpr uint8_t REG_INT_PIN_CFG  = 0x37;
    static constexpr uint8_t REG_INT_ENABLE   = 0x38;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static constexpr uint8_t REG_GYRO_XOUT_H  = 0x43;
    
    static constexpr uint8_t MPU9250_WHOAMI   = 0x71;
    
    // AK8963 Magnetômetro
    static constexpr uint8_t AK8963_ADDR           = 0x0C;
    static constexpr uint8_t AK8963_REG_WIA        = 0x00;
    static constexpr uint8_t AK8963_REG_ST1        = 0x02;
    static constexpr uint8_t AK8963_REG_HXL        = 0x03;
    static constexpr uint8_t AK8963_REG_ST2        = 0x09;
    static constexpr uint8_t AK8963_REG_CNTL1      = 0x0A;
    static constexpr uint8_t AK8963_REG_CNTL2      = 0x0B;
    static constexpr uint8_t AK8963_WHOAMI         = 0x48;
    static constexpr uint8_t BYPASS_EN             = 0x02;
    static constexpr uint8_t AK8963_MODE_CONT_100HZ_16BIT = 0x16;
    
    // Métodos privados
    bool _write8(uint8_t reg, uint8_t value);
    uint8_t _read8(uint8_t reg);
    bool _readBytes(uint8_t reg, uint8_t* buf, size_t len);
    xyzFloat _readAccelRaw();
    xyzFloat _readGyroRaw();
    xyzFloat _readMagRaw();
    void _updateScales();
};

#endif // MPU9250_H
