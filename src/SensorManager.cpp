/**
 * @file SensorManager.cpp
 * @brief SensorManager V6.0.0 - Gerenciamento Centralizado
 * @version 6.0.0
 * @date 2025-11-30
 */

#include "SensorManager.h"
#include <math.h>

// ============================================================================
// CONSTRUTOR
// ============================================================================
SensorManager::SensorManager()
    : _mpu9250Manager(MPU9250_ADDRESS),
      _bmp280Manager(),
      _si7021Manager(),
      _ccs811Manager(),
      _temperature(NAN),
      _lastHealthCheck(0),
      _consecutiveFailures(0) {
}

// ============================================================================
// INICIALIZAÇÃO - TODOS OS SENSORES
// ============================================================================
bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores PION (v6.0.0)...");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    bool success = false;
    uint8_t sensorsFound = 0;
    
    // 1. MPU9250Manager (9-axis IMU)
    if (_mpu9250Manager.begin()) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] MPU9250Manager: ONLINE (9-axis)");
        success = true;
    }
    
    // 2. BMP280Manager (Pressão/Temperatura)
    if (_bmp280Manager.begin()) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] BMP280Manager: ONLINE");
        success = true;
    }
    
    // 3. SI7021Manager (Umidade/Temperatura)
    if (_si7021Manager.begin()) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] SI7021Manager: ONLINE");
        success = true;
    }
    
    // 4. CCS811Manager (CO2/TVOC)
    if (_ccs811Manager.begin()) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] CCS811Manager: ONLINE");
        success = true;
    }
    
    // Auto-calibração MPU9250 (após inicialização)
    if (_mpu9250Manager.isOnline() && !_mpu9250Manager.isCalibrated()) {
        DEBUG_PRINTLN("[SensorManager] Magnetômetro OK, iniciando calibração...");
        _mpu9250Manager.calibrateMagnetometer();
    }
    
    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", sensorsFound);
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    return success;
}

// ============================================================================
// LOOP PRINCIPAL - ATUALIZAÇÃO (50Hz)
// ============================================================================
void SensorManager::update() {
    uint32_t currentTime = millis();
    
    // Health check a cada 30s
    if (currentTime - _lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }
    
    // Atualizar todos os managers
    _mpu9250Manager.update();
    _bmp280Manager.update();
    _si7021Manager.update();
    _ccs811Manager.update();
    
    // Redundância temperatura
    _updateTemperatureRedundancy();
    
    // Contar falhas consecutivas
    bool anyOnline = _mpu9250Manager.isOnline() || 
                     _bmp280Manager.isOnline() || 
                     _si7021Manager.isOnline() || 
                     _ccs811Manager.isOnline();
    
    if (!anyOnline) {
        _consecutiveFailures++;
    } else {
        _consecutiveFailures = 0;
    }
}

// ============================================================================
// REDUNDÂNCIA TEMPERATURA (Prioridade: SI7021 > BMP280)
// ============================================================================
void SensorManager::_updateTemperatureRedundancy() {
    // Prioridade 1: SI7021 (mais preciso)
    if (_si7021Manager.isOnline() && _si7021Manager.isTempValid()) {
        _temperature = _si7021Manager.getTemperature();
        return;
    }
    
    // Fallback: BMP280
    if (_bmp280Manager.isOnline() && _bmp280Manager.isTempValid()) {
        _temperature = _bmp280Manager.getTemperature();
        return;
    }
    
    // Nenhum sensor disponível
    _temperature = NAN;
}

// ============================================================================
// HEALTH CHECK CENTRALIZADO
// ============================================================================
void SensorManager::_performHealthCheck() {
    if (_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        DEBUG_PRINTLN("[SensorManager] Health check: Resetando todos os sensores...");
        resetAll();
        _consecutiveFailures = 0;
    }
    
    // Tentativa de recuperação seletiva
    if (!_mpu9250Manager.isOnline() && _mpu9250Manager.getFailCount() >= 5) {
        DEBUG_PRINTLN("[SensorManager] Recuperando MPU9250...");
        _mpu9250Manager.reset();
    }
    
    if (!_bmp280Manager.isOnline() && _bmp280Manager.getFailCount() >= 5) {
        DEBUG_PRINTLN("[SensorManager] Recuperando BMP280...");
        _bmp280Manager.forceReinit();
    }
}

// ============================================================================
// RESET TOTAL
// ============================================================================
void SensorManager::resetAll() {
    DEBUG_PRINTLN("[SensorManager] Reset total dos sensores...");
    
    _mpu9250Manager.reset();
    _bmp280Manager.forceReinit();
    _si7021Manager.reset();
    _ccs811Manager.reset();
    
    _consecutiveFailures = 0;
    _temperature = NAN;
    
    delay(500);
}

// ============================================================================
// UTILITÁRIOS
// ============================================================================
void SensorManager::getRawData(float& gx, float& gy, float& gz,
                               float& ax, float& ay, float& az,
                               float& mx, float& my, float& mz) const {
    _mpu9250Manager.getRawData(gx, gy, gz, ax, ay, az, mx, my, mz);
}

void SensorManager::printSensorStatus() const {
    DEBUG_PRINTLN("========== STATUS DOS SENSORES ==========");
    
    _mpu9250Manager.printStatus();
    _bmp280Manager.printStatus();
    
    if (_si7021Manager.isOnline()) {
        DEBUG_PRINTF(" SI7021: ONLINE");
        DEBUG_PRINTF(" (T=%.1f°C H=%.1f%%)", 
                     _si7021Manager.getTemperature(),
                     _si7021Manager.getHumidity());
        DEBUG_PRINTLN();
    } else {
        DEBUG_PRINTLN(" SI7021: OFFLINE");
    }
    
    _ccs811Manager.printStatus();
    
    // Status de redundância
    DEBUG_PRINTLN("Redundância de Temperatura:");
    if (!isnan(_temperature)) {
        DEBUG_PRINTF("  Usando: %.2f°C (%s)\n",
                     _temperature,
                     _si7021Manager.isTempValid() ? "SI7021" : "BMP280");
    } else {
        DEBUG_PRINTLN("  CRÍTICO: Nenhum sensor disponível!");
    }
    
    DEBUG_PRINTF("Falhas consecutivas: %d\n", _consecutiveFailures);
    DEBUG_PRINTLN("========================================");
}

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Escaneando barramento I2C...");
    uint8_t devicesFound = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            DEBUG_PRINTF("  Dispositivo em 0x%02X\n", addr);
            devicesFound++;
        }
    }
    
    DEBUG_PRINTF("[SensorManager] Total: %d dispositivo(s) I2C\n", devicesFound);
}
