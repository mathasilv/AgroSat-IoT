/**
 * @file GPSManager.cpp
 * @brief Implementação com filtros de suavização e detecção de anomalias
 * @version 1.3.0
 */

#include "GPSManager.h"
#include <math.h>

GPSManager::GPSManager() 
    : _serial(nullptr), 
      _latitude(0.0), _longitude(0.0), 
      _prevLatitude(0.0), _prevLongitude(0.0),
      _altitude(0.0), _filteredAltitude(0.0),
      _hdop(99.0f), _speed(0.0f),
      _satellites(0), _hasFix(false), _isFirstFix(true),
      _lastEncoded(0), _lastValidFix(0)
{}

bool GPSManager::begin() {
    DEBUG_PRINTLN("[GPSManager] Inicializando GPS NEO-M8N...");
    
    _serial = &Serial2; 
    
    // Inicia com a velocidade confirmada e pinos do config.h
    _serial->begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    return true;
}

void GPSManager::update() {
    uint32_t now = millis();
    
    // Processa todos os caracteres disponíveis
    while (_serial->available() > 0) {
        char c = _serial->read();
        
        // Alimenta a biblioteca TinyGPS++ para processamento
        if (_gps.encode(c)) {
            _lastEncoded = now;
            
            // Atualiza contagem de satélites primeiro
            if (_gps.satellites.isValid()) {
                _satellites = _gps.satellites.value();
            }
            
            // Atualiza HDOP (qualidade do fix)
            if (_gps.hdop.isValid()) {
                _hdop = _gps.hdop.hdop();
            }
            
            // Verifica qualidade mínima antes de aceitar posição
            if (_satellites < MIN_SATELLITES) {
                DEBUG_PRINTF("[GPS] Poucos satelites: %d (min: %d)\n", _satellites, MIN_SATELLITES);
                continue; // Continua processando mas não atualiza posição
            }
            
            if (_hdop > MAX_HDOP) {
                DEBUG_PRINTF("[GPS] HDOP muito alto: %.1f (max: %.1f)\n", _hdop, MAX_HDOP);
                continue;
            }
            
            // Atualiza cache apenas se a localização for válida
            if (_gps.location.isValid()) {
                double lat = _gps.location.lat();
                double lng = _gps.location.lng();
                
                // Validação de range para coordenadas
                if (!_isValidCoordinate(lat, lng)) {
                    DEBUG_PRINTF("[GPS] Coordenadas invalidas: %.6f, %.6f\n", lat, lng);
                    _hasFix = false;
                    continue;
                }
                
                // Calcula delta tempo desde último fix válido
                uint32_t dtMs = now - _lastValidFix;
                
                // Detecção de salto anômalo (exceto no primeiro fix)
                if (!_isFirstFix && _isAnomalousJump(lat, lng, dtMs)) {
                    DEBUG_PRINTF("[GPS] Salto anomalo detectado! Ignorando leitura.\n");
                    continue;
                }
                
                // Salva posição anterior
                _prevLatitude = _latitude;
                _prevLongitude = _longitude;
                
                // Aplica filtro de suavização na posição (exceto primeiro fix)
                if (_isFirstFix) {
                    _latitude = lat;
                    _longitude = lng;
                    _isFirstFix = false;
                } else {
                    // Média móvel exponencial para suavizar
                    _latitude = _latitude + POSITION_FILTER_ALPHA * (lat - _latitude);
                    _longitude = _longitude + POSITION_FILTER_ALPHA * (lng - _longitude);
                }
                
                // Calcula velocidade baseada no deslocamento
                if (dtMs > 0 && _prevLatitude != 0.0) {
                    double dist = _haversineDistance(_prevLatitude, _prevLongitude, _latitude, _longitude);
                    _speed = (float)((dist / dtMs) * 3600.0); // m/ms -> km/h
                }
                
                _hasFix = true;
                _lastValidFix = now;
                
            } else {
                // Sentença válida mas sem fix (ainda a adquirir satélites)
                _hasFix = false; 
            }

            // Validação e filtro de altitude
            if (_gps.altitude.isValid()) {
                float alt = _gps.altitude.meters();
                if (alt >= -500.0f && alt <= 50000.0f) {
                    _altitude = alt;
                    // Aplica filtro de suavização na altitude
                    if (_filteredAltitude == 0.0f) {
                        _filteredAltitude = alt; // Primeiro valor
                    } else {
                        _filteredAltitude = _exponentialFilter(_filteredAltitude, alt, ALTITUDE_FILTER_ALPHA);
                    }
                }
            }
        }
    }

    // Timeout de segurança: Se passar 5s sem dados válidos, perde o fix
    if (now - _lastEncoded > 5000) {
        _hasFix = false;
    }
}

bool GPSManager::_isValidCoordinate(double lat, double lng) const {
    // Latitude: -90 a +90
    if (lat < -90.0 || lat > 90.0) return false;
    
    // Longitude: -180 a +180
    if (lng < -180.0 || lng > 180.0) return false;
    
    // Coordenadas (0,0) são suspeitas (ponto nulo no oceano)
    // Aceitar apenas se houver satélites suficientes
    if (lat == 0.0 && lng == 0.0) {
        if (_satellites < MIN_SATELLITES) return false;
    }
    
    return true;
}

double GPSManager::_haversineDistance(double lat1, double lon1, double lat2, double lon2) const {
    // Raio da Terra em metros
    constexpr double R = 6371000.0;
    
    // Converte para radianos
    double lat1Rad = lat1 * DEG_TO_RAD;
    double lat2Rad = lat2 * DEG_TO_RAD;
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    
    // Fórmula de Haversine
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1Rad) * cos(lat2Rad) *
               sin(dLon / 2.0) * sin(dLon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return R * c; // Distância em metros
}

bool GPSManager::_isAnomalousJump(double newLat, double newLng, uint32_t dtMs) {
    // Se não temos posição anterior válida, não é salto
    if (_latitude == 0.0 && _longitude == 0.0) return false;
    
    // Calcula distância do salto
    double dist = _haversineDistance(_latitude, _longitude, newLat, newLng);
    
    // Calcula velocidade implícita (m/s)
    if (dtMs == 0) dtMs = 1; // Evita divisão por zero
    double speedMs = dist / (dtMs / 1000.0);
    double speedKmh = speedMs * 3.6;
    
    // Se velocidade > MAX_SPEED_KMH, é salto anômalo
    if (speedKmh > MAX_SPEED_KMH) {
        DEBUG_PRINTF("[GPS] Velocidade impossivel: %.1f km/h (dist: %.1fm, dt: %lums)\n", 
                     speedKmh, dist, dtMs);
        return true;
    }
    
    return false;
}

float GPSManager::_exponentialFilter(float current, float newValue, float alpha) const {
    return current + alpha * (newValue - current);
}

uint32_t GPSManager::getLastFixAge() const {
    return millis() - _lastEncoded;
}
