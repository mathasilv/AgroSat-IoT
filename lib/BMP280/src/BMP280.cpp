#include "BMP280.h"

// ============================================================================
// Construtor
// ============================================================================
BMP280::BMP280(TwoWire& wirePort)
    : _wire(&wirePort),
      _i2cAddress(0),
      _initialized(false),
      _dig_T1(0), _dig_T2(0), _dig_T3(0),
      _dig_P1(0), _dig_P2(0), _dig_P3(0), _dig_P4(0),
      _dig_P5(0), _dig_P6(0), _dig_P7(0), _dig_P8(0), _dig_P9(0),
      _t_fine(0)
{
}

// ============================================================================
// Inicialização do sensor
// ============================================================================
bool BMP280::begin(uint8_t i2cAddress) {
    _i2cAddress = i2cAddress;
    _initialized = false;
    
    // Verificar comunicação I2C e ID do chip
    uint8_t chipId = 0;
    if (!_readRegister(REG_ID, chipId)) {
        return false;
    }
    
    if (chipId != CHIP_ID) {
        #ifdef DEBUG_BMP280
        Serial.printf("[BMP280] ID inesperado: 0x%02X (esperado 0x%02X)\n", chipId, CHIP_ID);
        #endif
        return false;
    }
    
    // Soft reset para garantir estado limpo
    reset();
    delay(10);
    
    // Aguardar sensor ficar pronto
    if (!_waitForReady()) {
        #ifdef DEBUG_BMP280
        Serial.println("[BMP280] Timeout aguardando sensor ficar pronto");
        #endif
        return false;
    }
    
    // Ler coeficientes de calibração da NVM
    if (!_readCalibration()) {
        #ifdef DEBUG_BMP280
        Serial.println("[BMP280] Falha ao ler calibração");
        #endif
        return false;
    }
    
    // Aplicar configuração padrão otimizada
    if (!configure()) {
        return false;
    }
    
    _initialized = true;
    
    #ifdef DEBUG_BMP280
    Serial.println("[BMP280] Inicializado com sucesso");
    #endif
    
    return true;
}

// ============================================================================
// Configuração do sensor
// ============================================================================
bool BMP280::configure(Mode mode,
                       TempOversampling tempOS,
                       PressOversampling pressOS,
                       Filter filter,
                       StandbyTime standby) {
    
    // Construir registro CONFIG (filtro e tempo de standby)
    uint8_t configReg = (static_cast<uint8_t>(standby) << 5) |
                        (static_cast<uint8_t>(filter) << 2);
    
    if (!_writeRegister(REG_CONFIG, configReg)) {
        #ifdef DEBUG_BMP280
        Serial.println("[BMP280] Falha ao escrever CONFIG");
        #endif
        return false;
    }
    
    // Construir registro CTRL_MEAS (oversampling e modo)
    uint8_t ctrlMeasReg = (static_cast<uint8_t>(tempOS) << 5) |
                          (static_cast<uint8_t>(pressOS) << 2) |
                          static_cast<uint8_t>(mode);
    
    if (!_writeRegister(REG_CTRL_MEAS, ctrlMeasReg)) {
        #ifdef DEBUG_BMP280
        Serial.println("[BMP280] Falha ao escrever CTRL_MEAS");
        #endif
        return false;
    }
    
    return true;
}

// ============================================================================
// Leitura de temperatura
// ============================================================================
float BMP280::readTemperature() {
    if (!_initialized) {
        return NAN;
    }
    
    int32_t adcTemp = 0;
    int32_t adcPress = 0;
    
    if (!_readRawData(adcTemp, adcPress)) {
        return NAN;
    }
    
    // Aplicar compensação de temperatura e retornar em °C
    int32_t temp = _compensateTemp(adcTemp);
    return temp / 100.0f;
}

// ============================================================================
// Leitura de pressão
// ============================================================================
float BMP280::readPressure() {
    if (!_initialized) {
        return NAN;
    }
    
    int32_t adcTemp = 0;
    int32_t adcPress = 0;
    
    if (!_readRawData(adcTemp, adcPress)) {
        return NAN;
    }
    
    // Compensação de temperatura necessária para pressão
    _compensateTemp(adcTemp);
    
    // Aplicar compensação de pressão e retornar em Pa
    uint32_t press = _compensatePress(adcPress);
    return press / 256.0f;
}

// ============================================================================
// Cálculo de altitude (fórmula barométrica internacional)
// ============================================================================
float BMP280::readAltitude(float seaLevelPressure) {
    float pressure = readPressure();
    if (isnan(pressure)) {
        return NAN;
    }
    
    // Fórmula barométrica: h = 44330 * (1 - (p/p0)^(1/5.255))
    return 44330.0f * (1.0f - pow(pressure / seaLevelPressure, 0.1903f));
}

