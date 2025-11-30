/**
 * @file SI7021Manager.cpp
 * @brief SI7021Manager - Implementação do gerenciador
 */
#include "SI7021Manager.h"

SI7021Manager::SI7021Manager() 
    : _si7021(Wire), 
      _online(false), 
      _lastTemp(-999.0), 
      _lastHum(-999.0),
      _failCount(0),
      _lastRead(0),
      _initTime(0),
      _warmupProgress(0) {}

bool SI7021Manager::begin() {
    DEBUG_PRINTLN("[SI7021Manager] ========================================");
    DEBUG_PRINTLN("[SI7021Manager] Inicializando SI7021Manager...");
    
    _initTime = millis();
    _warmupProgress = WARMUP_TIME_MS / 1000;

    // Inicializar driver nativo
    if (!_si7021.begin()) {
        DEBUG_PRINTLN("[SI7021Manager] ❌ Falha ao inicializar driver");
        _online = false;
        return false;
    }

    // Driver OK → considera online, mesmo que a primeira leitura não seja perfeita
    _online = true;

    // Leitura inicial (opcional, só para preencher cache)
    float temp, hum;
    if (_si7021.readTemperature(temp) && _si7021.readHumidity(hum)) {
        _lastTemp = temp;
        _lastHum  = hum;
        DEBUG_PRINTF("[SI7021Manager] ✅ OK: T=%.1f°C RH=%.1f%%\n", temp, hum);
    } else {
        DEBUG_PRINTLN("[SI7021Manager] ⚠ Leitura inicial falhou (mantendo cache inválido)");
        _lastTemp = -999.0f;
        _lastHum  = -999.0f;
    }

    DEBUG_PRINTLN("[SI7021Manager] INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[SI7021Manager] ========================================");
    return true;
}

void SI7021Manager::update() {
    if (!_online) return;
    
    uint32_t now = millis();
    
    // Countdown do warmup
    if (_warmupProgress > 0) {
        uint32_t elapsed = (now - _initTime) / 1000;
        _warmupProgress = (WARMUP_TIME_MS / 1000 > elapsed) ? (WARMUP_TIME_MS / 1000 - elapsed) : 0;
        return;
    }
    
    if (now - _lastRead < READ_INTERVAL_MS) return;
    
    _lastRead = now;
    
    // Ler temperatura e umidade
    float temp, hum;
    bool tempOk = _si7021.readTemperature(temp);
    bool humOk = _si7021.readHumidity(hum);
    
    if (tempOk && humOk) {
        _lastTemp = temp;
        _lastHum = hum;
        _failCount = 0;
    } else {
        _failCount++;
        
        if (_failCount >= 5) {
            DEBUG_PRINTLN("[SI7021Manager] ⚠ 5 falhas consecutivas - OFFLINE");
            _online = false;
        }
    }
}

void SI7021Manager::reset() {
    DEBUG_PRINTLN("[SI7021Manager] Reset...");
    _si7021.reset();
    delay(50);
    
    _online = false;
    _failCount = 0;
    _lastTemp = -999.0;
    _lastHum = -999.0;
    _warmupProgress = 0;
    
    // Reinicializar
    begin();
}

void SI7021Manager::printStatus() const {
    if (_online) {
        if (_warmupProgress > 0) {
            DEBUG_PRINTF(" SI7021: ONLINE (warm-up: %lus)\n", _warmupProgress);
        } else {
            DEBUG_PRINTF(" SI7021: ONLINE (T=%.1f°C H=%.1f%% Erros=%d)\n", 
                        _lastTemp, _lastHum, _failCount);
        }
    } else {
        DEBUG_PRINTLN(" SI7021: OFFLINE");
    }
}
