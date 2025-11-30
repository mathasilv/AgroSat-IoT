/**
 * @file CCS811.cpp
 * @brief Implementação do driver CCS811 nativo
 */

#include "CCS811.h"

// ============================================================================
// CONSTRUTOR
// ============================================================================
CCS811::CCS811(TwoWire& wirePort)
    : _wire(&wirePort),
      _i2cAddress(0),
      _initialized(false),
      _eco2(400),  // Valor inicial padrão (ar limpo)
      _tvoc(0)
{
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================
bool CCS811::begin(uint8_t i2cAddress) {
    _i2cAddress = i2cAddress;
    _initialized = false;

    Serial.println("[CCS811] Iniciando...");
    
    // I2C STABILIZATION PRIMEIRO
    Wire.end();
    delay(100);
    Wire.begin();
    delay(100);
    
    // PASSO 0: SOFTWARE RESET
    Serial.println("[CCS811] PASSO 0: Software Reset...");
    uint8_t resetSeq[4] = {0x11, 0xE5, 0x72, 0x8A};
    if (_writeRegisters(REG_SW_RESET, resetSeq, 4)) {
        delay(300);  // ← AUMENTADO
    } else {
        Serial.println("[CCS811] PASSO 0 FALHOU: Reset não enviado");
        return false;
    }
    
    // I2C STABILIZATION PÓS-RESET
    Wire.end();
    delay(100);
    Wire.begin();
    delay(100);
    
    // PASSO 1: Verificar presença
    if (Wire.endTransmission(_i2cAddress) != 0) {
        Serial.printf("[CCS811] PASSO 1 FALHOU: Não detectado 0x%02X\n", _i2cAddress);
        return false;
    }
    Serial.println("[CCS811] PASSO 1 OK");

    // PASSO 2: HW_ID com retry
    if (!_checkHardwareID()) {
        Serial.println("[CCS811] PASSO 2 FALHOU");
        return false;
    }
    Serial.println("[CCS811] PASSO 2 OK");

    // PASSO 3-6 iguais (app valid, start app, etc.)
    if (!_verifyAppValid()) return false;
    if (!_startApp()) return false;
    delay(3000);
    if (!_waitForAppMode(500)) return false;
    
    Serial.println("[CCS811] CCS811 ONLINE!");
    _initialized = true;
    return true;
}
 
// ============================================================================
// RESET
// ============================================================================
void CCS811::reset() {
    #ifdef DEBUG_CCS811
    Serial.println("[CCS811] Software reset...");
    #endif
    
    // Sequência de reset: escrever 0x11, 0xE5, 0x72, 0x8A no registro 0xFF
    uint8_t resetSeq[4] = {0x11, 0xE5, 0x72, 0x8A};
    _writeRegisters(REG_SW_RESET, resetSeq, 4);
    
    delay(100);
    _initialized = false;
}

// ============================================================================
// CONFIGURAÇÃO - MODO DE OPERAÇÃO
// ============================================================================
bool CCS811::setDriveMode(DriveMode mode) {
    if (!_initialized) return false;
    
    uint8_t measMode = (static_cast<uint8_t>(mode) << 4);
    bool result = _writeRegister(REG_MEAS_MODE, measMode);
    
#ifdef DEBUG_CCS811
    Serial.printf("[CCS811] setDriveMode(%d): %s\n", static_cast<uint8_t>(mode), result ? "OK" : "FAIL");
#endif
    
    return result;
}

// ============================================================================
// CONFIGURAÇÃO - COMPENSAÇÃO AMBIENTAL
// ============================================================================
bool CCS811::setEnvironmentalData(float humidity, float temperature) {
    if (!_initialized) return false;
    
    // Limitar valores válidos
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;
    
    // Codificar em formato Q9.7 (datasheet seção 4.11)
    uint8_t buffer[4];
    _encodeEnvironmentalData(humidity, temperature, buffer);
    
    return _writeRegisters(REG_ENV_DATA, buffer, 4);
}

// ============================================================================
// LEITURA DE DADOS
// ============================================================================
bool CCS811::available() {
    if (!_initialized) return false;
    
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) {
        return false;
    }
    
    return (status & STATUS_DATA_READY) != 0;
}

