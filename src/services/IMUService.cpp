#include "IMUService.h"
#include "DisplayManager.h"   // para feedback de calibração

// Ponteiro global já existente no teu código
extern DisplayManager* g_displayManagerPtr;

IMUService::IMUService(MPU9250Hal& imu)
    : _imu(imu),
      _online(false),
      _calibrated(false),
      _gyroX(0.0f), _gyroY(0.0f), _gyroZ(0.0f),
      _accelX(0.0f), _accelY(0.0f), _accelZ(0.0f), _accelMag(0.0f),
      _magX(0.0f), _magY(0.0f), _magZ(0.0f),
      _magOffsetX(0.0f), _magOffsetY(0.0f), _magOffsetZ(0.0f),
      _sumAccelX(0.0f), _sumAccelY(0.0f), _sumAccelZ(0.0f),
      _filterIndex(0)
{
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0f;
        _accelYBuffer[i] = 0.0f;
        _accelZBuffer[i] = 0.0f;
    }
}

bool IMUService::begin() {
    if (!_imu.begin()) {
        DEBUG_PRINTLN("[IMUService] Falha ao inicializar MPU9250");
        _online = false;
        return false;
    }

    if (!_imu.initMagnetometer()) {
        DEBUG_PRINTLN("[IMUService] Magnetometro falhou");
        _online = true;     // ainda dá pra usar acc+gyro
        _calibrated = false;
        return true;
    }

    if (_calibrateMagnetometer()) {
        _calibrated = true;
    } else {
        _magOffsetX = _magOffsetY = _magOffsetZ = 0.0f;
        _calibrated = false;
    }

    _online = true;
    return true;
}

void IMUService::update() {
    if (!_online) return;

    xyzFloat acc = _imu.getGValues();
    xyzFloat gyr = _imu.getGyrValues();
    xyzFloat mag = _imu.getMagValues();

    if (!_validateReadings(gyr, acc, mag)) {
        return;
    }

    // Filtro de média móvel (mesmo do SensorManager) [attached_file:65]
    _accelX = _applyFilter(acc.x, _accelXBuffer, _sumAccelX);
    _accelY = _applyFilter(acc.y, _accelYBuffer, _sumAccelY);
    _accelZ = _applyFilter(acc.z, _accelZBuffer, _sumAccelZ);

    _gyroX = gyr.x;
    _gyroY = gyr.y;
    _gyroZ = gyr.z;

    // Aplicar offsets do magnetômetro
    _magX = mag.x - _magOffsetX;
    _magY = mag.y - _magOffsetY;
    _magZ = mag.z - _magOffsetZ;

    _accelMag = sqrtf(_accelX*_accelX + _accelY*_accelY + _accelZ*_accelZ);
}

bool IMUService::_calibrateMagnetometer() {
    DEBUG_PRINTLN("[IMUService] Magnetometro OK, iniciando calibração...");

    float magMin[3] = {9999.0f, 9999.0f, 9999.0f};
    float magMax[3] = {-9999.0f, -9999.0f, -9999.0f};

    DEBUG_PRINTLN("[IMUService] Rotacione o CubeSat lentamente em todos os eixos...");

    uint32_t startTime         = millis();
    uint16_t samples           = 0;
    const uint32_t calibTimeMs = 10000;  // 10 s

    while (millis() - startTime < calibTimeMs) {
        xyzFloat mag = _imu.getMagValues();

        if (!isnan(mag.x) && !isnan(mag.y) && !isnan(mag.z)) {
            magMin[0] = min(magMin[0], mag.x);
            magMin[1] = min(magMin[1], mag.y);
            magMin[2] = min(magMin[2], mag.z);

            magMax[0] = max(magMax[0], mag.x);
            magMax[1] = max(magMax[1], mag.y);
            magMax[2] = max(magMax[2], mag.z);

            samples++;
        }

        static uint32_t lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate >= 100) {
            lastDisplayUpdate = millis();

            uint8_t progress =
                ((millis() - startTime) * 100) / calibTimeMs;

            if (g_displayManagerPtr && g_displayManagerPtr->isOn()) {
                g_displayManagerPtr->showCalibration(progress);
            }

            if ((millis() - startTime) % 2000 < 100) {
                DEBUG_PRINTF("[IMUService] Calibrando... %lus / 10s (%d samples)\n",
                             (millis() - startTime) / 1000, samples);
            }
        }

        delay(50);
    }

    if (samples <= 100) {
        DEBUG_PRINTLN("[IMUService] Calibração insuficiente, usando offsets zero");
        return false;
    }

    _magOffsetX = (magMax[0] + magMin[0]) / 2.0f;
    _magOffsetY = (magMax[1] + magMin[1]) / 2.0f;
    _magOffsetZ = (magMax[2] + magMin[2]) / 2.0f;

    DEBUG_PRINTLN("[IMUService] Magnetometro calibrado!");
    DEBUG_PRINTF("[IMUService] Offsets: X=%.2f Y=%.2f Z=%.2f µT\n",
                 _magOffsetX, _magOffsetY, _magOffsetZ);
    DEBUG_PRINTF("[IMUService] Samples coletados: %d\n", samples);

    if (g_displayManagerPtr && g_displayManagerPtr->isOn()) {
        g_displayManagerPtr->showCalibrationResult(_magOffsetX,
                                                   _magOffsetY,
                                                   _magOffsetZ);
    }

    return true;
}

bool IMUService::_validateReadings(const xyzFloat& gyr,
                                   const xyzFloat& acc,
                                   const xyzFloat& mag) {
    if (isnan(gyr.x) || isnan(gyr.y) || isnan(gyr.z)) return false;
    if (isnan(acc.x) || isnan(acc.y) || isnan(acc.z)) return false;
    if (isnan(mag.x) || isnan(mag.y) || isnan(mag.z)) return false;

    if (fabsf(gyr.x) > 2000.0f || fabsf(gyr.y) > 2000.0f || fabsf(gyr.z) > 2000.0f) return false;
    if (fabsf(acc.x) > 16.0f   || fabsf(acc.y) > 16.0f   || fabsf(acc.z) > 16.0f)   return false;

    // Mesmos limites que você já usava para o mag [attached_file:65]
    if (mag.x < MAG_MIN_VALID || mag.x > MAG_MAX_VALID) return false;
    if (mag.y < MAG_MIN_VALID || mag.y > MAG_MAX_VALID) return false;
    if (mag.z < MAG_MIN_VALID || mag.z > MAG_MAX_VALID) return false;

    return true;
}

float IMUService::_applyFilter(float value, float* buffer, float& sum) {
    sum -= buffer[_filterIndex];
    buffer[_filterIndex] = value;
    sum += value;

    _filterIndex++;
    if (_filterIndex >= CUSTOM_FILTER_SIZE) {
        _filterIndex = 0;
    }

    return sum / (float)CUSTOM_FILTER_SIZE;
}
