/**
 * @file SensorManager.cpp
 * @brief Implementação do orquestrador de sensores I2C
 */

#include "SensorManager.h"
#include <math.h>

SensorManager::SensorManager()
    : _mpu9250(MPU9250_ADDRESS),
      _bmp280(),
      _si7021(),
      _ccs811(),
      _sensorCount(0),
      _lastEnvCompensation(0),
      _lastHealthCheck(0),
      _consecutiveFailures(0),
      _temperature(NAN),
      _i2cMutex(NULL)
{
    _i2cMutex = xSemaphoreCreateMutex();
}

bool SensorManager::_lockI2C() {
    if (_i2cMutex == NULL) return false;
    return (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE);
}

void SensorManager::_unlockI2C() {
    if (_i2cMutex != NULL) {
        xSemaphoreGive(_i2cMutex);
    }
}

bool SensorManager::begin() {
    if (!_lockI2C()) {
        DEBUG_PRINTLN("[SensorManager] Erro: I2C ocupado ao iniciar!");
        return false;
    }

    DEBUG_PRINTLN("[SensorManager] Inicializando sensores...");
    
    _sensorCount = 0;
    
    // MPU9250
    if (_mpu9250.begin()) {
        _sensorCount++;
        DEBUG_PRINTLN("[SensorManager] MPU9250Manager: ONLINE (9-axis)");
    }
    
    // BMP280
    if (_bmp280.begin()) {
        _sensorCount++;
        DEBUG_PRINTLN("[SensorManager] BMP280Manager: ONLINE");
    }
    
    // SI7021
    if (_si7021.begin()) {
        _sensorCount++;
        DEBUG_PRINTLN("[SensorManager] SI7021Manager: ONLINE");
    }
    
    // CCS811
    if (_ccs811.begin()) {
        _sensorCount++;
        DEBUG_PRINTLN("[SensorManager] CCS811Manager: ONLINE");
    }

    _unlockI2C(); 

    restoreCCS811Baseline();
    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", _sensorCount);
    
    return (_sensorCount > 0);
}

void SensorManager::updateFast() {
    if (_lockI2C()) {
        _mpu9250.update();
        _bmp280.update();
        _updateTemperatureRedundancy();
        _unlockI2C();
    }
}

void SensorManager::updateSlow() {
    static uint32_t lastSlowUpdate = 0;
    if (millis() - lastSlowUpdate < 2000) return;
    lastSlowUpdate = millis();

    if (_lockI2C()) {
        _si7021.update();
        _unlockI2C();
    }
    
    delay(10);
    
    if (_lockI2C()) {
        _ccs811.update();
        _autoApplyEnvironmentalCompensation();
        _unlockI2C();
    }
}

void SensorManager::updateHealth() {
    uint32_t currentTime = millis();

    if (currentTime - _lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }

    bool anyOnline = _mpu9250.isOnline() || _bmp280.isOnline() || _si7021.isOnline() || _ccs811.isOnline();
    if (!anyOnline) {
        _consecutiveFailures++;
    } else {
        _consecutiveFailures = 0;
    }
}

void SensorManager::update() {
    updateFast();
    updateSlow();
    updateHealth();
}

void SensorManager::_performHealthCheck() {
    if (_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        DEBUG_PRINTLN("[SensorManager] Falha critica global: Resetando todos os sensores...");
        resetAll(); 
        return;
    }
    
    if (_lockI2C()) {
        if (!_mpu9250.isOnline() && _mpu9250.getFailCount() >= 5) {
            _mpu9250.reset();
        }
        if (!_bmp280.isOnline() && _bmp280.getFailCount() >= 5) {
             _bmp280.forceReinit();
        }
        _unlockI2C();
    }
}

void SensorManager::reset() {
    resetAll();
}

void SensorManager::resetAll() {
    if (_lockI2C()) {
        DEBUG_PRINTLN("[SensorManager] >>> RECUPERANDO BARRAMENTO I2C <<<");
        
        Wire.end();
        delay(50);
        
        // Bit-Banging para destravar SDA (9 Clocks)
        pinMode(SENSOR_I2C_SDA, OUTPUT);
        pinMode(SENSOR_I2C_SCL, OUTPUT);
        digitalWrite(SENSOR_I2C_SDA, HIGH);
        digitalWrite(SENSOR_I2C_SCL, HIGH);
        
        for(int i = 0; i < 9; i++) {
            digitalWrite(SENSOR_I2C_SCL, LOW);
            delayMicroseconds(10);
            digitalWrite(SENSOR_I2C_SCL, HIGH);
            delayMicroseconds(10);
        }
        
        // Stop
        digitalWrite(SENSOR_I2C_SDA, LOW);
        delayMicroseconds(10);
        digitalWrite(SENSOR_I2C_SCL, HIGH);
        delayMicroseconds(10);
        digitalWrite(SENSOR_I2C_SDA, HIGH);
        delay(50);
        
        // Reinicia Wire
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000);
        
        DEBUG_PRINTLN("[SensorManager] Barramento reiniciado. Reconfigurando sensores...");
        
        _mpu9250.reset();
        _bmp280.forceReinit();
        _si7021.reset();
        _ccs811.reset();
        
        _unlockI2C();
    }
    _consecutiveFailures = 0;
    _temperature = NAN;
}