bool CCS811::readData() {
    if (!_initialized) return false;
    
    uint8_t buffer[8];
    if (!_readRegisters(REG_ALG_RESULT_DATA, buffer, 8)) {
        return false;
    }
    
    // Formato: eCO2(2) TVOC(2) STATUS(1) ERROR_ID(1) RAW_DATA(2)
    uint16_t eco2 = (uint16_t(buffer[0]) << 8) | buffer[1];
    uint16_t tvoc = (uint16_t(buffer[2]) << 8) | buffer[3];
    uint8_t status = buffer[4];
    uint8_t errorId = buffer[5];
    
    // Verificar se há erro
    if (status & STATUS_ERROR) {
        #ifdef DEBUG_CCS811
        Serial.printf("[CCS811] Erro reportado: 0x%02X\n", errorId);
        #endif
        return false;
    }
    
    // Validar valores (range físico do sensor)
    if (eco2 < 400 || eco2 > 8192) {
        #ifdef DEBUG_CCS811
        Serial.printf("[CCS811] eCO2 fora do range: %d ppm\n", eco2);
        #endif
        return false;
    }
    
    if (tvoc > 1187) {
        #ifdef DEBUG_CCS811
        Serial.printf("[CCS811] TVOC fora do range: %d ppb\n", tvoc);
        #endif
        return false;
    }
    
    _eco2 = eco2;
    _tvoc = tvoc;
    
    return true;
}

// ============================================================================
// BASELINE (Calibração)
// ============================================================================
bool CCS811::getBaseline(uint16_t& baseline) {
    if (!_initialized) return false;
    
    uint8_t buffer[2];
    if (!_readRegisters(REG_BASELINE, buffer, 2)) {
        return false;
    }
    
    baseline = (uint16_t(buffer[0]) << 8) | buffer[1];
    return true;
}

bool CCS811::setBaseline(uint16_t baseline) {
    if (!_initialized) return false;
    
    uint8_t buffer[2];
    buffer[0] = (baseline >> 8) & 0xFF;
    buffer[1] = baseline & 0xFF;
    
    return _writeRegisters(REG_BASELINE, buffer, 2);
}

// ============================================================================
// RAW DATA
// ============================================================================
bool CCS811::readRawData(uint16_t& current, uint16_t& voltage) {
    if (!_initialized) return false;
    
    uint8_t buffer[2];
    if (!_readRegisters(REG_RAW_DATA, buffer, 2)) {
        return false;
    }
    
    // Current: bits [15:10], Voltage: bits [9:0]
    uint16_t rawData = (uint16_t(buffer[0]) << 8) | buffer[1];
    current = (rawData >> 10) & 0x3F;
    voltage = rawData & 0x3FF;
    
    return true;
}

// ============================================================================
// DIAGNÓSTICO
// ============================================================================
uint8_t CCS811::getHardwareID() {
    uint8_t id = 0;
    _readRegister(REG_HW_ID, id);
    return id;
}

uint8_t CCS811::getHardwareVersion() {
    uint8_t version = 0;
    _readRegister(REG_HW_VERSION, version);
    return (version >> 4) & 0x0F; // Upper nibble
}

uint16_t CCS811::getBootloaderVersion() {
    uint8_t buffer[2];
    if (!_readRegisters(REG_FW_BOOT_VERSION, buffer, 2)) {
        return 0;
    }
    return (uint16_t(buffer[0]) << 8) | buffer[1];
}

uint16_t CCS811::getApplicationVersion() {
    uint8_t buffer[2];
    if (!_readRegisters(REG_FW_APP_VERSION, buffer, 2)) {
        return 0;
    }
    return (uint16_t(buffer[0]) << 8) | buffer[1];
}

