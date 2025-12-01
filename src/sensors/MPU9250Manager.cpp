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
    DEBUG_PRINTLN("[MPU9250Manager] Inicializando MPU9250 9-Axis IMU");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    _online = false;
    _magOnline = false;
    _calibrated = false;
    _failCount = 0;
    _filterIndex = 0;
    _sumAccelX = _sumAccelY = _sumAccelZ = 0.0f;
    
    // Inicializar buffers do filtro
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0f;
        _accelYBuffer[i] = 0.0f;
        _accelZBuffer[i] = 0.0f;
    }
    
    // PASSO 1: Verificar presença I2C
    DEBUG_PRINTF("[MPU9250Manager] PASSO 1: Detectando MPU9250 em 0x%02X...\n", _addr);
    
    Wire.beginTransmission(_addr);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        DEBUG_PRINTF("[MPU9250Manager] ❌ FALHA: MPU9250 não detectado (erro I2C: %d)\n", error);
        return false;
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ✓ MPU9250 detectado no barramento I2C");
    
    // PASSO 2: Inicializar MPU9250 (Acelerômetro + Giroscópio)
    DEBUG_PRINTLN("[MPU9250Manager] PASSO 2: Inicializando IMU (Acc + Gyro)...");
    
    if (!_initMPU()) {
        DEBUG_PRINTLN("[MPU9250Manager] ❌ FALHA: Inicialização IMU");
        return false;
    }
    
    _online = true;
    DEBUG_PRINTLN("[MPU9250Manager] ✓ IMU (Acc + Gyro): ONLINE");
    
    // PASSO 3: Ativar I2C bypass para acessar AK8963
    DEBUG_PRINTLN("[MPU9250Manager] PASSO 3: Ativando I2C bypass para magnetômetro...");
    
    if (!_enableI2CBypass()) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ AVISO: Falha ao ativar I2C bypass");
        DEBUG_PRINTLN("[MPU9250Manager]   Magnetômetro pode não funcionar");
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] ✓ I2C bypass ativado");
    }
    
    // PASSO 4: Inicializar Magnetômetro AK8963
    DEBUG_PRINTLN("[MPU9250Manager] PASSO 4: Inicializando magnetômetro AK8963...");
    
    if (_initMagnetometer()) {
        _magOnline = true;
        DEBUG_PRINTLN("[MPU9250Manager] ✓ Magnetômetro AK8963: ONLINE");
        
        // ========================================
        // NOVO: PASSO 5 - Tentar carregar offsets salvos
        // ========================================
        DEBUG_PRINTLN("[MPU9250Manager] PASSO 5: Verificando calibração salva...");
        
        if (hasValidOffsetsInMemory()) {
            DEBUG_PRINTLN("[MPU9250Manager] ✓ Calibração salva encontrada!");
            
            if (loadOffsetsFromMemory()) {
                _calibrated = true;
                DEBUG_PRINTLN("[MPU9250Manager] ✓ Offsets carregados da memória!");
                DEBUG_PRINTF("[MPU9250Manager]   X = %.2f µT\n", _magOffsetX);
                DEBUG_PRINTF("[MPU9250Manager]   Y = %.2f µT\n", _magOffsetY);
                DEBUG_PRINTF("[MPU9250Manager]   Z = %.2f µT\n", _magOffsetZ);
                DEBUG_PRINTLN("[MPU9250Manager] ========================================");
                DEBUG_PRINTLN("[MPU9250Manager] Magnetômetro: CALIBRADO (dados salvos)");
                DEBUG_PRINTLN("[MPU9250Manager] Para recalibrar, use calibrateMagnetometer()");
            } else {
                DEBUG_PRINTLN("[MPU9250Manager] ⚠ Falha ao carregar offsets");
                DEBUG_PRINTLN("[MPU9250Manager] Será necessária nova calibração");
            }
        } else {
            DEBUG_PRINTLN("[MPU9250Manager] ℹ Nenhuma calibração salva encontrada");
            DEBUG_PRINTLN("[MPU9250Manager] PASSO 5: Iniciando calibração automática...");
            DEBUG_PRINTLN("[MPU9250Manager]   ATENÇÃO: Rotacione o dispositivo em todos os eixos");
            
            if (calibrateMagnetometer()) {
                _calibrated = true;
                
                // Salvar automaticamente após calibração bem-sucedida
                DEBUG_PRINTLN("[MPU9250Manager] ========================================");
                DEBUG_PRINTLN("[MPU9250Manager] Salvando calibração na memória...");
                
                if (saveOffsetsToMemory()) {
                    DEBUG_PRINTLN("[MPU9250Manager] ✅ Calibração salva com sucesso!");
                    DEBUG_PRINTLN("[MPU9250Manager]   Será carregada automaticamente no próximo boot");
                } else {
                    DEBUG_PRINTLN("[MPU9250Manager] ⚠ Falha ao salvar calibração");
                    DEBUG_PRINTLN("[MPU9250Manager]   Será necessário recalibrar após reboot");
                }
            } else {
                DEBUG_PRINTLN("[MPU9250Manager] ⚠ Calibração falhou - usando offsets zerados");
            }
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ AVISO: Magnetômetro não inicializado");
        DEBUG_PRINTLN("[MPU9250Manager]   Sistema continuará com 6-DOF apenas");
    }
    
    // PASSO 6: Leitura de teste
    DEBUG_PRINTLN("[MPU9250Manager] PASSO 6: Leitura de teste...");
    delay(100);
    
    xyzFloat testAcc = _mpu.getGValues();
    xyzFloat testGyro = _mpu.getGyrValues();
    
    DEBUG_PRINTF("[MPU9250Manager]   Accel: X=%.2fg Y=%.2fg Z=%.2fg\n", 
                 testAcc.x, testAcc.y, testAcc.z);
    DEBUG_PRINTF("[MPU9250Manager]   Gyro: X=%.1f°/s Y=%.1f°/s Z=%.1f°/s\n", 
                 testGyro.x, testGyro.y, testGyro.z);
    
    if (_magOnline) {
        xyzFloat testMag = _mpu.getMagValues();
        DEBUG_PRINTF("[MPU9250Manager]   Mag (raw): X=%.1fµT Y=%.1fµT Z=%.1fµT\n", 
                     testMag.x, testMag.y, testMag.z);
        
        if (_calibrated) {
            DEBUG_PRINTF("[MPU9250Manager]   Mag (calibrado): X=%.1fµT Y=%.1fµT Z=%.1fµT\n", 
                         testMag.x - _magOffsetX, 
                         testMag.y - _magOffsetY, 
                         testMag.z - _magOffsetZ);
        }
    }
    
    // SUCESSO!
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] ✅ MPU9250 INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTF("[MPU9250Manager] Modo: %s\n", _magOnline ? "9-DOF" : "6-DOF");
    
    if (_magOnline) {
        if (_calibrated) {
            DEBUG_PRINTLN("[MPU9250Manager] Magnetômetro: CALIBRADO ✓");
        } else {
            DEBUG_PRINTLN("[MPU9250Manager] Magnetômetro: SEM CALIBRAÇÃO ⚠");
            DEBUG_PRINTLN("[MPU9250Manager]   Execute calibrateMagnetometer() para calibrar");
        }
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    return true;
}

