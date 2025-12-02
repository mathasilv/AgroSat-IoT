/**
 * @file CCS811.cpp
 * @brief Implementação do driver CCS811 - FIX I2C ROBUSTEZ
 */

#include "CCS811.h"

// ============================================================================
// CONSTRUTOR
// ============================================================================
CCS811::CCS811(TwoWire& wirePort)
    : _wire(&wirePort),
      _i2cAddress(0),
      _initialized(false),
      _eco2(400),
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
    delay(100); 
    
    // PASSO 0: SOFTWARE RESET
    Serial.println("[CCS811] PASSO 0: Software Reset...");
    uint8_t resetSeq[4] = {0x11, 0xE5, 0x72, 0x8A};
    _writeRegisters(REG_SW_RESET, resetSeq, 4);
    delay(100);
    
    // PASSO 1: Verificar presença (Ping)
    _wire->beginTransmission(_i2cAddress);
    if (_wire->endTransmission() != 0) {
        Serial.printf("[CCS811] PASSO 1 FALHOU: Não detectado 0x%02X\n", _i2cAddress);
        return false;
    }
    Serial.println("[CCS811] PASSO 1 OK");

    // PASSO 2: HW_ID com retry
    if (!_checkHardwareID()) {
        Serial.println("[CCS811] PASSO 2 FALHOU (HW_ID incorreto)");
        return false;
    }
    Serial.println("[CCS811] PASSO 2 OK");

    // PASSO 3: Verificar se o App é válido
    if (!_verifyAppValid()) {
        Serial.println("[CCS811] Erro: Firmware APP inválido");
        return false;
    }

    // PASSO 4: Iniciar Aplicação
    if (!_startApp()) {
        Serial.println("[CCS811] Erro: Falha ao iniciar APP_START");
        return false;
    }
    
    delay(1000);
    
    if (!_waitForAppMode(2000)) {
        Serial.println("[CCS811] Erro: Timeout aguardando APP_MODE");
        return false;
    }
    
    Serial.println("[CCS811] CCS811 ONLINE!");
    _initialized = true;
    return true;
}
 
void CCS811::reset() {
    uint8_t resetSeq[4] = {0x11, 0xE5, 0x72, 0x8A};
    _writeRegisters(REG_SW_RESET, resetSeq, 4);
    delay(100);
    _initialized = false;
}

bool CCS811::setDriveMode(DriveMode mode) {
    if (!_initialized) return false;
    uint8_t measMode = (static_cast<uint8_t>(mode) << 4);
    return _writeRegister(REG_MEAS_MODE, measMode);
}

bool CCS811::setEnvironmentalData(float humidity, float temperature) {
    if (!_initialized) return false;
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;
    
    uint8_t buffer[4];
    _encodeEnvironmentalData(humidity, temperature, buffer);
    return _writeRegisters(REG_ENV_DATA, buffer, 4);
}

bool CCS811::available() {
    if (!_initialized) return false;
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) return false;
    return (status & STATUS_DATA_READY) != 0;
}

bool CCS811::readData() {
    if (!_initialized) return false;
    
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) return false;
    
    if (!(status & STATUS_DATA_READY)) return false;
    
    uint8_t buffer[8];
    if (!_readRegisters(REG_ALG_RESULT_DATA, buffer, 8)) return false;
    
    uint16_t eco2_raw = (uint16_t(buffer[0]) << 8) | buffer[1];
    uint16_t tvoc_raw = (uint16_t(buffer[2]) << 8) | buffer[3];
    uint8_t status_byte = buffer[4];
    uint8_t errorId = buffer[5];
    
    if (status_byte & STATUS_ERROR) {
        Serial.printf("[CCS811] Erro interno do sensor: 0x%02X\n", errorId);
        return false;
    }
    
    _eco2 = eco2_raw;
    _tvoc = tvoc_raw;
    
    return true;
}

