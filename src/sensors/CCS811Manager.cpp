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
    _initTime = 0;
    _lastReadTime = 0;
    
    // PASSO 1: Verificar presença I2C nos dois endereços possíveis
    uint8_t addresses[] = {CCS811::I2C_ADDR_LOW, CCS811::I2C_ADDR_HIGH};
    uint8_t detectedAddr = 0x00;
    
    DEBUG_PRINTLN("[CCS811Manager] PASSO 1: Detectando sensor I2C...");
    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[CCS811Manager]   Testando 0x%02X... ", addr);
        
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            DEBUG_PRINTLN("✓ DETECTADO");
            detectedAddr = addr;
            break;
        } else {
            DEBUG_PRINTF("✗ Erro I2C: %d\n", error);
        }
        delay(50);
    }
    
    if (detectedAddr == 0x00) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: Sensor não detectado em nenhum endereço");
        return false;
    }
    
    // PASSO 2: Inicializar driver com o endereço detectado
    DEBUG_PRINTF("[CCS811Manager] PASSO 2: Inicializando driver em 0x%02X...\n", detectedAddr);
    
    if (!_ccs811.begin(detectedAddr)) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: begin() retornou false");
        DEBUG_PRINTLN("[CCS811Manager]   Possíveis causas:");
        DEBUG_PRINTLN("[CCS811Manager]   - Sensor em modo BOOT (precisa carregar APP)");
        DEBUG_PRINTLN("[CCS811Manager]   - HW_ID incorreto");
        DEBUG_PRINTLN("[CCS811Manager]   - Falha na comunicação I2C");
        return false;
    }
    
    DEBUG_PRINTLN("[CCS811Manager] ✓ Driver inicializado");
    
    // PASSO 3: Validar HW_ID (deve ser 0x81)
    DEBUG_PRINTLN("[CCS811Manager] PASSO 3: Validando HW_ID...");
    uint8_t hwId = _ccs811.getHardwareID();
    DEBUG_PRINTF("[CCS811Manager]   HW_ID lido: 0x%02X (esperado: 0x81)\n", hwId);
    
    if (hwId != 0x81) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: HW_ID inválido");
        DEBUG_PRINTLN("[CCS811Manager]   Isso indica:");
        DEBUG_PRINTLN("[CCS811Manager]   - Sensor não é CCS811 genuíno");
        DEBUG_PRINTLN("[CCS811Manager]   - Comunicação I2C corrompida");
        return false;
    }
    
    DEBUG_PRINTLN("[CCS811Manager] ✓ HW_ID válido (CCS811 genuíno)");
    
    // PASSO 4: Verificar se firmware APP está válido
    DEBUG_PRINTLN("[CCS811Manager] PASSO 4: Verificando firmware APP...");
    
    // Ler registrador STATUS (0x00)
    Wire.beginTransmission(detectedAddr);
    Wire.write(0x00); // STATUS register
    Wire.endTransmission(false);
    Wire.requestFrom(detectedAddr, (uint8_t)1);
    
    if (Wire.available()) {
        uint8_t status = Wire.read();
        bool appValid = (status & 0x10); // Bit 4: APP_VALID
        
        DEBUG_PRINTF("[CCS811Manager]   STATUS=0x%02X APP_VALID=%s\n", 
                     status, appValid ? "SIM" : "NÃO");
        
        if (!appValid) {
            DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: Firmware APP não está carregado");
            DEBUG_PRINTLN("[CCS811Manager]   Sensor está em modo BOOT");
            DEBUG_PRINTLN("[CCS811Manager]   Solução: Verificar pinagem WAKE e alimentação");
            return false;
        }
    } else {
        DEBUG_PRINTLN("[CCS811Manager] ⚠ AVISO: Não foi possível ler STATUS");
    }
    
    DEBUG_PRINTLN("[CCS811Manager] ✓ Firmware APP válido");
    
    // PASSO 5: Configurar modo de medição (MODE 1 = 1 medição/segundo)
    DEBUG_PRINTLN("[CCS811Manager] PASSO 5: Configurando modo de medição...");
    DEBUG_PRINTLN("[CCS811Manager]   Modo selecionado: MODE_1SEC (1 Hz)");
    
    bool modeConfigured = false;
    for (int retry = 0; retry < 3; retry++) {
        if (_ccs811.setDriveMode(CCS811::DriveMode::MODE_1SEC)) {
            modeConfigured = true;
            DEBUG_PRINTLN("[CCS811Manager] ✓ Modo configurado com sucesso");
            break;
        }
        
        DEBUG_PRINTF("[CCS811Manager]   Retry %d/3...\n", retry + 1);
        delay(100);
    }
    
    if (!modeConfigured) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: Não foi possível configurar modo");
        return false;
    }
    
    // PASSO 6: Warm-up inicial obrigatório (20 segundos mínimo)
    DEBUG_PRINTLN("[CCS811Manager] PASSO 6: Warm-up inicial (20 segundos)...");
    DEBUG_PRINTLN("[CCS811Manager]   IMPORTANTE: Dados serão confiáveis após 20 min");
    
    if (!_performWarmup()) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ FALHA: Timeout no warm-up");
        return false;
    }
    
    // PASSO 7: Leitura de teste
    DEBUG_PRINTLN("[CCS811Manager] PASSO 7: Leitura de teste...");
    delay(2000); // Aguardar medição completar
    
    if (_ccs811.available()) {
        if (_ccs811.readData()) {
            uint16_t testEco2 = _ccs811.geteCO2();
            uint16_t testTvoc = _ccs811.getTVOC();
            
            DEBUG_PRINTF("[CCS811Manager]   eCO2 = %d ppm\n", testEco2);
            DEBUG_PRINTF("[CCS811Manager]   TVOC = %d ppb\n", testTvoc);
            DEBUG_PRINTLN("[CCS811Manager] ✓ Leitura de teste OK");
        }
    }
    
    // SUCESSO!
    _online = true;
    _initTime = millis();
    
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    DEBUG_PRINTLN("[CCS811Manager] ✅ CCS811 INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    DEBUG_PRINTLN("[CCS811Manager] OBSERVAÇÕES:");
    DEBUG_PRINTLN("[CCS811Manager] - Dados funcionais após 20 segundos");
    DEBUG_PRINTLN("[CCS811Manager] - Precisão ideal após 20 minutos");
    DEBUG_PRINTLN("[CCS811Manager] - Usar setEnvironmentalData() para melhor precisão");
    DEBUG_PRINTLN("[CCS811Manager] ========================================");
    
    return true;
}
// ============================================================================
// ATUALIZAÇÃO PRINCIPAL
// ============================================================================
void CCS811Manager::update() {
    if (!_online) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // Respeitar intervalo mínimo de 5 segundos entre leituras
    // (sensor mede a 1 Hz, mas não precisamos ler tão rápido)
    if (currentTime - _lastReadTime < READ_INTERVAL) {
        return;
    }
    
    // Verificar se há novos dados disponíveis
    if (!_ccs811.available()) {
        // Não há dados novos ainda, retornar sem erro
        return;
    }
    
    // Tentar ler dados do sensor
    if (!_ccs811.readData()) {
        DEBUG_PRINTLN("[CCS811Manager] ❌ Erro ao ler dados");
        
        // Verificar se há código de erro específico
        if (_ccs811.checkError()) {
            uint8_t errorCode = _ccs811.getErrorCode();
            DEBUG_PRINTF("[CCS811Manager]   Código de erro: 0x%02X\n", errorCode);
            
            // Decodificar erro (baseado no datasheet CCS811)
            if (errorCode & 0x01) DEBUG_PRINTLN("[CCS811Manager]   - WRITE_REG_INVALID");
            if (errorCode & 0x02) DEBUG_PRINTLN("[CCS811Manager]   - READ_REG_INVALID");
            if (errorCode & 0x04) DEBUG_PRINTLN("[CCS811Manager]   - MEASMODE_INVALID");
            if (errorCode & 0x08) DEBUG_PRINTLN("[CCS811Manager]   - MAX_RESISTANCE");
            if (errorCode & 0x10) DEBUG_PRINTLN("[CCS811Manager]   - HEATER_FAULT");
            if (errorCode & 0x20) DEBUG_PRINTLN("[CCS811Manager]   - HEATER_SUPPLY");
            
            // Erro de heater é crítico - reiniciar sensor
            if (errorCode & 0x30) {
                DEBUG_PRINTLN("[CCS811Manager] ⚠ Erro crítico de heater - reset necessário");
                reset();
            }
        }
        return;
    }
    
    // Obter valores lidos
    uint16_t eco2 = _ccs811.geteCO2();
    uint16_t tvoc = _ccs811.getTVOC();
    
    // VALIDAÇÃO RIGOROSA baseada no datasheet
    if (!_validateData(eco2, tvoc)) {
        DEBUG_PRINTF("[CCS811Manager] ⚠ Dados fora do range válido: eCO2=%d ppm, TVOC=%d ppb\n", 
                    eco2, tvoc);
        
        // Não atualizar valores, manter último conhecido
        return;
    }
    
    // Validação adicional: detectar valores fixos (sensor travado)
    static uint16_t lastEco2 = 0;
    static uint8_t identicalCount = 0;
    
    if (eco2 == lastEco2 && lastEco2 != 0) {
        identicalCount++;
        
        if (identicalCount >= 10) {
            DEBUG_PRINTLN("[CCS811Manager] ⚠ Sensor pode estar travado (10 leituras idênticas)");
            DEBUG_PRINTLN("[CCS811Manager]   Considerando reset...");
            
            // Resetar contador para não ficar spammando
            identicalCount = 0;
            
            // Opcional: forçar reset após muitas leituras idênticas
            // reset();
            // return;
        }
    } else {
        identicalCount = 0;
    }
    lastEco2 = eco2;
    
    // Dados válidos - atualizar valores
    _eco2 = eco2;
    _tvoc = tvoc;
    _lastReadTime = currentTime;
    
    // Debug apenas se houve mudança significativa
    static uint16_t lastPrintedEco2 = 0;
    if (abs((int)eco2 - (int)lastPrintedEco2) >= 50) {
        DEBUG_PRINTF("[CCS811Manager] eCO2=%d ppm, TVOC=%d ppb", eco2, tvoc);
        
        // Indicar status de warm-up
        if (!isWarmupComplete()) {
            uint32_t progress = getWarmupProgress();
            DEBUG_PRINTF(" [Warm-up: %lu%%]", progress);
        }
        
        DEBUG_PRINTLN();
        lastPrintedEco2 = eco2;
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