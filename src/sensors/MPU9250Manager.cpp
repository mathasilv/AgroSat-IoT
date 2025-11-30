/**
 * @file MPU9250Manager.cpp
 * @brief MPU9250Manager Implementation - 9-axis IMU
 */
#include "MPU9250Manager.h"
#include <math.h>


// ============================================================================
// CONSTRUTOR
// ============================================================================
MPU9250Manager::MPU9250Manager(uint8_t addr)
    : _mpu(addr), _addr(addr), _online(false), _magOnline(false), _calibrated(false),
      _failCount(0), _lastReadTime(0),
      _accelX(0), _accelY(0), _accelZ(0),
      _gyroX(0), _gyroY(0), _gyroZ(0),
      _magX(0), _magY(0), _magZ(0),
      _magOffsetX(0), _magOffsetY(0), _magOffsetZ(0),
      _filterIndex(0), _sumAccelX(0), _sumAccelY(0), _sumAccelZ(0) {
    
    // Inicializar buffers do filtro
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0f;
        _accelYBuffer[i] = 0.0f;
        _accelZBuffer[i] = 0.0f;
    }
}


// ============================================================================
// INICIALIZAÇÃO
// ============================================================================
bool MPU9250Manager::begin() {
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] Inicializando MPU9250 9-axis IMU");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    _online = false;
    _magOnline = false;
    _calibrated = false;
    _failCount = 0;
    
    // Verificar presença I2C
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTF("[MPU9250Manager] MPU9250 não detectado em 0x%02X\n", _addr);
        return false;
    }
    
    // Inicializar MPU9250 (Acc + Gyro)
    if (!_initMPU()) {
        DEBUG_PRINTLN("[MPU9250Manager] FALHA: Inicialização MPU9250");
        return false;
    }
    
    _online = true;
    DEBUG_PRINTLN("[MPU9250Manager] MPU9250 (Acc+Gyro): ONLINE");
    
    // Inicializar Magnetômetro AK8963
    if (_initMagnetometer()) {
        _magOnline = true;
        DEBUG_PRINTLN("[MPU9250Manager] Magnetômetro AK8963: ONLINE");
        
        // Auto-calibração magnetômetro
        DEBUG_PRINTLN("[MPU9250Manager] Iniciando calibração automática...");
        if (calibrateMagnetometer()) {
            _calibrated = true;
            DEBUG_PRINTLN("[MPU9250Manager] ========================================");
            DEBUG_PRINTLN("[MPU9250Manager] MPU9250 INICIALIZADO COM SUCESSO!");
            DEBUG_PRINTF("[MPU9250Manager] Mag Offsets: X=%.2f Y=%.2f Z=%.2f µT\n", 
                         _magOffsetX, _magOffsetY, _magOffsetZ);
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] AVISO: Magnetômetro não inicializado");
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    return true;
}


// ============================================================================
// INICIALIZAÇÃO - MPU9250 (ACC + GYRO)
// ============================================================================
bool MPU9250Manager::_initMPU() {
    DEBUG_PRINTLN("[MPU9250Manager] Testando MPU9250...");
    if (!_mpu.begin()) {
        DEBUG_PRINTLN("[MPU9250Manager] FALHA: begin()");
        return false;
    }
    
    // 3 testes (config.h padrão)
    for(int i = 0; i < 3; i++) {
        xyzFloat test = _mpu.getGValues();
        if (!isnan(test.x) && fabs(test.x) < 10.0f) {
            DEBUG_PRINTF("[MPU9250Manager] Teste %d OK: %.2fg\n", i+1, test.x);
            return true;
        }
        delay(50);
    }
    return false;
}


// ============================================================================
// INICIALIZAÇÃO - MAGNETÔMETRO AK8963
// ============================================================================
bool MPU9250Manager::_initMagnetometer() {
    return _mpu.initMagnetometer();
}


// ============================================================================
// ATUALIZAÇÃO - 50Hz
// ============================================================================
void MPU9250Manager::update() {
    if (!_online) return;
    
    uint32_t currentTime = millis();
    if (currentTime - _lastReadTime < READ_INTERVAL) return;
    
    _lastReadTime = currentTime;
    _updateIMU();
}