bool MPU9250Manager::_enableI2CBypass() {
    // Registrador INT_PIN_CFG (0x37)
    const uint8_t INT_PIN_CFG = 0x37;
    const uint8_t BYPASS_EN = 0x02;  // Bit 1
    
    // Ler valor atual
    Wire.beginTransmission(_addr);
    Wire.write(INT_PIN_CFG);
    if (Wire.endTransmission(false) != 0) {
        DEBUG_PRINTLN("[MPU9250Manager]   Erro ao acessar INT_PIN_CFG");
        return false;
    }
    
    Wire.requestFrom(_addr, (uint8_t)1);
    if (!Wire.available()) {
        DEBUG_PRINTLN("[MPU9250Manager]   Erro ao ler INT_PIN_CFG");
        return false;
    }
    
    uint8_t currentValue = Wire.read();
    DEBUG_PRINTF("[MPU9250Manager]   INT_PIN_CFG atual: 0x%02X\n", currentValue);
    
    // Ativar BYPASS_EN (bit 1)
    uint8_t newValue = currentValue | BYPASS_EN;
    
    Wire.beginTransmission(_addr);
    Wire.write(INT_PIN_CFG);
    Wire.write(newValue);
    
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("[MPU9250Manager]   Erro ao escrever INT_PIN_CFG");
        return false;
    }
    
    // Verificar se foi escrito corretamente
    delay(10);
    Wire.beginTransmission(_addr);
    Wire.write(INT_PIN_CFG);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1);
    
    if (Wire.available()) {
        uint8_t verifyValue = Wire.read();
        DEBUG_PRINTF("[MPU9250Manager]   INT_PIN_CFG novo: 0x%02X\n", verifyValue);
        
        if ((verifyValue & BYPASS_EN) == 0) {
            DEBUG_PRINTLN("[MPU9250Manager]   Falha: BYPASS_EN não foi ativado");
            return false;
        }
    }
    
    DEBUG_PRINTLN("[MPU9250Manager]   ✓ I2C bypass ativado com sucesso");
    return true;
}

