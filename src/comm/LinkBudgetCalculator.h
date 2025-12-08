/**
 * @file LinkBudgetCalculator.h
 * @brief Cálculo de Link Budget para LoRa Satélite LEO
 * @version 1.0.0 (MODERADO 4.2 - Link Budget Implementado)
 */

#ifndef LINK_BUDGET_CALCULATOR_H
#define LINK_BUDGET_CALCULATOR_H

#include <Arduino.h>
#include <math.h>
#include "config.h"

class LinkBudgetCalculator {
public:
    LinkBudgetCalculator();
    
    /**
     * @brief Calcula link budget baseado em posição GPS e parâmetros LoRa
     * @param satLat Latitude do satélite (graus)
     * @param satLon Longitude do satélite (graus)
     * @param satAlt Altitude do satélite (km)
     * @param groundLat Latitude do nó terrestre (graus)
     * @param groundLon Longitude do nó terrestre (graus)
     * @param spreadingFactor SF usado (7-12)
     * @param bandwidth Largura de banda (Hz)
     * @return Estrutura LinkBudget preenchida
     */
    LinkBudget calculate(
        double satLat, double satLon, float satAlt,
        double groundLat, double groundLon,
        uint8_t spreadingFactor = 7,
        uint32_t bandwidth = 125000
    );
    
    /**
     * @brief Recomenda o SF baseado na distância calculada
     * @param distance Distância em km
     * @return SF recomendado (7-12)
     */
    static int8_t recommendSF(float distance);
    
    /**
     * @brief Retorna o último link budget calculado
     */
    LinkBudget getLastBudget() const { return _lastBudget; }
    
private:
    LinkBudget _lastBudget;
    
    // Cálculo de distância (Haversine)
    float _calculateDistance(double lat1, double lon1, double lat2, double lon2);
    
    // Cálculo de Path Loss (Free Space Path Loss)
    float _calculatePathLoss(float distanceKm, uint32_t frequencyHz);
    
    // Cálculo de Sensibilidade do Receptor LoRa
    float _calculateRxSensitivity(uint8_t sf, uint32_t bandwidth);
    
    // Constantes
    static constexpr float TX_POWER_DBM = LORA_TX_POWER;          // 20 dBm
    static constexpr float ANTENNA_GAIN_DBM = 2.15f;              // Dipolo
    static constexpr uint32_t FREQUENCY_HZ = LORA_FREQUENCY;      // 915 MHz
    static constexpr float SPEED_OF_LIGHT = 299792458.0f;         // m/s
};

#endif