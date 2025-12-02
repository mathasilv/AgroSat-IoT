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
    DEBUG_PRINTLN("[CCS811Manager] Inicializando CCS811 Air Quality Sensor");
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    
    _online = false;
    _eco2 = 400;
    _tvoc = 0;
    _initTime = millis();
    _lastReadTime = 0;
    
    // 🔧 Testar ambos endereços I2C
    uint8_t addresses[] = {CCS811::I2C_ADDR_LOW, CCS811::I2C_ADDR_HIGH};
    uint8_t detectedAddr = 0;
    
    DEBUG_PRINTLN("[CCS811Manager] 🔍 Detectando sensor I2C...");
    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[CCS811Manager]   Testando 0x%02X... ", addr);
        
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            DEBUG_PRINTLN("✓ DETECTADO");
            detectedAddr = addr;
            break;
        }
        DEBUG_PRINTLN("✗ Não respondendo");
        delay(50);
    }
    
    if (!detectedAddr) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ Sensor não encontrado");
        return false;
    }
    
    // 🔧 Inicializar driver
    DEBUG_PRINTF("[CCS811Manager] 🚀 Inicializando driver (0x%02X)...\n", detectedAddr);
    if (!_ccs811.begin(detectedAddr)) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ Falha na inicialização do driver");
        return false;
    }
    
    // 🔧 Configurar modo 1Hz (1 medição/segundo)
    if (!_ccs811.setDriveMode(CCS811::DriveMode::MODE_1SEC)) {
        DEBUG_PRINTLN("[CCS811Manager] ⚠️ Modo 1Hz não configurado");
    }
    
    // 🔧 Warm-up de 20 segundos
    DEBUG_PRINTLN("[CCS811Manager] ⏳ Warm-up (20s)...");
    delay(20000);
    
    _online = true;
    
    DEBUG_PRINTLN("[CCS811Manager] ✅ CCS811 ONLINE!");
    DEBUG_PRINTLN("[CCS811Manager] 📊 Precisão máxima após 20 minutos");
    return true;
}