bool MPU9250Manager::saveOffsetsToMemory() {
    DEBUG_PRINTLN("[MPU9250Manager] Salvando offsets na memória...");
    
    // Validar offsets antes de salvar
    if (!_validateOffsets(_magOffsetX, _magOffsetY, _magOffsetZ)) {
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Offsets inválidos - não salvando");
        return false;
    }
    
    // Abrir namespace de preferências
    if (!_prefs.begin(PREFS_NAMESPACE, false)) {  // false = read/write
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Falha ao abrir Preferences");
        return false;
    }
    
    // Salvar magic number (validação)
    _prefs.putUInt(KEY_VALID, OFFSET_MAGIC);
    
    // Salvar offsets
    _prefs.putFloat(KEY_OFFSET_X, _magOffsetX);
    _prefs.putFloat(KEY_OFFSET_Y, _magOffsetY);
    _prefs.putFloat(KEY_OFFSET_Z, _magOffsetZ);
    
    _prefs.end();
    
    DEBUG_PRINTLN("[MPU9250Manager] ✓ Offsets salvos:");
    DEBUG_PRINTF("[MPU9250Manager]   X = %.2f µT\n", _magOffsetX);
    DEBUG_PRINTF("[MPU9250Manager]   Y = %.2f µT\n", _magOffsetY);
    DEBUG_PRINTF("[MPU9250Manager]   Z = %.2f µT\n", _magOffsetZ);
    
    return true;
}

bool MPU9250Manager::loadOffsetsFromMemory() {
    DEBUG_PRINTLN("[MPU9250Manager] Carregando offsets da memória...");
    
    // Abrir namespace de preferências
    if (!_prefs.begin(PREFS_NAMESPACE, true)) {  // true = read-only
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Falha ao abrir Preferences");
        return false;
    }
    
    // Verificar se há dados válidos
    uint32_t magic = _prefs.getUInt(KEY_VALID, 0);
    if (magic != OFFSET_MAGIC) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ Magic number inválido - dados corrompidos");
        _prefs.end();
        return false;
    }
    
    // Carregar offsets
    float x = _prefs.getFloat(KEY_OFFSET_X, 0.0f);
    float y = _prefs.getFloat(KEY_OFFSET_Y, 0.0f);
    float z = _prefs.getFloat(KEY_OFFSET_Z, 0.0f);
    
    _prefs.end();
    
    // Validar offsets carregados
    if (!_validateOffsets(x, y, z)) {
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Offsets carregados são inválidos");
        return false;
    }
    
    // Aplicar offsets
    _magOffsetX = x;
    _magOffsetY = y;
    _magOffsetZ = z;
    
    DEBUG_PRINTLN("[MPU9250Manager] ✓ Offsets carregados:");
    DEBUG_PRINTF("[MPU9250Manager]   X = %.2f µT\n", _magOffsetX);
    DEBUG_PRINTF("[MPU9250Manager]   Y = %.2f µT\n", _magOffsetY);
    DEBUG_PRINTF("[MPU9250Manager]   Z = %.2f µT\n", _magOffsetZ);
    
    return true;
}

