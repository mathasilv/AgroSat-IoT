/**
 * @file GPSManager.cpp
 * @brief Implementação do Gerenciador GPS
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
    
    // Configura UART1 com os pinos remapeados no config.h
    // RX = 34, TX = 12
    _serial = &Serial1;
    _serial->begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    return true;
}

void GPSManager::update() {
    // Processa todos os caracteres disponíveis no buffer da Serial
    while (_serial->available() > 0) {
        char c = _serial->read();
        
        // encode() retorna true quando uma nova sentença válida é completada
        if (_gps.encode(c)) {
            _lastEncoded = millis();
            
            // Atualiza cache apenas se a localização for válida
            if (_gps.location.isValid()) {
                _latitude = _gps.location.lat();
                _longitude = _gps.location.lng();
                _hasFix = true;
            } else {
                // Se a sentença é válida mas sem location (ex: satélites insuficientes),
                // mantemos o valor anterior mas marcamos flag se necessário.
                // Aqui optamos por manter _hasFix true se os dados forem recentes (ver getLastFixAge)
                // ou confiar no isValid() instantâneo:
                _hasFix = _gps.location.isValid(); 
            }

            if (_gps.altitude.isValid()) {
                _altitude = _gps.altitude.meters();
            }

            if (_gps.satellites.isValid()) {
                _satellites = _gps.satellites.value();
            }
        }
    }

    // Lógica de Timeout para perder o FIX
    // Se não recebermos dados válidos por 5 segundos, consideramos sem fix
    if (millis() - _lastEncoded > 5000) {
        _hasFix = false;
    }
}

// CORREÇÃO: Adicionado 'const' para coincidir com o header
uint32_t GPSManager::getLastFixAge() const {
    return millis() - _lastEncoded;
}