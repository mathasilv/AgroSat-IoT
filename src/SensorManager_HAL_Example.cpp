/**
 * @file SensorManager_HAL_Example.cpp
 * @brief Exemplo de implementação do SensorManager com HAL
 * @details Demonstra como migrar métodos críticos
 * 
 * ESTE ARQUIVO É UM EXEMPLO - NÃO COMPILA DIRETAMENTE!
 * Use como referência para migrar seu SensorManager.cpp
 * 
 * @author AgroSat-IoT Team  
 * @date 2025-11-27
 */

#include "include/SensorManager_HAL.h"

// ========================================
// CONSTRUTOR COM DEPENDENCY INJECTION
// ========================================

SensorManager::SensorManager(HAL::I2C* i2c_hal)
    : _i2c_hal(i2c_hal),  // +++ Injetar HAL
      _mpu9250(0x68),     // MPU ainda usa TwoWire interno
      _seaLevelPressure(1013.25),
      _mpu9250Online(false),
      _bmp280Online(false),
      _si7021Online(false),
      _ccs811Online(false),
      _calibrated(false),
      _filterIndex(0),
      _sumAccelX(0), _sumAccelY(0), _sumAccelZ(0),
      _lastReadTime(0),
      _lastCCS811Read(0),
      _lastSI7021Read(0),
      _lastHealthCheck(0),
      _consecutiveFailures(0),
      _lastBMP280Reinit(0),
      _bmp280FailCount(0),
      _historyIndex(0),
      _historyFull(false),
      _lastUpdateTime(0),
      _lastPressureRead(0),
      _identicalReadings(0),
      _warmupStartTime(0),
      _si7021TempValid(false),
      _bmp280TempValid(false),
      _si7021TempFailures(0),
      _bmp280TempFailures(0) {
    
    // Inicializar buffers
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0;
        _accelYBuffer[i] = 0;
        _accelZBuffer[i] = 0;
    }
}

// ========================================
// SCAN I2C - EXEMPLO DE USO DO HAL
// ========================================

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("\n[SensorMgr] Scanning I2C bus (HAL)...");
    
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        // +++ Usar HAL em vez de Wire.beginTransmission()
        if (_i2c_hal->write(address, nullptr, 0)) {
            DEBUG_PRINTF("[SensorMgr] Device found at 0x%02X", address);
            
            // Identificar dispositivo
            switch (address) {
                case 0x68: DEBUG_PRINTLN(" - MPU9250/MPU6050"); break;
                case 0x76:
                case 0x77: DEBUG_PRINTLN(" - BMP280"); break;
                case 0x40: DEBUG_PRINTLN(" - SI7021"); break;
                case 0x5A:
                case 0x5B: DEBUG_PRINTLN(" - CCS811"); break;
                default:   DEBUG_PRINTLN(" - Unknown"); break;
            }
            deviceCount++;
        }
    }
    
    DEBUG_PRINTF("[SensorMgr] Scan complete. Found %d devices.\n", deviceCount);
}

// ========================================
// INIT SI7021 - MIGRADO PARA HAL
// ========================================

bool SensorManager::_initSI7021() {
    DEBUG_PRINTLN("[SensorMgr] Initializing SI7021 (HAL)...");
    
    const uint8_t SI7021_ADDR = 0x40;
    const uint8_t CMD_READ_USER_REG = 0xE7;
    const uint8_t CMD_WRITE_USER_REG = 0xE6;
    const uint8_t CMD_SOFT_RESET = 0xFE;
    
    // Soft reset usando HAL
    if (!_i2c_hal->write(SI7021_ADDR, &CMD_SOFT_RESET, 1)) {
        DEBUG_PRINTLN("[SensorMgr]   ❌ SI7021 soft reset failed");
        return false;
    }
    delay(15);  // Wait for reset
    
    // Read user register
    uint8_t userReg = _i2c_hal->readRegister(SI7021_ADDR, CMD_READ_USER_REG);
    DEBUG_PRINTF("[SensorMgr]   User register: 0x%02X\n", userReg);
    
    // Configure: 12-bit RH, 14-bit Temp, disable heater
    uint8_t newConfig = (userReg & 0x3A) | 0x00;  // Resolution 00 = 12bit RH, 14bit Temp
    
    uint8_t configCmd[2] = {CMD_WRITE_USER_REG, newConfig};
    if (!_i2c_hal->write(SI7021_ADDR, configCmd, 2)) {
        DEBUG_PRINTLN("[SensorMgr]   ❌ Failed to write config");
        return false;
    }
    
    // Verify write
    uint8_t verifyReg = _i2c_hal->readRegister(SI7021_ADDR, CMD_READ_USER_REG);
    if (verifyReg != newConfig) {
        DEBUG_PRINTF("[SensorMgr]   ❌ Config mismatch: wrote 0x%02X, read 0x%02X\n", 
                    newConfig, verifyReg);
        return false;
    }
    
    _si7021Online = true;
    _si7021TempValid = false;
    _si7021TempFailures = 0;
    
    DEBUG_PRINTLN("[SensorMgr]   ✅ SI7021 initialized");
    return true;
}