bool MPU9250Manager::_validateOffsets(float x, float y, float z) const {
    // 1. Verificar NAN
    if (isnan(x) || isnan(y) || isnan(z)) {
        DEBUG_PRINTLN("[MPU9250Manager]   Validação: NAN detectado");
        return false;
    }
    
    // 2. Verificar range individual (±500 µT é razoável para hard iron)
    const float MAX_OFFSET = 500.0f;  // µT
    if (fabs(x) > MAX_OFFSET || fabs(y) > MAX_OFFSET || fabs(z) > MAX_OFFSET) {
        DEBUG_PRINTF("[MPU9250Manager]   Validação: Offset fora do range: %.1f, %.1f, %.1f µT\n", 
                     x, y, z);
        return false;
    }
    
    // 3. Verificar magnitude total (hard iron excessivo indica problema)
    float magnitude = sqrt(x*x + y*y + z*z);
    const float MAX_MAGNITUDE = 300.0f;  // µT
    
    if (magnitude > MAX_MAGNITUDE) {
        DEBUG_PRINTF("[MPU9250Manager]   Validação: Magnitude excessiva: %.1f µT (max: %.1f)\n", 
                     magnitude, MAX_MAGNITUDE);
        return false;
    }
    
    // 4. Verificar se não está zerado (indica calibração não realizada)
    if (magnitude < 1.0f) {
        DEBUG_PRINTLN("[MPU9250Manager]   Validação: Offsets zerados (não calibrado)");
        return false;
    }
    
    DEBUG_PRINTF("[MPU9250Manager]   Validação OK: Magnitude = %.1f µT\n", magnitude);
    return true;
}

void MPU9250Manager::printSavedOffsets() const {
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] OFFSETS SALVOS NA MEMÓRIA");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) {
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Falha ao abrir Preferences");
        return;
    }
    
    uint32_t magic = prefs.getUInt(KEY_VALID, 0);
    
    if (magic != OFFSET_MAGIC) {
        DEBUG_PRINTLN("[MPU9250Manager] Nenhuma calibração salva");
        prefs.end();
        return;
    }
    
    float x = prefs.getFloat(KEY_OFFSET_X, 0.0f);
    float y = prefs.getFloat(KEY_OFFSET_Y, 0.0f);
    float z = prefs.getFloat(KEY_OFFSET_Z, 0.0f);
    
    prefs.end();
    
    DEBUG_PRINTF("[MPU9250Manager] X = %.2f µT\n", x);
    DEBUG_PRINTF("[MPU9250Manager] Y = %.2f µT\n", y);
    DEBUG_PRINTF("[MPU9250Manager] Z = %.2f µT\n", z);
    
    float magnitude = sqrt(x*x + y*y + z*z);
    DEBUG_PRINTF("[MPU9250Manager] Magnitude = %.1f µT\n", magnitude);
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
}

bool MPU9250Manager::hasValidOffsetsInMemory() {
    if (!_prefs.begin(PREFS_NAMESPACE, true)) {  // true = read-only
        return false;
    }
    
    uint32_t magic = _prefs.getUInt(KEY_VALID, 0);
    _prefs.end();
    
    return (magic == OFFSET_MAGIC);
}

void MPU9250Manager::clearOffsetsFromMemory() {
    DEBUG_PRINTLN("[MPU9250Manager] Limpando offsets da memória...");
    
    if (!_prefs.begin(PREFS_NAMESPACE, false)) {
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Falha ao abrir Preferences");
        return;
    }
    
    _prefs.clear();  // Limpar tudo no namespace
    _prefs.end();
    
    DEBUG_PRINTLN("[MPU9250Manager] ✓ Offsets limpos - será necessária nova calibração");
}

