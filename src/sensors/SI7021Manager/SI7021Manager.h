/**
 * @file SI7021Manager.h
 * @brief Gerenciador do SI7021 (Aplicação)
 * @details Controla tempo de leitura, validação de dados e reinicialização.
 */

#ifndef SI7021MANAGER_H
#define SI7021MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "SI7021.h" // Inclui o driver
#include "config.h"

class SI7021Manager {
public:
    SI7021Manager();

    bool begin();
    void update();
    void reset();

    // Getters
    float getTemperature() const { return _lastTemp; }
    float getHumidity() const { return _lastHum; }

    // Status
    bool isOnline() const { return _online; }
    bool isTempValid() const { return _online && !isnan(_lastTemp); }
    bool isHumValid() const { return _online && !isnan(_lastHum); }

private:
    SI7021 _si7021; // Instância do driver

    bool _online;
    float _lastTemp;
    float _lastHum;
    uint8_t _failCount;
    unsigned long _lastRead;

    // Limites de Validação
    static constexpr float TEMP_MIN = -40.0f;
    static constexpr float TEMP_MAX = 85.0f;
    static constexpr float HUM_MIN = 0.0f;
    static constexpr float HUM_MAX = 100.0f;
    
    static constexpr unsigned long READ_INTERVAL_MS = 2000; // Ler a cada 2s
};

#endif // SI7021MANAGER_H