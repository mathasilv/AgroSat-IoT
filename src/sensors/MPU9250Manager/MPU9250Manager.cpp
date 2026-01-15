/**
 * @file MPU9250Manager.cpp
 * @brief Implementação do driver MPU9250 com calibração Hard/Soft Iron
 */

#include "MPU9250Manager.h"
#include <math.h>
#include <new>

MPU9250Manager::MPU9250Manager(uint8_t addr)
    : _mpu(addr), _addr(addr), _online(false), _magOnline(false), _calibrated(false),
      _failCount(0), _lastRead(0),
      _accelX(0), _accelY(0), _accelZ(0),
      _gyroX(0), _gyroY(0), _gyroZ(0),
      _magX(0), _magY(0), _magZ(0),
      _magOffX(0), _magOffY(0), _magOffZ(0),
      _filterIdx(0) 
{
    memset(_bufAX, 0, sizeof(_bufAX));
    memset(_bufAY, 0, sizeof(_bufAY));
    memset(_bufAZ, 0, sizeof(_bufAZ));
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            _softIronMatrix[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

bool MPU9250Manager::begin() {
    DEBUG_PRINTLN("[MPU9250Manager] Inicializando...");
    
    _online = false;
    _magOnline = false;
    
    if (!_mpu.begin()) {
        DEBUG_PRINTLN("[MPU9250Manager] ERRO: MPU9250 nao detectado.");
        return false;
    }
    
    _online = true;
    DEBUG_PRINTLN("[MPU9250Manager] IMU Online.");

    if (_mpu.initMagnetometer()) {
        _magOnline = true;
        
        if (_loadOffsets()) {
            _calibrated = true;
            DEBUG_PRINTF("[MPU9250Manager] Mag calibrado: Hard Iron=(%.1f, %.1f, %.1f)\n", 
                        _magOffX, _magOffY, _magOffZ);
        } else {
            DEBUG_PRINTLN("[MPU9250Manager] Mag sem calibracao salva.");
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] Aviso: Magnetometro offline.");
    }

    return true;
}

void MPU9250Manager::update() {
    if (!_online) return;

    if (millis() - _lastRead < 20) return;
    _lastRead = millis();

    xyzFloat g = _mpu.getGValues();
    xyzFloat gyr = _mpu.getGyrValues();
    
    _accelX = _applyFilter(g.x, _bufAX);
    _accelY = _applyFilter(g.y, _bufAY);
    _accelZ = _applyFilter(g.z, _bufAZ);
    
    _filterIdx = (_filterIdx + 1) % FILTER_SIZE;

    _gyroX = gyr.x;
    _gyroY = gyr.y;
    _gyroZ = gyr.z;

    if (_magOnline) {
        xyzFloat mag = _mpu.getMagValues();
        
        if (mag.x != 0 || mag.y != 0 || mag.z != 0) {
            float mx = mag.x - _magOffX;
            float my = mag.y - _magOffY;
            float mz = mag.z - _magOffZ;
            _applySoftIronCorrection(mx, my, mz);
            _magX = mx;
            _magY = my;
            _magZ = mz;
            _failCount = 0;
        }
    }
}

void MPU9250Manager::reset() {
    _online = false;
    _failCount = 0;
    delay(50);
    begin();
}

bool MPU9250Manager::calibrateMagnetometer() {
    if (!_magOnline) return false;

    DEBUG_PRINTLN("[MPU9250Manager] Calibracao do Magnetometro (20s)");
    DEBUG_PRINTLN("  Gire o sensor lentamente em figura 8");
    
    const int maxSamples = 500;
    using SampleArray = float[3];
    SampleArray* samples = new (std::nothrow) SampleArray[maxSamples];

    if (!samples) {
        DEBUG_PRINTLN("[MPU9250Manager] ERRO: Memoria insuficiente.");
        return false;
    }

    int numSamples = 0;
    
    float minX = 9999, minY = 9999, minZ = 9999;
    float maxX = -9999, maxY = -9999, maxZ = -9999;
    
    uint32_t start = millis();

    while (millis() - start < 20000 && numSamples < maxSamples) {
        xyzFloat mag = _mpu.getMagValues();
        
        if (mag.x != 0 || mag.y != 0) {
            samples[numSamples][0] = mag.x;
            samples[numSamples][1] = mag.y;
            samples[numSamples][2] = mag.z;
            numSamples++;
            
            if(mag.x < minX) minX = mag.x;
            if(mag.x > maxX) maxX = mag.x;
            if(mag.y < minY) minY = mag.y;
            if(mag.y > maxY) maxY = mag.y;
            if(mag.z < minZ) minZ = mag.z;
            if(mag.z > maxZ) maxZ = mag.z;
            
            if (numSamples % 50 == 0) {
                DEBUG_PRINTF("  Amostras: %d / %d\n", numSamples, maxSamples);
            }
        }
        delay(40);
    }

    if (numSamples < 200) {
        DEBUG_PRINTLN("[MPU9250Manager] Falha: Poucas amostras.");
        delete[] samples;
        return false;
    }

    _magOffX = (maxX + minX) / 2.0f;
    _magOffY = (maxY + minY) / 2.0f;
    _magOffZ = (maxZ + minZ) / 2.0f;
    
    DEBUG_PRINTF("[MPU9250Manager] Hard Iron: X=%.1f, Y=%.1f, Z=%.1f\n", 
                 _magOffX, _magOffY, _magOffZ);
    
    _calculateSoftIronMatrix(samples, numSamples);
    delete[] samples;
    _saveOffsets();
    _calibrated = true;
    
    DEBUG_PRINTLN("[MPU9250Manager] Calibracao completa.");
    return true;
}

void MPU9250Manager::_applySoftIronCorrection(float& mx, float& my, float& mz) {
    float mx_corr = _softIronMatrix[0][0] * mx + 
                    _softIronMatrix[0][1] * my + 
                    _softIronMatrix[0][2] * mz;
                    
    float my_corr = _softIronMatrix[1][0] * mx + 
                    _softIronMatrix[1][1] * my + 
                    _softIronMatrix[1][2] * mz;
                    
    float mz_corr = _softIronMatrix[2][0] * mx + 
                    _softIronMatrix[2][1] * my + 
                    _softIronMatrix[2][2] * mz;
    
    mx = mx_corr;
    my = my_corr;
    mz = mz_corr;
}

void MPU9250Manager::_calculateSoftIronMatrix(float samples[][3], int numSamples) {
    float* centeredX = (float*)malloc(numSamples * sizeof(float));
    float* centeredY = (float*)malloc(numSamples * sizeof(float));
    float* centeredZ = (float*)malloc(numSamples * sizeof(float));
    
    if (!centeredX || !centeredY || !centeredZ) {
        DEBUG_PRINTLN("[MPU9250Manager] ERRO: Memoria insuficiente para Soft Iron.");
        if(centeredX) free(centeredX);
        if(centeredY) free(centeredY);
        if(centeredZ) free(centeredZ);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                _softIronMatrix[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        return;
    }
    
    for (int i = 0; i < numSamples; i++) {
        centeredX[i] = samples[i][0] - _magOffX;
        centeredY[i] = samples[i][1] - _magOffY;
        centeredZ[i] = samples[i][2] - _magOffZ;
    }
    
    float varX = 0, varY = 0, varZ = 0;
    for (int i = 0; i < numSamples; i++) {
        varX += centeredX[i] * centeredX[i];
        varY += centeredY[i] * centeredY[i];
        varZ += centeredZ[i] * centeredZ[i];
    }
    
    varX = sqrt(varX / numSamples);
    varY = sqrt(varY / numSamples);
    varZ = sqrt(varZ / numSamples);
    
    float avgScale = (varX + varY + varZ) / 3.0f;
    float scaleX = (varX > 0.1f) ? (avgScale / varX) : 1.0f;
    float scaleY = (varY > 0.1f) ? (avgScale / varY) : 1.0f;
    float scaleZ = (varZ > 0.1f) ? (avgScale / varZ) : 1.0f;
    
    _softIronMatrix[0][0] = scaleX; _softIronMatrix[0][1] = 0;      _softIronMatrix[0][2] = 0;
    _softIronMatrix[1][0] = 0;      _softIronMatrix[1][1] = scaleY; _softIronMatrix[1][2] = 0;
    _softIronMatrix[2][0] = 0;      _softIronMatrix[2][1] = 0;      _softIronMatrix[2][2] = scaleZ;
    
    free(centeredX);
    free(centeredY);
    free(centeredZ);
}

float MPU9250Manager::_applyFilter(float val, float* buf) {
    buf[_filterIdx] = val;
    float sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += buf[i];
    return sum / FILTER_SIZE;
}

void MPU9250Manager::clearOffsetsFromMemory() {
    _prefs.begin(PREFS_NAME, false);
    _prefs.clear();
    _prefs.end();
    
    _magOffX = _magOffY = _magOffZ = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            _softIronMatrix[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    _calibrated = false;
    DEBUG_PRINTLN("[MPU9250Manager] Calibracao apagada.");
}

bool MPU9250Manager::_loadOffsets() {
    if (!_prefs.begin(PREFS_NAME, true)) return false;
    if (_prefs.getUInt("magic") != MAGIC_KEY) { _prefs.end(); return false; }
    
    _magOffX = _prefs.getFloat("hx", 0);
    _magOffY = _prefs.getFloat("hy", 0);
    _magOffZ = _prefs.getFloat("hz", 0);
    
    _softIronMatrix[0][0] = _prefs.getFloat("s00", 1.0f);
    _softIronMatrix[0][1] = _prefs.getFloat("s01", 0.0f);
    _softIronMatrix[0][2] = _prefs.getFloat("s02", 0.0f);
    _softIronMatrix[1][0] = _prefs.getFloat("s10", 0.0f);
    _softIronMatrix[1][1] = _prefs.getFloat("s11", 1.0f);
    _softIronMatrix[1][2] = _prefs.getFloat("s12", 0.0f);
    _softIronMatrix[2][0] = _prefs.getFloat("s20", 0.0f);
    _softIronMatrix[2][1] = _prefs.getFloat("s21", 0.0f);
    _softIronMatrix[2][2] = _prefs.getFloat("s22", 1.0f);
    
    _prefs.end();
    return true;
}

bool MPU9250Manager::_saveOffsets() {
    if (!_prefs.begin(PREFS_NAME, false)) return false;
    
    _prefs.putUInt("magic", MAGIC_KEY);
    
    _prefs.putFloat("hx", _magOffX);
    _prefs.putFloat("hy", _magOffY);
    _prefs.putFloat("hz", _magOffZ);
    
    _prefs.putFloat("s00", _softIronMatrix[0][0]);
    _prefs.putFloat("s01", _softIronMatrix[0][1]);
    _prefs.putFloat("s02", _softIronMatrix[0][2]);
    _prefs.putFloat("s10", _softIronMatrix[1][0]);
    _prefs.putFloat("s11", _softIronMatrix[1][1]);
    _prefs.putFloat("s12", _softIronMatrix[1][2]);
    _prefs.putFloat("s20", _softIronMatrix[2][0]);
    _prefs.putFloat("s21", _softIronMatrix[2][1]);
    _prefs.putFloat("s22", _softIronMatrix[2][2]);
    
    _prefs.end();
    return true;
}