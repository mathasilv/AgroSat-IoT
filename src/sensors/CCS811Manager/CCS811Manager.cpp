/**
 * @file CCS811Manager.cpp
 * @brief Implementação limpa do Manager CCS811
 */

#include "CCS811Manager.h"
#include <Preferences.h>

CCS811Manager::CCS811Manager() 
    : _ccs811(Wire), _online(false), _eco2(0), _tvoc(0), 
      _initTime(0), _lastRead(0), _failCount(0) {}

bool CCS811Manager::begin() {
    DEBUG_PRINTLN("[CCS811Manager] Inicializando...");

    // Tenta endereço padrão 0x5A
    if (_ccs811.begin(CCS811::ADDR_5A)) {
        DEBUG_PRINTLN("[CCS811Manager] Detectado em 0x5A.");
        _online = true;
    } 
    // Tenta secundário 0x5B
    else if (_ccs811.begin(CCS811::ADDR_5B)) {
        DEBUG_PRINTLN("[CCS811Manager] Detectado em 0x5B.");
        _online = true;
    } 
    else {
        DEBUG_PRINTLN("[CCS811Manager] ERRO: Sensor não encontrado.");
        _online = false;
        return false;
    }

    _initTime = millis();
    _failCount = 0;
    
    // Tenta restaurar baseline silenciosamente
    restoreBaseline();
    
    return true;
}

void CCS811Manager::update() {
    if (!_online) return;

    if (millis() - _lastRead < READ_INTERVAL) return;
    _lastRead = millis();

    // Só tenta ler se tiver dados prontos
    if (_ccs811.available()) {
        if (_ccs811.readData()) {
            _eco2 = _ccs811.geteCO2();
            _tvoc = _ccs811.getTVOC();
            _failCount = 0;
        } else {
            _failCount++;
        }
    }

    if (_failCount > 20) {
        DEBUG_PRINTLN("[CCS811Manager] Falhas excessivas. Resetando...");
        reset();
    }
}

void CCS811Manager::reset() {
    _ccs811.reset();
    _online = false;
    delay(100);
    begin();
}

bool CCS811Manager::isDataValid() const {
    return _online && isWarmupComplete();
}

bool CCS811Manager::isDataReliable() const {
    return isDataValid();
}

bool CCS811Manager::isWarmupComplete() const {
    if (_initTime == 0) return false;
    return (millis() - _initTime) > WARMUP_TIME;
}

void CCS811Manager::setEnvironmentalData(float hum, float temp) {
    if (_online) {
        _ccs811.setEnvironmentalData(hum, temp);
    }
}

bool CCS811Manager::saveBaseline() {
    if (!_online) return false;
    
    uint16_t baseline;
    if (_ccs811.getBaseline(baseline)) {
        Preferences prefs;
        if (prefs.begin("ccs811", false)) { 
            prefs.putUShort("base", baseline);
            prefs.end();
            DEBUG_PRINTF("[CCS811Manager] Baseline salvo: 0x%04X\n", baseline);
            return true;
        }
    }
    return false;
}

bool CCS811Manager::restoreBaseline() {
    if (!_online) return false;

    Preferences prefs;
    if (prefs.begin("ccs811", true)) { 
        if (prefs.isKey("base")) {
            uint16_t baseline = prefs.getUShort("base");
            prefs.end();
            _ccs811.setBaseline(baseline);
            DEBUG_PRINTF("[CCS811Manager] Baseline restaurado: 0x%04X\n", baseline);
            return true;
        }
        prefs.end();
    }
    return false;
}