// ============================================================================
// INICIALIZAÇÃO - MPU9250 (ACC + GYRO)
// ============================================================================
bool MPU9250Manager::_initMPU() {
    DEBUG_PRINTLN("[MPU9250Manager]   Chamando _mpu.begin()...");
    
    if (!_mpu.begin()) {
        DEBUG_PRINTLN("[MPU9250Manager]   ❌ _mpu.begin() falhou");
        return false;
    }
    
    DEBUG_PRINTLN("[MPU9250Manager]   ✓ _mpu.begin() OK");
    
    // Validar WHO_AM_I (deve ser 0x71 para MPU9250)
    const uint8_t WHO_AM_I_REG = 0x75;
    const uint8_t MPU9250_WHO_AM_I = 0x71;
    
    Wire.beginTransmission(_addr);
    Wire.write(WHO_AM_I_REG);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1);
    
    if (Wire.available()) {
        uint8_t whoAmI = Wire.read();
        DEBUG_PRINTF("[MPU9250Manager]   WHO_AM_I: 0x%02X (esperado: 0x71)\n", whoAmI);
        
        if (whoAmI != MPU9250_WHO_AM_I) {
            DEBUG_PRINTLN("[MPU9250Manager]   ⚠ AVISO: WHO_AM_I não corresponde a MPU9250");
            DEBUG_PRINTLN("[MPU9250Manager]   Possível MPU6500 ou MPU6050");
        }
    }
    
    // Teste de leitura (3 tentativas)
    DEBUG_PRINTLN("[MPU9250Manager]   Testando leituras...");
    
    for (int i = 0; i < 3; i++) {
        xyzFloat test = _mpu.getGValues();
        
        if (!isnan(test.x) && !isnan(test.y) && !isnan(test.z)) {
            // Validar magnitude razoável (0.5g a 2g em repouso)
            float mag = sqrt(test.x*test.x + test.y*test.y + test.z*test.z);
            
            DEBUG_PRINTF("[MPU9250Manager]   Teste %d: Mag=%.2fg ", i+1, mag);
            
            if (mag >= 0.5f && mag <= 2.0f) {
                DEBUG_PRINTLN("✓ OK");
                return true;
            } else {
                DEBUG_PRINTLN("⚠ Magnitude anormal");
            }
        } else {
            DEBUG_PRINTF("[MPU9250Manager]   Teste %d: ✗ NAN\n", i+1);
        }
        
        delay(50);
    }
    
    DEBUG_PRINTLN("[MPU9250Manager]   ⚠ Leituras com valores suspeitos, mas prosseguindo");
    return true;
}