bool SensorManager::recalibrateMagnetometer() {
    if (!_mpu9250.isOnline() || !_mpu9250.isMagOnline()) return false;
    
    DEBUG_PRINTLN("[SensorManager] INICIANDO RECALIBRACAO... (3s)");
    delay(3000); // Tempo para o usuário se preparar
    
    bool success = false;
    if (_lockI2C()) {
        success = _mpu9250.calibrateMagnetometer();
        _unlockI2C();
    }
    return success;
}

void SensorManager::clearMagnetometerCalibration() {
    if (_lockI2C()) {
        _mpu9250.clearOffsetsFromMemory();
        _unlockI2C();
    }
}

void SensorManager::printMagnetometerCalibration() const {
    DEBUG_PRINTLN("[SensorManager] --- Calibracao Magnetometro ---");
    float x, y, z;
    _mpu9250.getMagOffsets(x, y, z); 
    DEBUG_PRINTF("Offsets: %.2f, %.2f, %.2f\n", x, y, z);
}

void SensorManager::getMagnetometerOffsets(float& x, float& y, float& z) const {
    _mpu9250.getMagOffsets(x, y, z);
}

bool SensorManager::applyCCS811EnvironmentalCompensation(float temperature, float humidity) {
    if (!_ccs811.isOnline()) return false;

    if (_lockI2C()) {
        _ccs811.setEnvironmentalData(humidity, temperature);
        _unlockI2C();
    }
    return true;
}

void SensorManager::_autoApplyEnvironmentalCompensation() {
    uint32_t now = millis();
    if (now - _lastEnvCompensation < ENV_COMPENSATION_INTERVAL) return;
    _lastEnvCompensation = now;
    
    if (!_ccs811.isOnline()) return;
    
    float temp = !isnan(_temperature) ? _temperature : 25.0f;
    float hum = _si7021.isOnline() ? _si7021.getHumidity() : 50.0f;
    _ccs811.setEnvironmentalData(hum, temp);
    DEBUG_PRINTF("[SensorManager] Comp. Amb.: T=%.1f H=%.1f\n", temp, hum);
}

bool SensorManager::saveCCS811Baseline() {
    bool success = false;
    if (_lockI2C()) {
        success = _ccs811.saveBaseline();
        _unlockI2C();
    }
    return success;
}

bool SensorManager::restoreCCS811Baseline() {
    bool success = false;
    if (_lockI2C()) {
        success = _ccs811.restoreBaseline();
        _unlockI2C();
    }
    return success;
}

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Scanning I2C Bus...");
    if (_lockI2C()) {
        uint8_t count = 0;
        for (uint8_t i = 1; i < 127; i++) {
            Wire.beginTransmission(i);
            if (Wire.endTransmission() == 0) {
                DEBUG_PRINTF("  Device found at: 0x%02X\n", i);
                count++;
            }
        }
        _unlockI2C();
    }
}

void SensorManager::getRawData(float& gx, float& gy, float& gz,
                               float& ax, float& ay, float& az,
                               float& mx, float& my, float& mz) const {
    _mpu9250.getRawData(gx, gy, gz, ax, ay, az, mx, my, mz);
}

void SensorManager::_updateTemperatureRedundancy() {
    if (_si7021.isOnline() && _si7021.isTempValid()) {
        _temperature = _si7021.getTemperature();
    } else if (_bmp280.isOnline() && _bmp280.isTempValid()) {
        _temperature = _bmp280.getTemperature();
    } else {
        _temperature = NAN;
    }
}

uint8_t SensorManager::getOnlineSensors() const {
    uint8_t count = 0;
    if (_mpu9250.isOnline()) count++;
    if (_bmp280.isOnline()) count++;
    if (_si7021.isOnline()) count++;
    if (_ccs811.isOnline()) count++;
    return count;
}

void SensorManager::printStatus() const { printDetailedStatus(); }
void SensorManager::printSensorStatus() const { printDetailedStatus(); }

void SensorManager::printDetailedStatus() const {
    DEBUG_PRINTLN("--- STATUS DETALHADO (SensorManager) ---");
    DEBUG_PRINTF("MPU9250: %s\n", _mpu9250.isOnline() ? "ONLINE" : "OFFLINE");
    DEBUG_PRINTF("BMP280:  %s (T: %.1f C)\n", _bmp280.isOnline() ? "ONLINE" : "OFFLINE", _bmp280.getTemperature());
    DEBUG_PRINTF("SI7021:  %s (RH: %.1f %%)\n", _si7021.isOnline() ? "ONLINE" : "OFFLINE", _si7021.getHumidity());
    
    bool ccsOnline = _ccs811.isOnline();
    bool ccsWarm = _ccs811.isWarmupComplete();
    DEBUG_PRINTF("CCS811:  %s (eCO2: %d) %s\n", 
                 ccsOnline ? "ONLINE" : "OFFLINE", 
                 _ccs811.geteCO2(),
                 (ccsOnline && !ccsWarm) ? "[WARM-UP]" : "");
                 
    DEBUG_PRINTF("Temp Final: %.2f C\n", _temperature);
    DEBUG_PRINTLN("----------------------------------------");
}
