/**
 * @file CCS811Manager.cpp
 * @brief Implementação do gerenciador CCS811
 */

#include "CCS811Manager.h"

// ============================================================================
// CONSTRUTOR
// ============================================================================
CCS811Manager::CCS811Manager()
    : _ccs811(Wire),
      _eco2(400),  // Valor padrão (ar limpo)
      _tvoc(0),
      _online(false),
      _initTime(0),
      _lastReadTime(0)
{
}

bool CCS811Manager::begin() {
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    DEBUG_PRINTLN("[CCS811Manager] DEBUG DETALHADO CCS811");
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    
    _online = false;
    
    // PASSO 1: Testar endereços
    uint8_t addresses[] = {CCS811::I2C_ADDR_LOW, CCS811::I2C_ADDR_HIGH};
    
    for (int i = 0; i < 2; i++) {
        uint8_t addr = addresses[i];
        DEBUG_PRINTF("[CCS811Manager] PASSO 1.%d: Testando 0x%02X... ", i+1, addr);
        
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            DEBUG_PRINTLN("✓ DETECTADO!");
            
            // PASSO 2: Tentar begin()
            DEBUG_PRINTF("[CCS811Manager] PASSO 2: _ccs811.begin(0x%02X)... ", addr);
            bool beginResult = _ccs811.begin(addr);
            DEBUG_PRINTF("Resultado: %s\n", beginResult ? "SUCESSO" : "FALHOU");
            
            if (beginResult) {
                // PASSO 3: Verificar HW_ID
                uint8_t hwId = _ccs811.getHardwareID();
                DEBUG_PRINTF("[CCS811Manager] PASSO 3: HW_ID=0x%02X (esperado: 0x81)\n", hwId);
                
                if (hwId == 0x81) {
                    DEBUG_PRINTLN("[CCS811Manager] ✓ HW_ID CORRETO!");
                    
                    // PASSO 4: Configurar modo
                    DEBUG_PRINTLN("[CCS811Manager] PASSO 4: setDriveMode...");
                    bool driveModeResult = _ccs811.setDriveMode(CCS811::DriveMode::MODE_1SEC);
                    DEBUG_PRINTF("Resultado: %s\n", driveModeResult ? "SUCESSO" : "FALHOU");
                    
                    if (driveModeResult) {
                        DEBUG_PRINTLN("[CCS811Manager] ========================================");
                        DEBUG_PRINTLN("[CCS811Manager] CCS811 INICIALIZADO COM SUCESSO!");
                        _online = true;
                        _initTime = millis();
                        return true;
                    }
                }
            }
        } else {
            DEBUG_PRINTF("✗ Erro I2C: %d\n", error);
        }
        delay(100);
    }
    
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    DEBUG_PRINTLN("[CCS811Manager] CCS811 FALHOU EM TODOS PASSOS");
    return false;
}


// ============================================================================
// ATUALIZAÇÃO PRINCIPAL
// ============================================================================
void CCS811Manager::update() {
    if (!_online) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // Respeitar intervalo mínimo de leitura
    if (currentTime - _lastReadTime < READ_INTERVAL) {
        return;
    }
    
    // Verificar se há dados disponíveis
    if (!_ccs811.available()) {
        return;
    }
    
    // Ler dados do sensor
    if (!_ccs811.readData()) {
        DEBUG_PRINTLN("[CCS811Manager] Erro ao ler dados");
        
        // Verificar código de erro
        if (_ccs811.checkError()) {
            uint8_t errorCode = _ccs811.getErrorCode();
            DEBUG_PRINTF("[CCS811Manager] Código de erro: 0x%02X\n", errorCode);
        }
        return;
    }
    
    // Obter valores lidos
    uint16_t eco2 = _ccs811.geteCO2();
    uint16_t tvoc = _ccs811.getTVOC();
    
    // Validar dados
    if (!_validateData(eco2, tvoc)) {
        DEBUG_PRINTF("[CCS811Manager] Dados inválidos: eCO2=%d ppm, TVOC=%d ppb\n", 
                    eco2, tvoc);
        return;
    }
    
    // Atualizar valores
    _eco2 = eco2;
    _tvoc = tvoc;
    _lastReadTime = currentTime;
}

// ============================================================================
// RESET
// ============================================================================
void CCS811Manager::reset() {
    DEBUG_PRINTLN("[CCS811Manager] Reset do sensor...");
    _ccs811.reset();
    _online = false;
    _initTime = 0;
    delay(100);
}