uint8_t CCS811::getErrorCode() {
    uint8_t error = 0;
    _readRegister(REG_ERROR_ID, error);
    return error;
}

bool CCS811::checkError() {
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) {
        return true; // Assume erro se não conseguir ler
    }
    return (status & STATUS_ERROR) != 0;
}

// ============================================================================
// MÉTODOS INTERNOS - INICIALIZAÇÃO
// ============================================================================
bool CCS811::_checkHardwareID() {
    uint8_t hwId = 0;
    
    // MÉTODO ROBUSTO: 3 tentativas com I2C recovery
    for (int retry = 0; retry < 3; retry++) {
        if (_readRegister(REG_HW_ID, hwId)) {
            if (hwId == HW_ID_CODE) {
                Serial.printf("[CCS811] HW_ID OK: 0x%02X\n", hwId);
                return true;
            }
        }
        
        // I2C RECOVERY entre tentativas
        Serial.printf("[CCS811] HW_ID retry %d/3 (lido: 0x%02X)\n", retry+1, hwId);
        Wire.end();
        delay(50);
        Wire.begin();
        delay(50);
    }
    
    Serial.printf("[CCS811] HW_ID FINAL: 0x%02X (esperado 0x81)\n", hwId);
    return false;
}


bool CCS811::_verifyAppValid() {
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) {
        return false;
    }
    
    return (status & STATUS_APP_VALID) != 0;
}

bool CCS811::_startApp() {
    // Enviar comando APP_START (0xF4) sem dados
    return _writeCommand(REG_APP_START);
}

bool CCS811::_waitForAppMode(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeoutMs) {
        uint8_t status;
        if (_readRegister(REG_STATUS, status)) {
            if (status & STATUS_FW_MODE) {
                return true; // APP mode ativo
            }
        }
        delay(10);
    }
    
    return false;
}

// ============================================================================
// MÉTODOS INTERNOS - I2C
// ============================================================================
bool CCS811::_readRegister(uint8_t reg, uint8_t& value) {
    return _readRegisters(reg, &value, 1);
}

bool CCS811::_readRegisters(uint8_t reg, uint8_t* buffer, size_t length) {
    // Escrever endereço do registrador
    _wire->beginTransmission(_i2cAddress);
    _wire->write(reg);
    
    if (_wire->endTransmission(false) != 0) { // Repeated START
        return false;
    }
    
    // Ler dados
    if (_wire->requestFrom(_i2cAddress, length) != length) {
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }
    
    return true;
}

bool CCS811::_writeRegister(uint8_t reg, uint8_t value) {
    return _writeRegisters(reg, &value, 1);
}

bool CCS811::_writeRegisters(uint8_t reg, const uint8_t* buffer, size_t length) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(reg);
    
    for (size_t i = 0; i < length; i++) {
        _wire->write(buffer[i]);
    }
    
    return (_wire->endTransmission() == 0);
}

bool CCS811::_writeCommand(uint8_t command) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(command);
    return (_wire->endTransmission() == 0);
}

// ============================================================================
// MÉTODOS INTERNOS - CONVERSÃO
// ============================================================================
void CCS811::_encodeEnvironmentalData(float humidity, float temperature, uint8_t* buffer) {
    // Formato Q9.7 (datasheet seção 4.11):
    // Humidity: valor * 512 (1 bit sign, 9 bits integer, 7 bits fractional)
    // Temperature: (valor + 25) * 512
    
    uint16_t humReg = static_cast<uint16_t>(humidity * 512.0f + 0.5f);
    uint16_t tempReg = static_cast<uint16_t>((temperature + 25.0f) * 512.0f + 0.5f);
    
    buffer[0] = (humReg >> 8) & 0xFF;
    buffer[1] = humReg & 0xFF;
    buffer[2] = (tempReg >> 8) & 0xFF;
    buffer[3] = tempReg & 0xFF;
}
