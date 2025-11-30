/**
 * @file CCS811Manager.h
 * @brief Gerenciador dedicado do sensor CCS811 - Módulo isolado
 * @version 1.0.0
 * @date 2025-11-30
 * 
 * CARACTERÍSTICAS:
 * - Usa biblioteca CCS811 nativa (sem Adafruit)
 * - Validação automática de dados (eCO2: 400-8192 ppm, TVOC: 0-1187 ppb)
 * - Warm-up obrigatório de 20 segundos
 * - Compensação ambiental automática (temperatura + umidade)
 * - Baseline management (save/restore)
 * - Interface limpa para integração com SensorManager
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
    
    // Inicialização e controle
    bool begin();
    void update();
    void reset();
    
    // Getters principais
    uint16_t geteCO2() const { return _eco2; }   // ppm
    uint16_t getTVOC() const { return _tvoc; }   // ppb
    
    // Compensação ambiental
    bool setEnvironmentalData(float humidity, float temperature);
    
    // Baseline (calibração)
    bool getBaseline(uint16_t& baseline);
    bool setBaseline(uint16_t baseline);
    
    // Status
    bool isOnline() const { return _online; }
    bool isWarmupComplete() const;
    uint32_t getWarmupProgress() const; // 0-100%
    
    // Diagnóstico
    void printStatus() const;
    bool checkError();
    uint8_t getErrorCode();
    
private:
    // Hardware
    CCS811 _ccs811;
    
    // Dados atuais
    uint16_t _eco2;  // ppm
    uint16_t _tvoc;  // ppb
    
    // Estado
    bool _online;
    unsigned long _initTime;
    unsigned long _lastReadTime;
    
    // Warm-up (20 minutos para melhor precisão, mas funcional após 20s)
    static constexpr unsigned long WARMUP_MINIMUM = 20000;  // 20s (funcional)
    static constexpr unsigned long WARMUP_OPTIMAL = 1200000; // 20 min (ideal)
    
    // Intervalo de leitura (não ler mais rápido que a taxa de medição)
    static constexpr unsigned long READ_INTERVAL = 5000; // 5s
    
    // Limites de validação
    static constexpr uint16_t ECO2_MIN = 400;
    static constexpr uint16_t ECO2_MAX = 8192;
    static constexpr uint16_t TVOC_MAX = 1187;
    
    // Métodos internos
    bool _initSensor();
    bool _performWarmup();
    bool _validateData(uint16_t eco2, uint16_t tvoc) const;
};

#endif // CCS811MANAGER_H