void CCS811Manager::update() {
    if (!_online) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // 🔧 CORREÇÃO 1: Rate limiting rigoroso (mínimo 2s entre leituras)
    static constexpr uint32_t MIN_READ_INTERVAL = 2000;  // 2 segundos
    if (currentTime - _lastReadTime < MIN_READ_INTERVAL) {
        return;
    }
    
    // 🔧 CORREÇÃO 2: VERIFICAÇÃO OBRIGATÓRIA do bit DATA_READY
    if (!_ccs811.available()) {
        // Sensor ainda processando - NÃO É ERRO, apenas aguardar
        return;
    }
    
    // 🔧 CORREÇÃO 3: Contador de erros consecutivos
    static uint16_t consecutiveErrors = 0;
    
    // Tentar leitura dos dados
    if (!_ccs811.readData()) {
        consecutiveErrors++;
        DEBUG_PRINTLN("[CCS811Manager] ❌ Erro ao ler dados");
        
        // 🔧 Reset automático após 8 erros consecutivos
        if (consecutiveErrors >= 8) {
            DEBUG_PRINTLN("[CCS811Manager] 🔄 Reset automático (8 erros)");
            reset();
            consecutiveErrors = 0;
            return;
        }
        
        return;
    }
    
    // ✅ SUCESSO - reset contador de erros
    consecutiveErrors = 0;
    _lastReadTime = currentTime;
    
    // Obter valores
    uint16_t eco2 = _ccs811.geteCO2();
    uint16_t tvoc = _ccs811.getTVOC();
    
    // 🔧 Validação rigorosa (datasheet CCS811)
    if (eco2 < 400 || eco2 > 8192 || tvoc > 1187) {
        DEBUG_PRINTF("[CCS811Manager] ⚠ Dados inválidos: eCO2=%d, TVOC=%d\n", eco2, tvoc);
        return;
    }
    
    // Atualizar valores internos
    _eco2 = eco2;
    _tvoc = tvoc;
    
    // 🔧 Log controlado (apenas mudanças > 50ppm)
    static uint16_t lastLoggedEco2 = 0;
    if (abs((int)eco2 - (int)lastLoggedEco2) >= 50) {
        uint32_t warmupProgress = getWarmupProgress();
        DEBUG_PRINTF("[CCS811Manager] eCO2=%d ppm, TVOC=%d ppb [Warm-up: %lu%%]\n", 
                     eco2, tvoc, warmupProgress);
        lastLoggedEco2 = eco2;
    }
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
    if (!_online) {
        DEBUG_PRINTLN("[CCS811Manager] Sensor offline - compensação não aplicada");
        return false;
    }
    
    // Validar ranges razoáveis
    if (humidity < 0.0f || humidity > 100.0f) {
        DEBUG_PRINTF("[CCS811Manager] Umidade inválida: %.1f%% (válido: 0-100)\n", humidity);
        return false;
    }
    
    if (temperature < -40.0f || temperature > 85.0f) {
        DEBUG_PRINTF("[CCS811Manager] Temperatura inválida: %.1f°C (válido: -40 a 85)\n", temperature);
        return false;
    }
    
    bool result = _ccs811.setEnvironmentalData(humidity, temperature);
    
    if (result) {
        DEBUG_PRINTF("[CCS811Manager] Compensação ambiental aplicada: T=%.1f°C RH=%.1f%%\n", 
                    temperature, humidity);
    } else {
        DEBUG_PRINTLN("[CCS811Manager] ⚠ Falha ao aplicar compensação ambiental");
    }
    
    return result;
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
/**
 * @brief Warm-up robusto com validação de disponibilidade
 * @return true se warm-up foi bem sucedido
 * 
 * DATASHEET CCS811:
 * - Warm-up mínimo: 20 segundos (dados funcionais)
 * - Warm-up ideal: 20 minutos (máxima precisão)
 * - Primeira leitura pode demorar 1 segundo após configuração
 */
bool CCS811Manager::_performWarmup() {
    DEBUG_PRINTLN("[CCS811Manager]   Iniciando warm-up obrigatório...");
    DEBUG_PRINTLN("[CCS811Manager]   Duração: 20 segundos (mínimo funcional)");
    
    uint32_t startTime = millis();
    uint32_t lastFeedback = 0;
    bool firstReadingOk = false;
    
    // Loop de warm-up de 20 segundos
    while ((millis() - startTime) < WARMUP_MINIMUM) {
        uint32_t elapsed = millis() - startTime;
        
        // Feedback a cada 5 segundos
        if (elapsed - lastFeedback >= 5000) {
            DEBUG_PRINTF("[CCS811Manager]   Warm-up: %lu s / 20 s\n", elapsed / 1000);
            lastFeedback = elapsed;
        }
        
        // Tentar fazer primeira leitura após 2 segundos
        if (elapsed >= 2000 && !firstReadingOk) {
            if (_ccs811.available()) {
                if (_ccs811.readData()) {
                    uint16_t eco2 = _ccs811.geteCO2();
                    DEBUG_PRINTF("[CCS811Manager]   ✓ Primeira leitura OK: eCO2=%d ppm\n", eco2);
                    firstReadingOk = true;
                }
            }
        }
        
        delay(500);
    }
    
    DEBUG_PRINTLN("[CCS811Manager]   ✓ Warm-up mínimo concluído (20s)");
    
    // Validar que pelo menos uma leitura funcionou
    if (!firstReadingOk) {
        DEBUG_PRINTLN("[CCS811Manager]   ⚠ AVISO: Nenhuma leitura durante warm-up");
        DEBUG_PRINTLN("[CCS811Manager]   Continuando mesmo assim...");
    }
    
    return true;
}


bool CCS811Manager::_validateData(uint16_t eco2, uint16_t tvoc) const {
    // Validar eCO2 (400-8192 ppm conforme datasheet)
    if (eco2 < ECO2_MIN || eco2 > ECO2_MAX) {
        DEBUG_PRINTF("[CCS811Manager]   eCO2 fora do range: %d ppm (válido: %d-%d)\n", 
                    eco2, ECO2_MIN, ECO2_MAX);
        return false;
    }
    
    // Validar TVOC (0-1187 ppb conforme datasheet)
    if (tvoc > TVOC_MAX) {
        DEBUG_PRINTF("[CCS811Manager]   TVOC fora do range: %d ppb (máximo: %d)\n", 
                    tvoc, TVOC_MAX);
        return false;
    }
    
    return true;
}

bool CCS811Manager::isDataReliable() const {
    if (!_online || _initTime == 0) {
        return false;
    }
    
    // Dados são totalmente confiáveis após 20 minutos
    return (millis() - _initTime) >= WARMUP_OPTIMAL;
}