// ============================================================================
// INICIALIZAÇÃO - MAGNETÔMETRO AK8963
// ============================================================================
bool MPU9250Manager::_initMagnetometer() {
    const uint8_t AK8963_ADDR = 0x0C;
    const uint8_t AK8963_WIA = 0x00;  // WHO_AM_I register
    const uint8_t AK8963_DEVICE_ID = 0x48;
    
    DEBUG_PRINTLN("[MPU9250Manager]   Verificando comunicação com AK8963...");
    
    // Verificar presença no endereço 0x0C
    Wire.beginTransmission(AK8963_ADDR);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        DEBUG_PRINTF("[MPU9250Manager]   ❌ AK8963 não responde em 0x%02X (erro: %d)\n", 
                     AK8963_ADDR, error);
        DEBUG_PRINTLN("[MPU9250Manager]   Verifique se I2C bypass está ativado");
        return false;
    }
    
    DEBUG_PRINTLN("[MPU9250Manager]   ✓ AK8963 responde em 0x0C");
    
    // Ler Device ID
    Wire.beginTransmission(AK8963_ADDR);
    Wire.write(AK8963_WIA);
    Wire.endTransmission(false);
    Wire.requestFrom(AK8963_ADDR, (uint8_t)1);
    
    if (Wire.available()) {
        uint8_t deviceId = Wire.read();
        DEBUG_PRINTF("[MPU9250Manager]   Device ID: 0x%02X (esperado: 0x48)\n", deviceId);
        
        if (deviceId != AK8963_DEVICE_ID) {
            DEBUG_PRINTLN("[MPU9250Manager]   ⚠ Device ID inválido");
            return false;
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager]   ❌ Não foi possível ler Device ID");
        return false;
    }
    
    // Inicializar magnetômetro usando biblioteca
    DEBUG_PRINTLN("[MPU9250Manager]   Chamando _mpu.initMagnetometer()...");
    
    if (!_mpu.initMagnetometer()) {
        DEBUG_PRINTLN("[MPU9250Manager]   ❌ initMagnetometer() falhou");
        return false;
    }
    
    DEBUG_PRINTLN("[MPU9250Manager]   ✓ Magnetômetro inicializado");
    
    // Teste de leitura
    delay(100);
    xyzFloat testMag = _mpu.getMagValues();
    
    if (!isnan(testMag.x) && !isnan(testMag.y) && !isnan(testMag.z)) {
        DEBUG_PRINTF("[MPU9250Manager]   Teste: X=%.1f Y=%.1f Z=%.1f µT\n", 
                     testMag.x, testMag.y, testMag.z);
        
        // Validar range razoável (-200 a +200 µT é normal)
        if (fabs(testMag.x) < 200.0f && fabs(testMag.y) < 200.0f && fabs(testMag.z) < 200.0f) {
            DEBUG_PRINTLN("[MPU9250Manager]   ✓ Leitura OK");
            return true;
        } else {
            DEBUG_PRINTLN("[MPU9250Manager]   ⚠ Valores fora do range esperado");
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager]   ❌ Leitura retornou NAN");
    }
    
    return false;
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
        DEBUG_PRINTLN("[MPU9250Manager] ❌ Magnetômetro offline - calibração cancelada");
        return false;
    }
    
    float magMin[3] = {9999.0f, 9999.0f, 9999.0f};
    float magMax[3] = {-9999.0f, -9999.0f, -9999.0f};
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] CALIBRAÇÃO DO MAGNETÔMETRO");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] INSTRUÇÕES:");
    DEBUG_PRINTLN("[MPU9250Manager] 1. Rotacione lentamente o dispositivo");
    DEBUG_PRINTLN("[MPU9250Manager] 2. Faça movimentos em 8 (figura de oito)");
    DEBUG_PRINTLN("[MPU9250Manager] 3. Cubra todos os eixos (X, Y e Z)");
    DEBUG_PRINTLN("[MPU9250Manager] Duração: 10 segundos");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    uint32_t startTime = millis();
    uint16_t validSamples = 0;
    uint16_t totalSamples = 0;
    uint32_t lastFeedback = 0;
    
    // Loop de calibração (10 segundos)
    while (millis() - startTime < 10000) {
        xyzFloat mag = _mpu.getMagValues();
        totalSamples++;
        
        // Validar leitura
        if (!isnan(mag.x) && !isnan(mag.y) && !isnan(mag.z)) {
            // Atualizar min/max
            magMin[0] = min(magMin[0], mag.x);
            magMin[1] = min(magMin[1], mag.y);
            magMin[2] = min(magMin[2], mag.z);
            
            magMax[0] = max(magMax[0], mag.x);
            magMax[1] = max(magMax[1], mag.y);
            magMax[2] = max(magMax[2], mag.z);
            
            validSamples++;
        }
        
        // Feedback a cada 2 segundos
        uint32_t elapsed = millis() - startTime;
        if (elapsed - lastFeedback >= 2000) {
            uint32_t remaining = (10000 - elapsed) / 1000;
            float progress = (elapsed / 10000.0f) * 100.0f;
            
            DEBUG_PRINTF("[MPU9250Manager] [%.0f%%] %lu s restantes | Samples: %d/%d\n",
                         progress, remaining, validSamples, totalSamples);
            
            // Mostrar ranges atuais
            DEBUG_PRINTF("[MPU9250Manager]   X: %.1f a %.1f µT (range: %.1f)\n", 
                         magMin[0], magMax[0], magMax[0] - magMin[0]);
            DEBUG_PRINTF("[MPU9250Manager]   Y: %.1f a %.1f µT (range: %.1f)\n", 
                         magMin[1], magMax[1], magMax[1] - magMin[1]);
            DEBUG_PRINTF("[MPU9250Manager]   Z: %.1f a %.1f µT (range: %.1f)\n", 
                         magMin[2], magMax[2], magMax[2] - magMin[2]);
            
            lastFeedback = elapsed;
        }
        
        delay(50);  // 20 Hz de amostragem
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    DEBUG_PRINTLN("[MPU9250Manager] RESULTADOS DA CALIBRAÇÃO");
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    // Validar quantidade de samples
    if (validSamples < 100) {
        DEBUG_PRINTF("[MPU9250Manager] ❌ FALHA: Amostras insuficientes (%d/100)\n", validSamples);
        DEBUG_PRINTLN("[MPU9250Manager]   Usando offsets zerados");
        _magOffsetX = _magOffsetY = _magOffsetZ = 0.0f;
        return false;
    }
    
    DEBUG_PRINTF("[MPU9250Manager] ✓ Amostras coletadas: %d (válidas: %d)\n", 
                 totalSamples, validSamples);
    
    // Calcular offsets (ponto médio de hard iron)
    _magOffsetX = (magMax[0] + magMin[0]) / 2.0f;
    _magOffsetY = (magMax[1] + magMin[1]) / 2.0f;
    _magOffsetZ = (magMax[2] + magMin[2]) / 2.0f;
    
    DEBUG_PRINTF("[MPU9250Manager] Offsets calculados:\n");
    DEBUG_PRINTF("[MPU9250Manager]   X = %.2f µT\n", _magOffsetX);
    DEBUG_PRINTF("[MPU9250Manager]   Y = %.2f µT\n", _magOffsetY);
    DEBUG_PRINTF("[MPU9250Manager]   Z = %.2f µT\n", _magOffsetZ);
    
    // Validar magnitude do offset (hard iron excessivo indica problema)
    float offsetMagnitude = sqrt(_magOffsetX*_magOffsetX + 
                                  _magOffsetY*_magOffsetY + 
                                  _magOffsetZ*_magOffsetZ);
    
    DEBUG_PRINTF("[MPU9250Manager] Magnitude do hard iron: %.1f µT\n", offsetMagnitude);
    
    if (offsetMagnitude > 200.0f) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ AVISO: Hard iron muito alto!");
        DEBUG_PRINTLN("[MPU9250Manager]   Possíveis causas:");
        DEBUG_PRINTLN("[MPU9250Manager]   - Interferência magnética próxima");
        DEBUG_PRINTLN("[MPU9250Manager]   - Componentes magnéticos na PCB");
        DEBUG_PRINTLN("[MPU9250Manager]   - Movimento insuficiente durante calibração");
    }
    
    // Validar ranges (movimento suficiente em cada eixo)
    float rangeX = magMax[0] - magMin[0];
    float rangeY = magMax[1] - magMin[1];
    float rangeZ = magMax[2] - magMin[2];
    
    DEBUG_PRINTF("[MPU9250Manager] Ranges observados:\n");
    DEBUG_PRINTF("[MPU9250Manager]   X: %.1f µT\n", rangeX);
    DEBUG_PRINTF("[MPU9250Manager]   Y: %.1f µT\n", rangeY);
    DEBUG_PRINTF("[MPU9250Manager]   Z: %.1f µT\n", rangeZ);
    
    // Range mínimo esperado: ~40µT por eixo (movimento suficiente)
    bool xOk = rangeX > 40.0f;
    bool yOk = rangeY > 40.0f;
    bool zOk = rangeZ > 40.0f;
    
    if (!xOk || !yOk || !zOk) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ AVISO: Movimento insuficiente detectado!");
        DEBUG_PRINTF("[MPU9250Manager]   Eixo X: %s\n", xOk ? "OK" : "INSUFICIENTE");
        DEBUG_PRINTF("[MPU9250Manager]   Eixo Y: %s\n", yOk ? "OK" : "INSUFICIENTE");
        DEBUG_PRINTF("[MPU9250Manager]   Eixo Z: %s\n", zOk ? "OK" : "INSUFICIENTE");
        DEBUG_PRINTLN("[MPU9250Manager]   Recomenda-se recalibrar com mais movimento");
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    if (xOk && yOk && zOk && offsetMagnitude < 200.0f) {
        DEBUG_PRINTLN("[MPU9250Manager] ✅ CALIBRAÇÃO BEM SUCEDIDA!");
        
        // Perguntar se deseja salvar (ou salvar automaticamente)
        DEBUG_PRINTLN("[MPU9250Manager] ========================================");
        DEBUG_PRINTLN("[MPU9250Manager] Salvando calibração automaticamente...");
        
        if (saveOffsetsToMemory()) {
            DEBUG_PRINTLN("[MPU9250Manager] ✓ Calibração salva na memória!");
            DEBUG_PRINTLN("[MPU9250Manager]   Será carregada automaticamente no próximo boot");
        } else {
            DEBUG_PRINTLN("[MPU9250Manager] ⚠ Falha ao salvar calibração");
        }
    } else {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ CALIBRAÇÃO COM RESSALVAS");
        DEBUG_PRINTLN("[MPU9250Manager]   Dados serão usados, mas NÃO serão salvos");
        DEBUG_PRINTLN("[MPU9250Manager]   Recalibre para melhores resultados");
    }
    
    DEBUG_PRINTLN("[MPU9250Manager] ========================================");
    
    return true;
}