// ============================================================================
// COMPENSAÇÃO AMBIENTAL
// ============================================================================
bool CCS811Manager::setEnvironmentalData(float humidity, float temperature) {
    if (!_online) return false;
    
    return _ccs811.setEnvironmentalData(humidity, temperature);
}

// ============================================================================
// BASELINE
// ============================================================================
bool CCS811Manager::getBaseline(uint16_t& baseline) {
    if (!_online) return false;
    
    return _ccs811.getBaseline(baseline);
}

bool CCS811Manager::setBaseline(uint16_t baseline) {
    if (!_online) return false;
    
    return _ccs811.setBaseline(baseline);
}

// ============================================================================
// STATUS
// ============================================================================
bool CCS811Manager::isWarmupComplete() const {
    if (!_online || _initTime == 0) return false;
    
    return (millis() - _initTime) >= WARMUP_OPTIMAL;
}

uint32_t CCS811Manager::getWarmupProgress() const {
    if (!_online || _initTime == 0) return 0;
    
    uint32_t elapsed = millis() - _initTime;
    if (elapsed >= WARMUP_OPTIMAL) return 100;
    
    return (elapsed * 100) / WARMUP_OPTIMAL;
}

// ============================================================================
// DIAGNÓSTICO
// ============================================================================
void CCS811Manager::printStatus() const {
    DEBUG_PRINTF(" CCS811: %s", _online ? "ONLINE" : "OFFLINE");
    
    if (_online) {
        uint32_t progress = getWarmupProgress();
        DEBUG_PRINTF(" (Warm-up: %lu%%)", progress);
        
        if (progress < 100) {
            uint32_t remaining = (WARMUP_OPTIMAL - (millis() - _initTime)) / 60000;
            DEBUG_PRINTF(" [%lu min restantes]", remaining);
        }
    }
    
    DEBUG_PRINTLN();
    
    if (_online) {
        DEBUG_PRINTF("   eCO2=%d ppm, TVOC=%d ppb\n", _eco2, _tvoc);
    }
}

bool CCS811Manager::checkError() {
    if (!_online) return true;
    
    return _ccs811.checkError();
}

uint8_t CCS811Manager::getErrorCode() {
    if (!_online) return 0xFF;
    
    return _ccs811.getErrorCode();
}

// ============================================================================
// MÉTODOS INTERNOS
// ============================================================================
bool CCS811Manager::_performWarmup() {
    DEBUG_PRINTLN("[CCS811Manager] Iniciando warm-up (20 segundos)...");
    
    uint32_t startTime = millis();
    
    // Aguardar sensor ficar disponível (warm-up mínimo)
    while ((millis() - startTime) < WARMUP_MINIMUM) {
        if (_ccs811.available()) {
            // Fazer uma leitura de teste
            if (_ccs811.readData()) {
                uint32_t elapsed = millis() - startTime;
                DEBUG_PRINTF("[CCS811Manager] Sensor disponível após %lu ms\n", elapsed);
                
                // Aguardar completar warm-up mínimo
                uint32_t remaining = WARMUP_MINIMUM - elapsed;
                if (remaining > 0) {
                    DEBUG_PRINTF("[CCS811Manager] Aguardando mais %lu ms...\n", remaining);
                    delay(remaining);
                }
                
                DEBUG_PRINTLN("[CCS811Manager] Warm-up mínimo concluído!");
                DEBUG_PRINTLN("[CCS811Manager] (Para precisão ótima, aguarde 20 minutos)");
                return true;
            }
        }
        
        // Feedback a cada 5 segundos
        if ((millis() - startTime) % 5000 < 100) {
            DEBUG_PRINTF("[CCS811Manager] Warm-up: %lu s / 20 s\n", 
                        (millis() - startTime) / 1000);
        }
        
        delay(500);
    }
    
    // Timeout - warm-up falhou
    DEBUG_PRINTLN("[CCS811Manager] Timeout no warm-up");
    return false;
}

bool CCS811Manager::_validateData(uint16_t eco2, uint16_t tvoc) const {
    // Validar range do eCO2
    if (eco2 < ECO2_MIN || eco2 > ECO2_MAX) {
        return false;
    }
    
    // Validar range do TVOC
    if (tvoc > TVOC_MAX) {
        return false;
    }
    
    return true;
}
