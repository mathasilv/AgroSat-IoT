#ifndef IMU_SERVICE_H
#define IMU_SERVICE_H

#include <Arduino.h>
#include "MPU9250Hal.h"
#include "config.h"

class IMUService {
public:
    explicit IMUService(MPU9250Hal& imu);

    bool begin();            // init + calibração de mag
    void update();           // lê IMU, filtra, aplica offsets

    // Status
    bool isOnline() const    { return _online; }
    bool isCalibrated() const{ return _calibrated; }

    // Getters de dados processados
    float getGyroX()   const { return _gyroX; }
    float getGyroY()   const { return _gyroY; }
    float getGyroZ()   const { return _gyroZ; }

    float getAccelX()  const { return _accelX; }
    float getAccelY()  const { return _accelY; }
    float getAccelZ()  const { return _accelZ; }
    float getAccelMag()const { return _accelMag; }

    float getMagX()    const { return _magX; }
    float getMagY()    const { return _magY; }
    float getMagZ()    const { return _magZ; }

private:
    MPU9250Hal& _imu;

    bool  _online;
    bool  _calibrated;

    // Dados filtrados
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ, _accelMag;
    float _magX, _magY, _magZ;

    // Offsets de calibração do mag
    float _magOffsetX, _magOffsetY, _magOffsetZ;

    // Filtro de média móvel para aceleração (mesma lógica atual) [attached_file:65]
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    float _sumAccelX, _sumAccelY, _sumAccelZ;
    uint8_t _filterIndex;

    // Helpers internos
    bool  _calibrateMagnetometer();
    bool  _validateReadings(const xyzFloat& gyr,
                            const xyzFloat& acc,
                            const xyzFloat& mag);
    float _applyFilter(float value, float* buffer, float& sum);
};

#endif // IMU_SERVICE_H
