/**
 * @file SensorManager.cpp
 * @brief SensorManager V6.1.0 - Com Recuperação Física de I2C
 */

#include "SensorManager.h"
#include <math.h>
#include <Preferences.h>

// ============================================================================
// CONSTRUTOR
// ============================================================================
SensorManager::SensorManager()
    : _mpu9250(MPU9250_ADDRESS),
      _sensorCount(0),
      _lastEnvCompensation(0),
      _lastHealthCheck(0),
      _consecutiveFailures(0),
      _temperature(NAN),
      _i2cMutex(NULL)
{
    _i2cMutex = xSemaphoreCreateMutex();
}

// ============================================================================
// HELPERS DO MUTEX
// ============================================================================
bool SensorManager::_lockI2C() {
    if (_i2cMutex == NULL) return false;
    return (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE);
}

void SensorManager::_unlockI2C() {
    if (_i2cMutex != NULL) {
        xSemaphoreGive(_i2cMutex);
    }
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================
bool SensorManager::begin() {
    if (!_lockI2C()) {
        DEBUG_PRINTLN("[SensorManager] Erro: I2C ocupado ao iniciar!");
        return false;
    }

    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores PION (v6.1.0)...");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
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

    // Restaura baseline fora do lock
    if (_ccs811.isOnline()) {
        restoreCCS811Baseline();
    }
    
    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", _sensorCount);
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    return (_sensorCount > 0);
}

// ============================================================================
// ATUALIZAÇÕES (LOOPS)
// ============================================================================

void SensorManager::updateFast() {
    if (_lockI2C()) {
        _mpu9250.update();
        _bmp280.update();
        _updateTemperatureRedundancy();
        _unlockI2C();
    }
}

void SensorManager::updateSlow() {
    if (_lockI2C()) {
        // --- ATUALIZAÇÃO PADRÃO (Sem troca de clock dinâmica) ---
        // Mantemos a estabilidade em 100kHz (configurado na Main)
        
        _si7021.update();
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

    // Monitoramento de Falha Global
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

// ============================================================================
// HEALTH CHECK & RESET (COM RECUPERAÇÃO FÍSICA)
// ============================================================================

void SensorManager::_performHealthCheck() {
    // 1. Verifica se o CCS811 morreu (Erro 263 constante)
    // Se ele estiver marcado como offline mas deveria estar online, ou falhando muito
    static uint8_t ccsDeadCount = 0;
    
    if (_ccs811.isOnline()) {
        // Se estiver online, zera contador
        ccsDeadCount = 0;
    } else {
        // Se estiver offline, conta falhas
        ccsDeadCount++;
        if (ccsDeadCount >= 3) {
            DEBUG_PRINTLN("[SensorManager] ⚠ CCS811 TRAVADO! Iniciando Reset Físico do Barramento...");
            resetAll(); // Aciona a "Bomba Atômica" de reset
            ccsDeadCount = 0;
            return; // Sai após resetar tudo
        }
    }

    // 2. Reset Global por falha total
    if (_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        DEBUG_PRINTLN("[SensorManager] Falha crítica global: Resetando todos os sensores...");
        resetAll(); 
        return;
    }
    
    // 3. Recuperação Individual (Leve)
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
        
        // 1. Desliga o periférico I2C do ESP32
        Wire.end();
        delay(50);
        
        // 2. Bit-Banging para destravar SDA (Técnica de 9 Clocks)
        // Isso solta sensores que ficaram travados no meio de um byte
        pinMode(SENSOR_I2C_SDA, OUTPUT);
        pinMode(SENSOR_I2C_SCL, OUTPUT);
        digitalWrite(SENSOR_I2C_SDA, HIGH); // Solta dados
        digitalWrite(SENSOR_I2C_SCL, HIGH);
        
        for(int i=0; i<9; i++) {
            digitalWrite(SENSOR_I2C_SCL, LOW);
            delayMicroseconds(10);
            digitalWrite(SENSOR_I2C_SCL, HIGH);
            delayMicroseconds(10);
        }
        
        // Gera um sinal de STOP manual
        digitalWrite(SENSOR_I2C_SDA, LOW);
        delayMicroseconds(10);
        digitalWrite(SENSOR_I2C_SCL, HIGH);
        delayMicroseconds(10);
        digitalWrite(SENSOR_I2C_SDA, HIGH);
        delay(50);
        
        // 3. Reinicia o Wire com Configuração Segura
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(100000); // 100kHz
        Wire.setTimeOut(1000); // 1000ms Timeout
        
        DEBUG_PRINTLN("[SensorManager] Barramento reiniciado. Reconfigurando sensores...");
        
        // 4. Reinicia Drivers
        _mpu9250.reset();
        _bmp280.forceReinit();
        _si7021.reset();
        _ccs811.reset();
        
        _unlockI2C();
    }
    _consecutiveFailures = 0;
    _temperature = NAN;
    delay(500);
}

// ============================================================================
// CALIBRAÇÃO E UTILS (Mantidos iguais)
// ============================================================================
bool SensorManager::recalibrateMagnetometer() {
    if (!_mpu9250.isOnline() || !_mpu9250.isMagOnline()) return false;
    DEBUG_PRINTLN("[SensorManager] INICIANDO RECALIBRAÇÃO... (3s)");
    delay(3000);
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
    DEBUG_PRINTLN("[SensorManager] --- Calibração Magnetômetro ---");
    float x, y, z;
    _mpu9250.getMagOffsets(x, y, z); 
    DEBUG_PRINTF("Offsets: %.2f, %.2f, %.2f\n", x, y, z);
}

void SensorManager::getMagnetometerOffsets(float& x, float& y, float& z) const {
    _mpu9250.getMagOffsets(x, y, z);
}

bool SensorManager::applyCCS811EnvironmentalCompensation(float temperature, float humidity) {
    bool res = false;
    if (_lockI2C()) {
        if (_ccs811.isOnline()) {
            res = _ccs811.setEnvironmentalData(humidity, temperature);
        }
        _unlockI2C();
    }
    return res;
}

void SensorManager::_autoApplyEnvironmentalCompensation() {
    unsigned long now = millis();
    if (now - _lastEnvCompensation < ENV_COMPENSATION_INTERVAL) return;
    _lastEnvCompensation = now;

    if (!_ccs811.isOnline()) return;

    float temp = (!isnan(_temperature)) ? _temperature : 25.0f;
    float hum = (_si7021.isOnline()) ? _si7021.getHumidity() : 50.0f;

    _ccs811.setEnvironmentalData(hum, temp);
    DEBUG_PRINTF("[SensorManager] Comp. Amb. Automática: T=%.1f H=%.1f\n", temp, hum);
}

bool SensorManager::saveCCS811Baseline() {
    uint16_t baseline = 0;
    bool success = false;
    if (_lockI2C()) {
        if (_ccs811.isOnline()) success = _ccs811.getBaseline(baseline);
        _unlockI2C();
    }
    if (success) {
        Preferences prefs;
        if (prefs.begin("ccs811", false)) {
            prefs.putUShort("baseline", baseline);
            prefs.putUInt("valid", 0xCAFEBABE);
            prefs.end();
            return true;
        }
    }
    return false;
}

bool SensorManager::restoreCCS811Baseline() {
    Preferences prefs;
    if (!prefs.begin("ccs811", true)) return false;
    uint32_t magic = prefs.getUInt("valid", 0);
    uint16_t baseline = prefs.getUShort("baseline", 0);
    prefs.end();
    if (magic != 0xCAFEBABE || baseline == 0) return false;
    
    bool res = false;
    if (_lockI2C()) {
        if (_ccs811.isOnline()) res = _ccs811.setBaseline(baseline);
        _unlockI2C();
    }
    return res;
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
    DEBUG_PRINTF("CCS811:  %s (eCO2: %d)\n", _ccs811.isOnline() ? "ONLINE" : "OFFLINE", _ccs811.geteCO2());
    DEBUG_PRINTF("Temp Final: %.2f C\n", _temperature);
    DEBUG_PRINTLN("----------------------------------------");
}