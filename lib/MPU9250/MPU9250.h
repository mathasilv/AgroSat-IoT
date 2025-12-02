/**
 * @file MPU9250.h
 * @brief Driver Nativo MPU9250 (9-DOF IMU)
 * @details Suporta Acelerômetro, Giroscópio e Magnetômetro (AK8963) via I2C Bypass.
 */

#ifndef MPU9250_H
#define MPU9250_H

#include <Arduino.h>
#include <Wire.h>

struct xyzFloat {
    float x, y, z;
    xyzFloat() : x(0), y(0), z(0) {}
    xyzFloat(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

class MPU9250 {
public:
    static constexpr uint8_t I2C_ADDR = 0x69;
    static constexpr uint8_t AK8963_ADDR = 0x0C;

    explicit MPU9250(uint8_t addr = I2C_ADDR);

    // Inicialização
    bool begin();
    bool initMagnetometer();
    
    // Configurações
    void enableI2CBypass(bool enable);
    void setAccRange(uint8_t range); // 0=2g, 1=4g, 2=8g, 3=16g
    void setGyrRange(uint8_t range); // 0=250, 1=500, 2=1000, 3=2000
    
    // Leituras (Dados calibrados com escala)
    xyzFloat getGValues();      // Acelerômetro (g)
    xyzFloat getGyrValues();    // Giroscópio (°/s)
    xyzFloat getMagValues();    // Magnetômetro (µT) - Retorna (0,0,0) se inválido

    // Raw Reads (para diagnósticos)
    int16_t readRawAccelX();
    
private:
    uint8_t _addr;
    float _accScale;
    float _gyroScale;
    float _magScale;
    bool _magInitialized;

    // Registradores MPU9250
    static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
    static constexpr uint8_t REG_CONFIG       = 0x1A;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_INT_PIN_CFG  = 0x37;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static constexpr uint8_t REG_GYRO_XOUT_H  = 0x43;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;

    // Registradores AK8963
    static constexpr uint8_t AK8963_WIA       = 0x00;
    static constexpr uint8_t AK8963_ST1       = 0x02;
    static constexpr uint8_t AK8963_HXL       = 0x03;
    static constexpr uint8_t AK8963_CNTL1     = 0x0A;
    static constexpr uint8_t AK8963_ASAX      = 0x10;

    // Helpers I2C
    bool _writeRegister(uint8_t addr, uint8_t reg, uint8_t data);
    uint8_t _readRegister(uint8_t addr, uint8_t reg);
    bool _readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len);
    
    void _updateScales(uint8_t accRange, uint8_t gyrRange);
};

#endif