// ========================================
// UPDATE SI7021 - MIGRADO PARA HAL
// ========================================

void SensorManager::_updateSI7021() {
    if (!_si7021Online) return;
    
    unsigned long now = millis();
    if (now - _lastSI7021Read < SI7021_READ_INTERVAL) return;
    _lastSI7021Read = now;
    
    const uint8_t SI7021_ADDR = 0x40;
    const uint8_t CMD_MEASURE_RH_HOLD = 0xE5;
    const uint8_t CMD_READ_TEMP_PREV = 0xE0;
    
    // 1. Trigger RH measurement (hold master mode)
    if (!_i2c_hal->write(SI7021_ADDR, &CMD_MEASURE_RH_HOLD, 1)) {
        DEBUG_PRINTLN("[SensorMgr] SI7021 RH trigger failed");
        _handleSI7021Failure();
        return;
    }
    
    delay(25);  // Wait for conversion (typ 12ms, max 22ms)
    
    // 2. Read RH data (3 bytes: MSB, LSB, CRC)
    uint8_t rhData[3];
    if (!_i2c_hal->read(SI7021_ADDR, rhData, 3)) {
        DEBUG_PRINTLN("[SensorMgr] SI7021 RH read failed");
        _handleSI7021Failure();
        return;
    }
    
    // 3. Calculate humidity
    uint16_t rhRaw = (rhData[0] << 8) | rhData[1];
    float humidity = ((125.0 * rhRaw) / 65536.0) - 6.0;
    
    // 4. Read temperature from previous RH measurement
    if (!_i2c_hal->write(SI7021_ADDR, &CMD_READ_TEMP_PREV, 1)) {
        DEBUG_PRINTLN("[SensorMgr] SI7021 temp trigger failed");
        _handleSI7021Failure();
        return;
    }
    
    uint8_t tempData[2];
    if (!_i2c_hal->read(SI7021_ADDR, tempData, 2)) {
        DEBUG_PRINTLN("[SensorMgr] SI7021 temp read failed");
        _handleSI7021Failure();
        return;
    }
    
    uint16_t tempRaw = (tempData[0] << 8) | tempData[1];
    float temperature = ((175.72 * tempRaw) / 65536.0) - 46.85;
    
    // 5. Validate readings
    if (_validateReading(humidity, 0, 100) && _validateReading(temperature, -40, 85)) {
        _humidity = humidity;
        _temperatureSI = temperature;
        _si7021TempValid = true;
        _si7021TempFailures = 0;
        
        DEBUG_PRINTF("[SensorMgr] SI7021: %.1f°C, %.1f%% RH\n", temperature, humidity);
    } else {
        DEBUG_PRINTLN("[SensorMgr] SI7021 invalid reading");
        _handleSI7021Failure();
    }
}

void SensorManager::_handleSI7021Failure() {
    _si7021TempFailures++;
    if (_si7021TempFailures >= MAX_TEMP_FAILURES) {
        _si7021TempValid = false;
        DEBUG_PRINTLN("[SensorMgr] ⚠️ SI7021 marked invalid");
    }
}

// ========================================
// NOTAS DE MIGRAÇÃO
// ========================================

/**
 * PADRÃO DE MIGRAÇÃO:
 * 
 * ANTES (acoplado ao Wire):
 *   Wire.beginTransmission(address);
 *   Wire.write(data);
 *   Wire.endTransmission();
 * 
 * DEPOIS (usando HAL):
 *   _i2c_hal->write(address, &data, 1);
 * 
 * ANTES:
 *   Wire.requestFrom(address, length);
 *   byte = Wire.read();
 * 
 * DEPOIS:
 *   _i2c_hal->read(address, buffer, length);
 * 
 * ANTES:
 *   Wire.beginTransmission(address);
 *   Wire.write(reg);
 *   Wire.endTransmission(false);
 *   Wire.requestFrom(address, 1);
 *   value = Wire.read();
 * 
 * DEPOIS:
 *   value = _i2c_hal->readRegister(address, reg);
 */