// ============================================================================
// ATUALIZAÇÃO - IMU (ACC + GYRO + MAG)
// ============================================================================
void MPU9250Manager::_updateIMU() {
    // Ler dados brutos
    xyzFloat gValues = _mpu.getGValues();
    xyzFloat gyrValues = _mpu.getGyrValues();
    xyzFloat magValues = _magOnline ? _mpu.getMagValues() : xyzFloat();
    
    // Validar leituras
    if (!_validateReadings(gyrValues.x, gyrValues.y, gyrValues.z,
                           gValues.x, gValues.y, gValues.z,
                           magValues.x, magValues.y, magValues.z)) {
        _failCount++;
        
        if (_failCount >= 10) {
            DEBUG_PRINTLN("[MPU9250Manager] 10 falhas - tentando reinicializar...");
            _online = false;
            delay(100);
            if (begin()) {
                _failCount = 0;
                DEBUG_PRINTLN("[MPU9250Manager] Reinicializado com sucesso!");
            }
        }
        return;
    }
    
    // Aplicar filtro de média móvel no acelerômetro
    _accelX = _applyFilter(gValues.x, _accelXBuffer, _sumAccelX);
    _accelY = _applyFilter(gValues.y, _accelYBuffer, _sumAccelY);
    _accelZ = _applyFilter(gValues.z, _accelZBuffer, _sumAccelZ);
    
    // Giroscópio (sem filtro - precisa ser responsivo)
    _gyroX = gyrValues.x;
    _gyroY = gyrValues.y;
    _gyroZ = gyrValues.z;
    
    // Magnetômetro (com calibração)
    if (_magOnline) {
        _magX = magValues.x - _magOffsetX;
        _magY = magValues.y - _magOffsetY;
        _magZ = magValues.z - _magOffsetZ;
    }
    
    _failCount = 0;
}


// ============================================================================
// CALIBRAÇÃO - MAGNETÔMETRO (10s)
// ============================================================================
bool MPU9250Manager::calibrateMagnetometer() {
    if (!_magOnline) {
        DEBUG_PRINTLN("[MPU9250Manager] Magnetômetro offline - calibração cancelada");
        return false;
    }
    
    float magMin[3] = {9999.0f, 9999.0f, 9999.0f};
    float magMax[3] = {-9999.0f, -9999.0f, -9999.0f};
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] CALIBRAÇÃO MAGNETÔMETRO");
    DEBUG_PRINTLN("[MPU9250Manager] Rotacione o dispositivo em todos os eixos...");
    DEBUG_PRINTLN("[MPU9250Manager] Duração: 10 segundos");
    
    uint32_t startTime = millis();
    uint16_t samples = 0;
    const uint32_t CALIB_TIME = 10000;
    
    // Loop de calibração com feedback
    while (millis() - startTime < CALIB_TIME) {
        xyzFloat mag = _mpu.getMagValues();
        
        if (!isnan(mag.x) && !isnan(mag.y) && !isnan(mag.z)) {
            magMin[0] = min(magMin[0], mag.x);
            magMin[1] = min(magMin[1], mag.y);
            magMin[2] = min(magMin[2], mag.z);
            
            magMax[0] = max(magMax[0], mag.x);
            magMax[1] = max(magMax[1], mag.y);
            magMax[2] = max(magMax[2], mag.z);
            
            samples++;
        }
        
        // Feedback a cada 2s
        if ((millis() - startTime) % 2000 < 100) {
            DEBUG_PRINTF("[MPU9250Manager] Calibrando... %lus / 10s (%d samples)\n",
                         (millis() - startTime) / 1000, samples);
        }
        
        delay(50);
    }
    
    // Calcular offsets (ponto médio)
    if (samples >= 100) {
        _magOffsetX = (magMax[0] + magMin[0]) / 2.0f;
        _magOffsetY = (magMax[1] + magMin[1]) / 2.0f;
        _magOffsetZ = (magMax[2] + magMin[2]) / 2.0f;
        
        DEBUG_PRINTLN("[MPU9250Manager] Magnetometro calibrado!");
        DEBUG_PRINTF("[MPU9250Manager] Offsets: X=%.2f Y=%.2f Z=%.2f µT\n",
                     _magOffsetX, _magOffsetY, _magOffsetZ);
        DEBUG_PRINTF("[MPU9250Manager] Samples coletados: %d\n", samples);
        DEBUG_PRINTLN("[MPU9250Manager] ========================================");
        
        return true;
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] FALHA: Amostras insuficientes");
        DEBUG_PRINTLN("[MPU9250Manager] Usando offsets zerados");
        _magOffsetX = _magOffsetY = _magOffsetZ = 0.0f;
        return false;
    }
}


