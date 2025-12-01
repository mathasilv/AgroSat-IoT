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
    : _mpu9250(MPU9250_ADDRESS),
      _sensorCount(0),
      _lastEnvCompensation(0)
{
}


// ============================================================================
// INICIALIZAÇÃO - TODOS OS SENSORES
// ============================================================================
bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores PION (v6.0.0)...");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    _sensorCount = 0;
    
    // MPU9250 (já carrega calibração automaticamente no begin)
    if (_mpu9250.begin()) {
        _sensorCount++;
        DEBUG_PRINTLN("[SensorManager] MPU9250Manager: ONLINE (9-axis)");
        
        if (_mpu9250.isCalibrated()) {
            DEBUG_PRINTLN("[SensorManager]   Magnetômetro: CALIBRADO ✓");
        } else {
            DEBUG_PRINTLN("[SensorManager]   Magnetômetro: SEM CALIBRAÇÃO ⚠");
        }
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
        
        // Tentar restaurar baseline salvo
        if (restoreCCS811Baseline()) {
            DEBUG_PRINTLN("[SensorManager]   Baseline do CCS811 restaurado");
        }
    }
    
    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", _sensorCount);
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    return (_sensorCount > 0);
}

bool SensorManager::recalibrateMagnetometer() {
    if (!_mpu9250.isOnline()) {
        DEBUG_PRINTLN("[SensorManager] MPU9250 offline - não é possível calibrar");
        return false;
    }
    
    if (!_mpu9250.isMagOnline()) {
        DEBUG_PRINTLN("[SensorManager] Magnetômetro offline - não é possível calibrar");
        return false;
    }
    
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] INICIANDO RECALIBRAÇÃO DO MAGNETÔMETRO");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] INSTRUÇÕES:");
    DEBUG_PRINTLN("[SensorManager] 1. Rotacione LENTAMENTE o dispositivo");
    DEBUG_PRINTLN("[SensorManager] 2. Faça movimentos AMPLOS em forma de 8");
    DEBUG_PRINTLN("[SensorManager] 3. Cubra TODOS os eixos (X, Y e Z)");
    DEBUG_PRINTLN("[SensorManager] 4. Duração: 10-30 segundos");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] Aguarde 3 segundos para começar...");
    delay(3000);
    
    // Executar calibração (já salva automaticamente se bem-sucedida)
    bool success = _mpu9250.calibrateMagnetometer();
    
    if (success) {
        DEBUG_PRINTLN("[SensorManager] ========================================");
        DEBUG_PRINTLN("[SensorManager] RECALIBRAÇÃO CONCLUÍDA COM SUCESSO!");
        DEBUG_PRINTLN("[SensorManager] ========================================");
        
        float x, y, z;
        _mpu9250.getMagOffsets(x, y, z);
        DEBUG_PRINTF("[SensorManager] Novos offsets: X=%.2f Y=%.2f Z=%.2f µT\n", x, y, z);
    } else {
        DEBUG_PRINTLN("[SensorManager] ========================================");
        DEBUG_PRINTLN("[SensorManager]  RECALIBRAÇÃO FALHOU");
        DEBUG_PRINTLN("[SensorManager] ========================================");
    }
    
    return success;
}

/**
 * @brief Limpa calibração salva do magnetômetro (força recalibração no boot)
 */
void SensorManager::clearMagnetometerCalibration() {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] LIMPANDO CALIBRAÇÃO DO MAGNETÔMETRO");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    _mpu9250.clearOffsetsFromMemory();
    
    DEBUG_PRINTLN("[SensorManager] Calibração removida da memória");
    DEBUG_PRINTLN("[SensorManager] Será necessária nova calibração no próximo boot");
    DEBUG_PRINTLN("[SensorManager] ========================================");
}

/**
 * @brief Mostra offsets salvos do magnetômetro
 */