bool CCS811::getBaseline(uint16_t& baseline) {
    if (!_initialized) return false;
    uint8_t buffer[2];
    if (!_readRegisters(REG_BASELINE, buffer, 2)) return false;
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

bool CCS811::readRawData(uint16_t& current, uint16_t& voltage) {
    if (!_initialized) return false;
    uint8_t buffer[2];
    if (!_readRegisters(REG_RAW_DATA, buffer, 2)) return false;
    uint16_t rawData = (uint16_t(buffer[0]) << 8) | buffer[1];
    current = (rawData >> 10) & 0x3F;
    voltage = rawData & 0x3FF;
    return true;
}

uint8_t CCS811::getHardwareID() {
    uint8_t id = 0;
    _readRegister(REG_HW_ID, id);
    return id;
}

uint8_t CCS811::getHardwareVersion() {
    uint8_t version = 0;
    _readRegister(REG_HW_VERSION, version);
    return (version >> 4) & 0x0F;
}

uint16_t CCS811::getBootloaderVersion() {
    uint8_t buffer[2];
    if (!_readRegisters(REG_FW_BOOT_VERSION, buffer, 2)) return 0;
    return (uint16_t(buffer[0]) << 8) | buffer[1];
}

uint16_t CCS811::getApplicationVersion() {
    uint8_t buffer[2];
    if (!_readRegisters(REG_FW_APP_VERSION, buffer, 2)) return 0;
    return (uint16_t(buffer[0]) << 8) | buffer[1];
}

uint8_t CCS811::getErrorCode() {
    uint8_t error = 0;
    _readRegister(REG_ERROR_ID, error);
    return error;
}

bool CCS811::checkError() {
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) return true;
    return (status & STATUS_ERROR) != 0;
}

// ============================================================================
// MÉTODOS INTERNOS - INICIALIZAÇÃO
// ============================================================================
bool CCS811::_checkHardwareID() {
    uint8_t hwId = 0;
    for (int retry = 0; retry < 3; retry++) {
        if (_readRegister(REG_HW_ID, hwId)) {
            if (hwId == HW_ID_CODE) return true;
        }
        delay(50);
    }
    return false;
}

bool CCS811::_verifyAppValid() {
    uint8_t status;
    if (!_readRegister(REG_STATUS, status)) return false;
    return (status & STATUS_APP_VALID) != 0;
}

bool CCS811::_startApp() {
    return _writeCommand(REG_APP_START);
}

bool CCS811::_waitForAppMode(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    while (millis() - startTime < timeoutMs) {
        uint8_t status;
        if (_readRegister(REG_STATUS, status)) {
            if (status & STATUS_FW_MODE) return true;
        }
        delay(10);
    }
    return false;
}

// ============================================================================
// MÉTODOS INTERNOS - I2C (CORRIGIDO)
// ============================================================================
bool CCS811::_readRegister(uint8_t reg, uint8_t& value) {
    return _readRegisters(reg, &value, 1);
}

bool CCS811::_readRegisters(uint8_t reg, uint8_t* buffer, size_t length) {
    for (uint8_t attempt = 0; attempt < 5; attempt++) {        
        _wire->beginTransmission(_i2cAddress);
        _wire->write(reg);
        
        // [FIX] Usar STOP (true) em vez de Repeated Start (false)
        // Isso é mais seguro para sensores sensíveis em barramento lento
        if (_wire->endTransmission(true) != 0) {
            delay(20);
            continue;
        }
        
        if (_wire->requestFrom(_i2cAddress, length) == length) {
            for (size_t i = 0; i < length; i++) {
                buffer[i] = _wire->read();
            }
            return true;
        }
        delay(20);
    }
    return false;
}

bool CCS811::_writeRegister(uint8_t reg, uint8_t value) {
    return _writeRegisters(reg, &value, 1);
}

bool CCS811::_writeRegisters(uint8_t reg, const uint8_t* buffer, size_t length) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(reg);
    for (size_t i = 0; i < length; i++) _wire->write(buffer[i]);
    return (_wire->endTransmission() == 0);
}

bool CCS811::_writeCommand(uint8_t command) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(command);
    return (_wire->endTransmission() == 0);
}

void CCS811::_encodeEnvironmentalData(float humidity, float temperature, uint8_t* buffer) {
    uint16_t humReg = static_cast<uint16_t>(humidity * 512.0f + 0.5f);
    uint16_t tempReg = static_cast<uint16_t>((temperature + 25.0f) * 512.0f + 0.5f);
    buffer[0] = (humReg >> 8) & 0xFF;
    buffer[1] = humReg & 0xFF;
    buffer[2] = (tempReg >> 8) & 0xFF;
    buffer[3] = tempReg & 0xFF;
}