// ============================================================================
// VALIDAÇÃO DE LEITURAS
// ============================================================================
bool MPU9250Manager::_validateReadings(float gx, float gy, float gz,
                                        float ax, float ay, float az,
                                        float mx, float my, float mz) {
    // 1. Validar NAN
    if (isnan(gx) || isnan(gy) || isnan(gz)) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ Giroscópio: NAN detectado");
        return false;
    }
    
    if (isnan(ax) || isnan(ay) || isnan(az)) {
        DEBUG_PRINTLN("[MPU9250Manager] ⚠ Acelerômetro: NAN detectado");
        return false;
    }
    
    // 2. Validar giroscópio (±2000°/s máximo)
    const float GYRO_MAX = 2000.0f;
    if (fabs(gx) > GYRO_MAX || fabs(gy) > GYRO_MAX || fabs(gz) > GYRO_MAX) {
        DEBUG_PRINTF("[MPU9250Manager] ⚠ Giroscópio fora do range: %.1f %.1f %.1f °/s\n", 
                     gx, gy, gz);
        return false;
    }
    
    // 3. Validar acelerômetro (±16g máximo)
    const float ACCEL_MAX = 16.0f;
    if (fabs(ax) > ACCEL_MAX || fabs(ay) > ACCEL_MAX || fabs(az) > ACCEL_MAX) {
        DEBUG_PRINTF("[MPU9250Manager] ⚠ Acelerômetro fora do range: %.2f %.2f %.2f g\n", 
                     ax, ay, az);
        return false;
    }
    
    // 4. Validar magnitude do acelerômetro (0.5g a 2.0g em repouso)
    float accelMag = sqrt(ax*ax + ay*ay + az*az);
    if (accelMag < 0.3f || accelMag > 3.0f) {
        DEBUG_PRINTF("[MPU9250Manager] ⚠ Magnitude do acelerômetro anormal: %.2fg\n", accelMag);
        // Não retornar false, apenas warning (pode estar em movimento)
    }
    
    // 5. Validar magnetômetro (se online)
    if (_magOnline) {
        if (isnan(mx) || isnan(my) || isnan(mz)) {
            DEBUG_PRINTLN("[MPU9250Manager] ⚠ Magnetômetro: NAN detectado");
            // Não falhar a leitura completa por causa do magnetômetro
            _magOnline = false;
            return true;  // Continuar com 6-DOF
        }
        
        // Range do AK8963: ±4800 µT
        const float MAG_MAX = 4800.0f;
        if (fabs(mx) > MAG_MAX || fabs(my) > MAG_MAX || fabs(mz) > MAG_MAX) {
            DEBUG_PRINTF("[MPU9250Manager] ⚠ Magnetômetro fora do range: %.1f %.1f %.1f µT\n", 
                         mx, my, mz);
            // Não falhar, apenas warning
        }
        
        // Validar que não está zerado (sensor travado)
        if (mx == 0.0f && my == 0.0f && mz == 0.0f) {
            DEBUG_PRINTLN("[MPU9250Manager] ⚠ Magnetômetro retornou zeros - pode estar travado");
            _magOnline = false;
        }
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
