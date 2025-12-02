/**
 * @file SI7021Manager.cpp
 * @brief Implementação limpa do Manager SI7021
 */

#include "SI7021Manager.h"

SI7021Manager::SI7021Manager()
    : _si7021(Wire),
      _online(false),
      _lastTemp(NAN),
      _lastHum(NAN),
      _failCount(0),
      _lastRead(0) 
{}

bool SI7021Manager::begin() {
    DEBUG_PRINTLN("[SI7021Manager] Inicializando...");

    _online = false;
    _failCount = 0;

    // 1. Tentar iniciar o driver
    if (!_si7021.begin()) {
        DEBUG_PRINTLN("[SI7021Manager] ERRO: Sensor não detectado (ACK Falhou).");
        return false;
    }

    // 2. Leitura de teste imediata
    float t, h;
    delay(100); // Estabilização inicial
    if (_si7021.readTemperature(t) && _si7021.readHumidity(h)) {
        _lastTemp = t;
        _lastHum = h;
        _online = true;
        DEBUG_PRINTF("[SI7021Manager] OK! T=%.2f C, RH=%.2f %%\n", t, h);
        return true;
    }

    DEBUG_PRINTLN("[SI7021Manager] Aviso: Detectado, mas leitura inicial falhou.");
    return false;
}

void SI7021Manager::update() {
    if (!_online) return;

    // Rate Limiting (respeita o READ_INTERVAL_MS)
    if (millis() - _lastRead < READ_INTERVAL_MS) return;
    _lastRead = millis();

    float t, h;
    bool successT = _si7021.readTemperature(t);
    bool successH = _si7021.readHumidity(h);

    if (successT && successH) {
        // Validação de Range
        if (t >= TEMP_MIN && t <= TEMP_MAX && h >= HUM_MIN && h <= HUM_MAX) {
            _lastTemp = t;
            _lastHum = h;
            _failCount = 0;
            return; // Sucesso
        } else {
            DEBUG_PRINTLN("[SI7021Manager] Dados fora do range válido.");
        }
    } else {
        DEBUG_PRINTLN("[SI7021Manager] Falha de leitura I2C.");
    }

    // Gerenciamento de Falhas
    _failCount++;
    if (_failCount >= 5) {
        DEBUG_PRINTLN("[SI7021Manager] 5 falhas consecutivas. Resetando...");
        reset();
    }
}

void SI7021Manager::reset() {
    _si7021.reset();
    _failCount = 0;
    _online = false;
    
    // Tenta reconectar imediatamente
    delay(50);
    begin();
}