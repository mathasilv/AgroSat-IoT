/**
 * @file MPU9250Manager.cpp
 * @brief Implementação limpa do Manager MPU9250
 */

#include "MPU9250Manager.h"

MPU9250Manager::MPU9250Manager(uint8_t addr)
    : _mpu(addr), _addr(addr), _online(false), _magOnline(false), _calibrated(false),
      _failCount(0), _lastRead(0),
      _accelX(0), _accelY(0), _accelZ(0),
      _gyroX(0), _gyroY(0), _gyroZ(0),
      _magX(0), _magY(0), _magZ(0),
      _magOffX(0), _magOffY(0), _magOffZ(0),
      _filterIdx(0) 
{
    // Inicializar buffers com 0
    memset(_bufAX, 0, sizeof(_bufAX));
    memset(_bufAY, 0, sizeof(_bufAY));
    memset(_bufAZ, 0, sizeof(_bufAZ));
}

bool MPU9250Manager::begin() {
    DEBUG_PRINTLN("[MPU9250Manager] Inicializando...");
    
    _online = false;
    _magOnline = false;
    
    // 1. Inicializar IMU Principal
    if (!_mpu.begin()) {
        DEBUG_PRINTLN("[MPU9250Manager] ERRO: MPU9250 não detectado (0x71).");
        return false;
    }
    
    _online = true;
    DEBUG_PRINTLN("[MPU9250Manager] IMU Online.");

    // 2. Inicializar Magnetômetro
    if (_mpu.initMagnetometer()) {
        _magOnline = true;
        
        // 3. Carregar Calibração
        if (_loadOffsets()) {
            _calibrated = true;
            DEBUG_PRINTF("[MPU9250Manager] Mag Calibrado (Load): X=%.1f Y=%.1f Z=%.1f\n", _magOffX, _magOffY, _magOffZ);
        } else {
            DEBUG_PRINTLN("[MPU9250Manager] Mag sem calibração salva.");
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] Aviso: Magnetômetro offline.");
    }

    return true;
}

void MPU9250Manager::update() {
    if (!_online) return;

    // Leitura a 50Hz (20ms)
    if (millis() - _lastRead < 20) return;
    _lastRead = millis();

    xyzFloat g = _mpu.getGValues();
    xyzFloat gyr = _mpu.getGyrValues();
    
    // Aplicar Filtro no Acelerômetro
    _accelX = _applyFilter(g.x, _bufAX);
    _accelY = _applyFilter(g.y, _bufAY);
    _accelZ = _applyFilter(g.z, _bufAZ);
    
    // IMPORTANTE: Incremento do índice do filtro (uma vez por ciclo completo)
    _filterIdx = (_filterIdx + 1) % FILTER_SIZE;

    _gyroX = gyr.x;
    _gyroY = gyr.y;
    _gyroZ = gyr.z;

    if (_magOnline) {
        xyzFloat mag = _mpu.getMagValues();
        // Verifica se leitura é válida (não zero exato em todos eixos)
        if (mag.x != 0 || mag.y != 0 || mag.z != 0) {
            _magX = mag.x - _magOffX;
            _magY = mag.y - _magOffY;
            _magZ = mag.z - _magOffZ;
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

// === Calibração ===

bool MPU9250Manager::calibrateMagnetometer() {
    if (!_magOnline) return false;

    DEBUG_PRINTLN("[MPU9250Manager] Calibrando Mag (10s)... Gire o sensor em 8.");
    
    float minX = 9999, minY = 9999, minZ = 9999;
    float maxX = -9999, maxY = -9999, maxZ = -9999;
    
    uint32_t start = millis();
    int samples = 0;

    while (millis() - start < 10000) {
        xyzFloat mag = _mpu.getMagValues();
        if (mag.x != 0 || mag.y != 0) {
            if(mag.x < minX) minX = mag.x;
            if(mag.x > maxX) maxX = mag.x;
            if(mag.y < minY) minY = mag.y;
            if(mag.y > maxY) maxY = mag.y;
            if(mag.z < minZ) minZ = mag.z;
            if(mag.z > maxZ) maxZ = mag.z;
            samples++;
        }
        delay(20);
    }

    if (samples < 100) {
        DEBUG_PRINTLN("[MPU9250Manager] Falha: Poucas amostras.");
        return false;
    }

    _magOffX = (maxX + minX) / 2.0f;
    _magOffY = (maxY + minY) / 2.0f;
    _magOffZ = (maxZ + minZ) / 2.0f;
    
    _saveOffsets();
    _calibrated = true;
    
    DEBUG_PRINTF("[MPU9250Manager] Calibração OK: X=%.1f Y=%.1f Z=%.1f\n", _magOffX, _magOffY, _magOffZ);
    return true;
}

// === Métodos Privados ===

float MPU9250Manager::_applyFilter(float val, float* buf) {
    // Filtro Média Móvel Simples
    // Usa o índice atual (_filterIdx) que é gerenciado pelo update()
    buf[_filterIdx] = val;
    
    float sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += buf[i];
    
    return sum / FILTER_SIZE;
}

void MPU9250Manager::getRawData(float& gx, float& gy, float& gz, 
                                float& ax, float& ay, float& az,
                                float& mx, float& my, float& mz) const {
    gx=_gyroX; gy=_gyroY; gz=_gyroZ;
    ax=_accelX; ay=_accelY; az=_accelZ;
    mx=_magX; my=_magY; mz=_magZ;
}

void MPU9250Manager::getMagOffsets(float& x, float& y, float& z) const {
    x = _magOffX; y = _magOffY; z = _magOffZ;
}

void MPU9250Manager::clearOffsetsFromMemory() {
    _prefs.begin(PREFS_NAME, false);
    _prefs.clear();
    _prefs.end();
    _magOffX = _magOffY = _magOffZ = 0;
    _calibrated = false;
    DEBUG_PRINTLN("[MPU9250Manager] Calibração apagada.");
}

bool MPU9250Manager::_loadOffsets() {
    if (!_prefs.begin(PREFS_NAME, true)) return false;
    
    if (_prefs.getUInt("magic") != MAGIC_KEY) {
        _prefs.end();
        return false;
    }
    
    _magOffX = _prefs.getFloat("x", 0);
    _magOffY = _prefs.getFloat("y", 0);
    _magOffZ = _prefs.getFloat("z", 0);
    _prefs.end();
    return true;
}

bool MPU9250Manager::_saveOffsets() {
    if (!_prefs.begin(PREFS_NAME, false)) return false;
    _prefs.putUInt("magic", MAGIC_KEY);
    _prefs.putFloat("x", _magOffX);
    _prefs.putFloat("y", _magOffY);
    _prefs.putFloat("z", _magOffZ);
    _prefs.end();
    return true;
}