// ============================================================================
// Soft reset
// ============================================================================
void BMP280::reset() {
    _writeRegister(REG_RESET, RESET_CMD);
    _initialized = false;
}

// ============================================================================
// MÉTODOS PRIVADOS - Calibração e compensação
// ============================================================================

bool BMP280::_readCalibration() {
    uint8_t calibData[CALIB_DATA_SIZE];
    
    if (!_readRegisters(REG_CALIB_START, calibData, CALIB_DATA_SIZE)) {
        return false;
    }
    
    // Desempacotar coeficientes (little-endian, datasheet seção 3.11.2)
    _dig_T1 = (uint16_t)((calibData[1] << 8) | calibData[0]);
    _dig_T2 = (int16_t)((calibData[3] << 8) | calibData[2]);
    _dig_T3 = (int16_t)((calibData[5] << 8) | calibData[4]);
    
    _dig_P1 = (uint16_t)((calibData[7] << 8) | calibData[6]);
    _dig_P2 = (int16_t)((calibData[9] << 8) | calibData[8]);
    _dig_P3 = (int16_t)((calibData[11] << 8) | calibData[10]);
    _dig_P4 = (int16_t)((calibData[13] << 8) | calibData[12]);
    _dig_P5 = (int16_t)((calibData[15] << 8) | calibData[14]);
    _dig_P6 = (int16_t)((calibData[17] << 8) | calibData[16]);
    _dig_P7 = (int16_t)((calibData[19] << 8) | calibData[18]);
    _dig_P8 = (int16_t)((calibData[21] << 8) | calibData[20]);
    _dig_P9 = (int16_t)((calibData[23] << 8) | calibData[22]);
    
    return true;
}

bool BMP280::_readRawData(int32_t& adcTemp, int32_t& adcPress) {
    uint8_t data[6];
    
    // Ler 6 bytes: pressão (F7-F9) + temperatura (FA-FC)
    if (!_readRegisters(REG_PRESS_MSB, data, 6)) {
        return false;
    }
    
    // Desempacotar ADC values (20-bit cada, MSB first)
    adcPress = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);
    adcTemp = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);
    
    return true;
}

// Compensação de temperatura (datasheet seção 3.11.3, código 32-bit integer)
int32_t BMP280::_compensateTemp(int32_t adcTemp) {
    int32_t var1, var2;
    
    var1 = ((((adcTemp >> 3) - ((int32_t)_dig_T1 << 1))) * ((int32_t)_dig_T2)) >> 11;
    var2 = (((((adcTemp >> 4) - (int32_t)_dig_T1) * 
              ((adcTemp >> 4) - (int32_t)_dig_T1)) >> 12) * 
            (int32_t)_dig_T3) >> 14;
    
    _t_fine = var1 + var2;
    
    int32_t T = (_t_fine * 5 + 128) >> 8;
    return T; // Temperatura em 0.01°C (dividir por 100 para °C)
}

// Compensação de pressão (datasheet seção 3.11.3, código 64-bit integer)
uint32_t BMP280::_compensatePress(int32_t adcPress) {
    int64_t var1, var2, p;
    
    var1 = ((int64_t)_t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)_dig_P6;
    var2 = var2 + ((var1 * (int64_t)_dig_P5) << 17);
    var2 = var2 + (((int64_t)_dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)_dig_P3) >> 8) + ((var1 * (int64_t)_dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)_dig_P1) >> 33;
    
    if (var1 == 0) {
        return 0; // Evitar divisão por zero
    }
    
    p = 1048576 - adcPress;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)_dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)_dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)_dig_P7) << 4);
    
    return (uint32_t)p; // Pressão em Pa * 256 (Q24.8)
}

// ============================================================================
// MÉTODOS PRIVADOS - I2C
// ============================================================================

bool BMP280::_readRegister(uint8_t reg, uint8_t& value) {
    return _readRegisters(reg, &value, 1);
}

bool BMP280::_readRegisters(uint8_t reg, uint8_t* buffer, size_t length) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(reg);
    
    if (_wire->endTransmission(false) != 0) { // Repeated START
        return false;
    }
    
    if (_wire->requestFrom(_i2cAddress, length) != length) {
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }
    
    return true;
}

bool BMP280::_writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_i2cAddress);
    _wire->write(reg);
    _wire->write(value);
    
    return (_wire->endTransmission() == 0);
}

bool BMP280::_waitForReady(uint32_t timeoutMs) {
    uint32_t start = millis();
    
    while (millis() - start < timeoutMs) {
        uint8_t status = 0;
        if (!_readRegister(REG_STATUS, status)) {
            return false;
        }
        
        // Bit 3: measuring, Bit 0: im_update
        if ((status & 0x09) == 0) {
            return true; // Pronto
        }
        
        delay(5);
    }
    
    return false; // Timeout
}