void SensorManager::printMagnetometerCalibration() const {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] CALIBRAÇÃO DO MAGNETÔMETRO");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    if (_mpu9250.isOnline()) {
        // Offsets atuais (em uso)
        float x, y, z;
        _mpu9250.getMagOffsets(x, y, z);
        
        DEBUG_PRINTLN("[SensorManager] Offsets ATUAIS (em uso):");
        DEBUG_PRINTF("[SensorManager]   X = %.2f µT\n", x);
        DEBUG_PRINTF("[SensorManager]   Y = %.2f µT\n", y);
        DEBUG_PRINTF("[SensorManager]   Z = %.2f µT\n", z);
        
        float magnitude = sqrt(x*x + y*y + z*z);
        DEBUG_PRINTF("[SensorManager]   Magnitude = %.1f µT\n", magnitude);
        DEBUG_PRINTLN("");
        
        // Offsets salvos na memória
        DEBUG_PRINTLN("[SensorManager] Offsets SALVOS na memória:");
        _mpu9250.printSavedOffsets();
        
        // Status
        if (_mpu9250.isCalibrated()) {
            DEBUG_PRINTLN("[SensorManager] Status: CALIBRADO ✓");
        } else {
            DEBUG_PRINTLN("[SensorManager] Status: NÃO CALIBRADO ⚠");
        }
    } else {
        DEBUG_PRINTLN("[SensorManager] MPU9250 offline");
    }
    
    DEBUG_PRINTLN("[SensorManager] ========================================");
}


// Atualizações rápidas (pode ser 5–10 Hz)
void SensorManager::updateFast() {
    // Antes você chamava tudo aqui dentro de update()
    _mpu9250.update();
    _bmp280.update();

    // Redundância de temperatura usando SI7021/BMP280
    _updateTemperatureRedundancy();
}

// Atualizações lentas (pode ser ~1 Hz)
void SensorManager::updateSlow() {
    _si7021.update();
    _ccs811.update();

    // Compensação ambiental do CCS811 a cada 60 s
    _autoApplyEnvironmentalCompensation();
}

// Só health check / recuperação
void SensorManager::updateHealth() {
    uint32_t currentTime = millis();

    // Health check a cada 30s
    if (currentTime - _lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }

    // Contar falhas consecutivas
    bool anyOnline = _mpu9250.isOnline() ||
                     _bmp280.isOnline()  ||
                     _si7021.isOnline()  ||
                     _ccs811.isOnline();

    if (!anyOnline) {
        _consecutiveFailures++;
    } else {
        _consecutiveFailures = 0;
    }
}

// Wrapper antigo (se ainda for usado em algum lugar)
void SensorManager::update() {
    updateFast();
    updateSlow();
    updateHealth();
}

// ============================================================================
// REDUNDÂNCIA TEMPERATURA (Prioridade: SI7021 > BMP280)
// ============================================================================
void SensorManager::_updateTemperatureRedundancy() {
    // Prioridade 1: SI7021 (mais preciso)
    if (_si7021.isOnline() && _si7021.isTempValid()) {
        _temperature = _si7021.getTemperature();
        return;
    }
    
    // Fallback: BMP280
    if (_bmp280.isOnline() && _bmp280.isTempValid()) {
        _temperature = _bmp280.getTemperature();
        return;
    }
    
    // Nenhum sensor disponí vel
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
    if (!_mpu9250.isOnline() && _mpu9250.getFailCount() >= 5) {
        DEBUG_PRINTLN("[SensorManager] Recuperando MPU9250...");
        _mpu9250.reset();
    }
    
    if (!_bmp280.isOnline() && _bmp280.getFailCount() >= 5) {
        DEBUG_PRINTLN("[SensorManager] Recuperando BMP280...");
        _bmp280.forceReinit();
    }
}

// ============================================================================
// RESET TOTAL
// ============================================================================
void SensorManager::resetAll() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando todos os sensores...");
    
    _mpu9250.reset();
    _bmp280.forceReinit();
    _si7021.reset();
    _ccs811.reset();
    
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
    _mpu9250.getRawData(gx, gy, gz, ax, ay, az, mx, my, mz);
}

