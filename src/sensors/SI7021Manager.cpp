/**
 * @file SI7021Manager.cpp
 * @brief SI7021Manager V3.0 - COMPILAÇÃO OK
 */
#include "SI7021Manager.h"

// ============================================================================
// CONSTRUTOR
SI7021Manager::SI7021Manager() 
    : _humidity(NAN), _temperature(NAN), 
      _online(false), _tempValid(false),
      _failCount(0), _lastReadTime(0),
      _initTime(0), _warmupProgress(0) {}

// ============================================================================
// INICIALIZAÇÃO
bool SI7021Manager::begin() {
    DEBUG_PRINTLN("[SI7021Manager] ========================================");
    DEBUG_PRINTLN("[SI7021Manager] Inicializando SI7021 (modo rigoroso)");
    
    _online = false;
    _failCount = 0;
    _initTime = millis();
    
    // TESTE 1: Presença física
    Wire.beginTransmission(_SI7021_ADDR);
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("[SI7021Manager] FALHA: Não detectado (0x40)");
        return false;
    }
    
    // TESTE 2: Soft Reset
    Wire.beginTransmission(_SI7021_ADDR);
    Wire.write(_CMD_SOFT_RESET);
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("[SI7021Manager] FALHA: Soft Reset");
        return false;
    }
    delay(50);
    
    // TESTE 3: Leitura CRÍTICA (detecta chip falso)
    if (!_testRealHumidityRead()) {
        DEBUG_PRINTLN("[SI7021Manager] FALHA: CHIP FALSO/DEFETUOSO (0x40 responde mas não lê)");
        return false;
    }
    
    _online = true;
    _warmupProgress = WARMUP_TIME;
    DEBUG_PRINTLN("[SI7021Manager] INICIALIZADO COM SUCESSO!");
    return true;
}

// ============================================================================
// TESTE CRÍTICO - Leitura real
bool SI7021Manager::_testRealHumidityRead() {
    Wire.beginTransmission(_SI7021_ADDR);
    Wire.write(_CMD_MEASURE_RH_NOHOLD);
    if (Wire.endTransmission() != 0) return false;
    
    delay(25);
    
    for (uint8_t retry = 0; retry < 3; retry++) {
        Wire.requestFrom((uint8_t)_SI7021_ADDR, (uint8_t)3);  // ← FIX: casts explícitos
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read();
            uint8_t lsb = Wire.read();
            if (Wire.available()) Wire.read(); // CRC
            
            uint16_t rawHum = (msb << 8) | lsb;
            
            if (rawHum > 0x0100 && rawHum < 0xFE00) {
                float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
                if (_validateHumidity(hum)) {
                    _humidity = hum;
                    DEBUG_PRINTF("[SI7021Manager] Teste OK: RH=%.1f%%\n", hum);
                    return true;
                }
            }
        }
        delay(10);
    }
    return false;
}

// ============================================================================
// UPDATE
void SI7021Manager::update() {
    if (!_online) return;
    
    uint32_t currentTime = millis();
    
    if (_warmupProgress > 0) {
        _warmupProgress = (WARMUP_TIME - (currentTime - _initTime)) / 1000;
        if (_warmupProgress <= 0) _warmupProgress = 0;
        return;
    }
    
    if (currentTime - _lastReadTime < READ_INTERVAL) return;
    _lastReadTime = currentTime;
    
    // UMIDADE
    bool humSuccess = false;
    Wire.beginTransmission(_SI7021_ADDR);
    Wire.write(_CMD_MEASURE_RH_NOHOLD);
    if (Wire.endTransmission() == 0) {
        delay(25);
        Wire.requestFrom((uint8_t)_SI7021_ADDR, (uint8_t)3);  // ← FIX
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read();
            uint8_t lsb = Wire.read();
            if (Wire.available()) Wire.read();
            
            uint16_t rawHum = (msb << 8) | lsb;
            if (rawHum > 0x0100 && rawHum < 0xFE00) {
                float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
                if (_validateHumidity(hum)) {
                    _humidity = hum;
                    humSuccess = true;
                }
            }
        }
    }
    
    // TEMPERATURA
    bool tempSuccess = false;
    delay(10);
    Wire.beginTransmission(_SI7021_ADDR);
    Wire.write(_CMD_MEASURE_T_NOHOLD);
    if (Wire.endTransmission() == 0) {
        delay(25);
        Wire.requestFrom((uint8_t)_SI7021_ADDR, (uint8_t)2);  // ← FIX: 2 bytes temp
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read();
            uint8_t lsb = Wire.read();
            
            uint16_t rawTemp = (msb << 8) | lsb;
            if (rawTemp > 0x0100 && rawTemp < 0xFE00) {
                float temp = ((175.72 * rawTemp) / 65536.0) - 46.85;
                if (_validateTemperature(temp)) {
                    _temperature = temp;
                    _tempValid = true;
                    tempSuccess = true;
                }
            }
        }
    }
    
    if (!humSuccess || !tempSuccess) {
        _failCount++;
        if (_failCount >= 5) {
            DEBUG_PRINTLN("[SI7021Manager] OFFLINE (5 falhas)");
            _online = false;
        }
    } else {
        _failCount = 0;
    }
}

// ============================================================================
// GETTERS + STATUS
float SI7021Manager::getHumidity() const { return _online ? _humidity : NAN; }
float SI7021Manager::getTemperature() const { return _online && _tempValid ? _temperature : NAN; }
uint32_t SI7021Manager::getWarmupProgress() const { return _warmupProgress; }

void SI7021Manager::printStatus() const {
    DEBUG_PRINTF(" SI7021: %s", _online ? "ONLINE" : "OFFLINE");
    if (_online && _warmupProgress == 0) {
        DEBUG_PRINTF(" RH=%.1f%%", _humidity);
        if (_tempValid) DEBUG_PRINTF(" T=%.1f°C", _temperature);
    }
    DEBUG_PRINTLN();
}

void SI7021Manager::reset() {
    Wire.beginTransmission(_SI7021_ADDR);
    Wire.write(_CMD_SOFT_RESET);
    Wire.endTransmission();
    delay(100);
    _online = false;
    _failCount = 0;
}

bool SI7021Manager::_validateHumidity(float hum) {
    return hum >= 0.0f && hum <= 100.0f && !isnan(hum);
}

bool SI7021Manager::_validateTemperature(float temp) {
    return temp >= -40.0f && temp <= 125.0f && !isnan(temp);
}
