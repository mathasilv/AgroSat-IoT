#ifndef MPU9250_HAL_H
#define MPU9250_HAL_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "config.h"

// Vetor 3D compatível com xyzFloat usado antes
struct xyzFloat {
    float x;
    float y;
    float z;
};

// Driver MPU9250 100% em cima de HAL::I2C (sem Wire / MPU9250_WE)
// - Acelerômetro, giroscópio e magnetômetro (AK8963 interno) [web:257][web:258]
class MPU9250Hal {
public:
    MPU9250Hal(HAL::I2C& i2c, uint8_t addr = MPU9250_ADDRESS);

    bool begin();               // Inicializa IMU (acc+gyro)
    bool initMagnetometer();    // Configura AK8963 em modo contínuo

    xyzFloat getGValues();      // aceleração em g
    xyzFloat getGyrValues();    // giroscópio em °/s
    xyzFloat getMagValues();    // magnetômetro em µT

private:
    HAL::I2C* _i2c;
    uint8_t   _addr;
    bool      _magInitialized;

    // Conversão de escala
    float _accelScale;          // g/LSB
    float _gyroScale;           // °/s por LSB
    float _magScale;            // µT/LSB

    // Helpers de registrador
    bool _write8(uint8_t reg, uint8_t value);
    bool _readBytes(uint8_t reg, uint8_t* buf, size_t len);
    uint8_t _read8(uint8_t reg);

    // Leitura crua
    xyzFloat _readAccelRaw();
    xyzFloat _readGyroRaw();
    xyzFloat _readMagRaw();
};

#endif // MPU9250_HAL_H
