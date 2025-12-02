/**
 * @file CCS811Manager.h
 * @brief Gerenciador CCS811 (Aplicação)
 */

#ifndef CCS811MANAGER_H
#define CCS811MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "CCS811.h"
#include "config.h"

class CCS811Manager {
public:
    CCS811Manager();

    bool begin();
    void update();
    void reset();

    // Getters
    uint16_t geteCO2() const { return _eco2; }
    uint16_t getTVOC() const { return _tvoc; }
    
    // Status
    bool isOnline() const { return _online; }
    bool isDataValid() const;      // Verifica se passou warmup e está online
    bool isDataReliable() const;   // Alias para isDataValid (para compatibilidade)
    bool isWarmupComplete() const; // Verifica apenas o tempo

    // Compensação e Calibração
    void setEnvironmentalData(float hum, float temp);
    bool saveBaseline();
    bool restoreBaseline();

    void printStatus() const;

private:
    CCS811 _ccs811;

    bool _online;
    uint16_t _eco2;
    uint16_t _tvoc;
    unsigned long _initTime;
    unsigned long _lastRead;
    
    // Controle de erros
    uint8_t _failCount;

    static constexpr unsigned long READ_INTERVAL = 2000;
    static constexpr unsigned long WARMUP_TIME = 20000; // 20s para dados úteis
};

#endif