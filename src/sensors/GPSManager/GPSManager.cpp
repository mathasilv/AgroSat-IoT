/**
 * @file GPSManager.cpp
 * @brief Implementação com Monitor Serial Ativo (Modo Espião)
 */

#include "GPSManager.h"

GPSManager::GPSManager() 
    : _serial(nullptr), 
      _latitude(0.0), _longitude(0.0), 
      _altitude(0.0), _satellites(0), 
      _hasFix(false), _lastEncoded(0)
{}

bool GPSManager::begin() {
    DEBUG_PRINTLN("[GPSManager] Inicializando GPS NEO-M8N...");
    
    // ============================================================
    // CORREÇÃO CRÍTICA: Usar Serial2 para evitar conflito de pinos
    // ============================================================
    _serial = &Serial2; 
    
    // Inicia com a velocidade confirmada e pinos do config.h
    _serial->begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    return true;
}

void GPSManager::update() {
    // Processa todos os caracteres disponíveis
    while (_serial->available() > 0) {
        char c = _serial->read();
        
        // ============================================================
        // MONITOR SERIAL (MODO ESPIÃO)
        // Imprime no PC tudo o que o GPS envia (raw data)
        // ============================================================
        Serial.print(c); 
        // ============================================================
        
        // Alimenta a biblioteca TinyGPS++ para processamento
        if (_gps.encode(c)) {
            _lastEncoded = millis();
            
            // Atualiza cache apenas se a localização for válida
            if (_gps.location.isValid()) {
                _latitude = _gps.location.lat();
                _longitude = _gps.location.lng();
                _hasFix = true;
            } else {
                // Sentença válida mas sem fix (ainda a adquirir satélites)
                // Mantemos o estado anterior ou marcamos como false se preferires
                _hasFix = false; 
            }

            if (_gps.altitude.isValid()) _altitude = _gps.altitude.meters();
            if (_gps.satellites.isValid()) _satellites = _gps.satellites.value();
        }
    }

    // Timeout de segurança: Se passar 5s sem dados válidos, perde o fix
    if (millis() - _lastEncoded > 5000) {
        _hasFix = false;
    }
}

uint32_t GPSManager::getLastFixAge() const {
    return millis() - _lastEncoded;
}