// ============================================================================
// VALIDAÇÃO DE LEITURAS
// ============================================================================
bool MPU9250Manager::_validateReadings(float gx, float gy, float gz,
                                        float ax, float ay, float az,
                                        float mx, float my, float mz) {
    // Validar giroscópio (±2000°/s max)
    if (isnan(gx) || isnan(gy) || isnan(gz)) return false;
    if (abs(gx) > 2000 || abs(gy) > 2000 || abs(gz) > 2000) return false;
    
    // Validar acelerômetro (±16g max)
    if (isnan(ax) || isnan(ay) || isnan(az)) return false;
    if (abs(ax) > 16 || abs(ay) > 16 || abs(az) > 16) return false;
    
    // Validar magnetômetro (se online)
    if (_magOnline) {
        if (isnan(mx) || isnan(my) || isnan(mz)) return false;
        if (mx < MAG_MIN_VALID || mx > MAG_MAX_VALID) return false;
        if (my < MAG_MIN_VALID || my > MAG_MAX_VALID) return false;
        if (mz < MAG_MIN_VALID || mz > MAG_MAX_VALID) return false;
    }
    
    return true;
}


// ============================================================================
// FILTRO MÉDIA MÓVEL (OTIMIZADO)
// ============================================================================
float MPU9250Manager::_applyFilter(float newValue, float* buffer, float& sum) {
    sum -= buffer[_filterIndex];
    buffer[_filterIndex] = newValue;
    sum += newValue;
    
    _filterIndex = (_filterIndex + 1) % FILTER_SIZE;
    
    return sum * (1.0f / FILTER_SIZE);  // Multiplicação é mais rápida que divisão
}


// ============================================================================
// GETTERS
// ============================================================================
float MPU9250Manager::getAccelMagnitude() const {
    return sqrt(_accelX*_accelX + _accelY*_accelY + _accelZ*_accelZ);
}


void MPU9250Manager::getMagOffsets(float& x, float& y, float& z) const {
    x = _magOffsetX;
    y = _magOffsetY;
    z = _magOffsetZ;
}


void MPU9250Manager::setMagOffsets(float x, float y, float z) {
    _magOffsetX = x;
    _magOffsetY = y;
    _magOffsetZ = z;
    _calibrated = true;
}


void MPU9250Manager::getRawData(float& gx, float& gy, float& gz,
                                 float& ax, float& ay, float& az,
                                 float& mx, float& my, float& mz) const {
    gx = _gyroX; gy = _gyroY; gz = _gyroZ;
    ax = _accelX; ay = _accelY; az = _accelZ;
    mx = _magX; my = _magY; mz = _magZ;
}


// ============================================================================
// STATUS
// ============================================================================
void MPU9250Manager::printStatus() const {
    DEBUG_PRINTF(" MPU9250: %s", _online ? "ONLINE" : "OFFLINE");
    
    if (_online) {
        DEBUG_PRINTF(" (9-axis)");
        if (_magOnline) {
            DEBUG_PRINTF(" Mag: %s", _calibrated ? "CALIBRADO" : "SEM CALIB");
        } else {
            DEBUG_PRINT(" Mag: OFFLINE");
        }
        
        if (_failCount > 0) {
            DEBUG_PRINTF(" [fails=%d]", _failCount);
        }
    }
    
    DEBUG_PRINTLN();
}


void MPU9250Manager::reset() {
    DEBUG_PRINTLN("[MPU9250Manager] Reset forçado...");
    _online = false;
    _magOnline = false;
    _calibrated = false;
    _failCount = 0;
    delay(100);
    begin();
}