void SensorManager::printSensorStatus() const {
    DEBUG_PRINTLN("========== STATUS DOS SENSORES ==========");
    
    _mpu9250.printStatus();
    _bmp280.printStatus();
    
    if (_si7021.isOnline()) {
        DEBUG_PRINTF(" SI7021: ONLINE");
        DEBUG_PRINTF(" (T=%.1f°C H=%.1f%%)", 
                     _si7021.getTemperature(),
                     _si7021.getHumidity());
        DEBUG_PRINTLN();
    } else {
        DEBUG_PRINTLN(" SI7021: OFFLINE");
    }
    
    _ccs811.printStatus();
    
    // Status de redundância
    DEBUG_PRINTLN("Redundância de Temperatura:");
    if (!isnan(_temperature)) {
        DEBUG_PRINTF("  Usando: %.2f°C (%s)\n",
                     _temperature,
                     _si7021.isTempValid() ? "SI7021" : "BMP280");
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

/**
 * @brief Obtém offsets atuais do magnetômetro
 */
void SensorManager::getMagnetometerOffsets(float& x, float& y, float& z) const {
    _mpu9250.getMagOffsets(x, y, z);
}

/**
 * @brief Aplica compensação ambiental automaticamente no CCS811
 * Executado periodicamente no update() a cada 60 segundos
 */
void SensorManager::_autoApplyEnvironmentalCompensation() {
    // Verificar se CCS811 está online
    if (!_ccs811.isOnline()) {
        return;
    }
    
    // Verificar intervalo (60 segundos)
    unsigned long now = millis();
    if (now - _lastEnvCompensation < ENV_COMPENSATION_INTERVAL) {
        return;
    }
    
    _lastEnvCompensation = now;
    
    // Obter temperatura
    float temp = 0.0f;
    if (_bmp280.isOnline()) {
        temp = _bmp280.getTemperature();
    } else if (_si7021.isOnline()) {
        temp = _si7021.getTemperature();
    } else {
        return;  // Nenhum sensor de temperatura disponível
    }
    
    // Obter umidade (se disponível)
    float hum = 50.0f;  // Valor padrão
    if (_si7021.isOnline()) {
        hum = _si7021.getHumidity();
    }
    
    // Aplicar compensação
    if (_ccs811.setEnvironmentalData(hum, temp)) {
        DEBUG_PRINTF("[SensorManager] Compensação ambiental aplicada: T=%.1f°C RH=%.1f%%\n", 
                    temp, hum);
    }
}

/**
 * @brief Aplica compensação ambiental manualmente no CCS811
 */
bool SensorManager::applyCCS811EnvironmentalCompensation(float temperature, float humidity) {
    if (!_ccs811.isOnline()) {
        DEBUG_PRINTLN("[SensorManager] CCS811 offline");
        return false;
    }
    
    return _ccs811.setEnvironmentalData(humidity, temperature);
}

/**
 * @brief Salva baseline do CCS811 (após 48h de operação)
 */
bool SensorManager::saveCCS811Baseline() {
    if (!_ccs811.isOnline()) {
        DEBUG_PRINTLN("[SensorManager] CCS811 offline");
        return false;
    }
    
    uint16_t baseline;
    if (!_ccs811.getBaseline(baseline)) {
        DEBUG_PRINTLN("[SensorManager] Falha ao obter baseline");
        return false;
    }
    
    // Salvar baseline usando Preferences
    Preferences prefs;
    if (!prefs.begin("ccs811", false)) {
        DEBUG_PRINTLN("[SensorManager] Falha ao abrir Preferences");
        return false;
    }
    
    prefs.putUShort("baseline", baseline);
    prefs.putUInt("valid", 0xCAFEBABE);  // Magic number
    prefs.end();
    
    DEBUG_PRINTF("[SensorManager] Baseline do CCS811 salvo: 0x%04X\n", baseline);
    return true;
}

/**
 * @brief Restaura baseline do CCS811
 */
bool SensorManager::restoreCCS811Baseline() {
    if (!_ccs811.isOnline()) {
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin("ccs811", true)) {
        return false;
    }
    
    uint32_t magic = prefs.getUInt("valid", 0);
    if (magic != 0xCAFEBABE) {
        prefs.end();
        return false;
    }
    
    uint16_t baseline = prefs.getUShort("baseline", 0);
    prefs.end();
    
    if (baseline == 0) {
        return false;
    }
    
    if (_ccs811.setBaseline(baseline)) {
        DEBUG_PRINTF("[SensorManager] Baseline do CCS811 restaurado: 0x%04X\n", baseline);
        return true;
    }
    
    return false;
}

/**
 * @brief Mostra status detalhado com informações de calibração
 */
void SensorManager::printDetailedStatus() const {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] STATUS DETALHADO DOS SENSORES");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    // MPU9250
    DEBUG_PRINTLN("[SensorManager] MPU9250 (9-DOF IMU):");
    if (_mpu9250.isOnline()) {
        DEBUG_PRINTLN("[SensorManager]   Status: ONLINE ✓");
        DEBUG_PRINTF("[SensorManager]   Modo: %s\n", 
                    _mpu9250.isMagOnline() ? "9-DOF" : "6-DOF");
        
        if (_mpu9250.isMagOnline()) {
            if (_mpu9250.isCalibrated()) {
                float x, y, z;
                _mpu9250.getMagOffsets(x, y, z);
                DEBUG_PRINTLN("[SensorManager]   Magnetômetro: CALIBRADO ✓");
                DEBUG_PRINTF("[SensorManager]     Offsets: X=%.2f Y=%.2f Z=%.2f µT\n", x, y, z);
            } else {
                DEBUG_PRINTLN("[SensorManager]   Magnetômetro: NÃO CALIBRADO ⚠");
            }
        }
    } else {
        DEBUG_PRINTLN("[SensorManager]   Status: OFFLINE ✗");
    }
    DEBUG_PRINTLN("");
    
    // BMP280
    DEBUG_PRINTLN("[SensorManager] BMP280 (Pressão/Temperatura):");
    if (_bmp280.isOnline()) {
        DEBUG_PRINTLN("[SensorManager]   Status: ONLINE ✓");
        DEBUG_PRINTF("[SensorManager]   Temperatura: %.1f°C\n", _bmp280.getTemperature());
        DEBUG_PRINTF("[SensorManager]   Pressão: %.0f hPa\n", _bmp280.getPressure());
    } else {
        DEBUG_PRINTLN("[SensorManager]   Status: OFFLINE ✗");
    }
    DEBUG_PRINTLN("");
    
    // SI7021
    DEBUG_PRINTLN("[SensorManager] SI7021 (Umidade/Temperatura):");
    if (_si7021.isOnline()) {
        DEBUG_PRINTLN("[SensorManager]   Status: ONLINE ✓");
        DEBUG_PRINTF("[SensorManager]   Umidade: %.1f%%\n", _si7021.getHumidity());
        DEBUG_PRINTF("[SensorManager]   Temperatura: %.1f°C\n", _si7021.getTemperature());
    } else {
        DEBUG_PRINTLN("[SensorManager]   Status: OFFLINE ✗");
    }
    DEBUG_PRINTLN("");
    
    // CCS811
    DEBUG_PRINTLN("[SensorManager] CCS811 (Qualidade do Ar):");
    if (_ccs811.isOnline()) {
        DEBUG_PRINTLN("[SensorManager]   Status: ONLINE ✓");
        DEBUG_PRINTF("[SensorManager]   eCO2: %d ppm\n", _ccs811.geteCO2());
        DEBUG_PRINTF("[SensorManager]   TVOC: %d ppb\n", _ccs811.getTVOC());
        DEBUG_PRINTF("[SensorManager]   Warm-up: %lu%%\n", _ccs811.getWarmupProgress());
        
        if (_ccs811.isDataReliable()) {
            DEBUG_PRINTLN("[SensorManager]   Dados: CONFIÁVEIS ✓");
        } else {
            DEBUG_PRINTLN("[SensorManager]   Dados: EM WARM-UP ⏳");
        }
    } else {
        DEBUG_PRINTLN("[SensorManager]   Status: OFFLINE ✗");
    }
    
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTF("[SensorManager] Total: %d/4 sensores online\n", getOnlineSensors());
    DEBUG_PRINTLN("[SensorManager] ========================================");
}

/**
 * @brief Retorna número de sensores online
 * @return Contagem de sensores ativos
 */
uint8_t SensorManager::getOnlineSensors() const {
    uint8_t count = 0;
    
    if (_mpu9250.isOnline()) count++;
    if (_bmp280.isOnline()) count++;
    if (_si7021.isOnline()) count++;
    if (_ccs811.isOnline()) count++;
    
    return count;
}
