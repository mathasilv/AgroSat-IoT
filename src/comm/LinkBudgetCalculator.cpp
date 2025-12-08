/**
 * @file LinkBudgetCalculator.cpp
 * @brief Implementação de Link Budget LoRa-Satélite
 */

#include "LinkBudgetCalculator.h"

LinkBudgetCalculator::LinkBudgetCalculator() {
    memset(&_lastBudget, 0, sizeof(LinkBudget));
}

LinkBudget LinkBudgetCalculator::calculate(
    double satLat, double satLon, float satAlt,
    double groundLat, double groundLon,
    uint8_t spreadingFactor,
    uint32_t bandwidth
) {
    LinkBudget budget;
    
    // 1. Calcular distância entre satélite e nó terrestre
    budget.currentDistance = _calculateDistance(satLat, satLon, groundLat, groundLon);
    budget.maxDistance = MAX_COMM_DISTANCE_KM;
    
    // 2. Calcular Path Loss (FSPL)
    budget.pathLoss = _calculatePathLoss(budget.currentDistance, FREQUENCY_HZ);
    
    // 3. Calcular sensibilidade do receptor LoRa
    float rxSensitivity = _calculateRxSensitivity(spreadingFactor, bandwidth);
    
    // 4. Calcular Link Budget
    // Margin = TxPower + TxGain + RxGain - PathLoss - RxSensitivity
    float txGain = ANTENNA_GAIN_DBM;
    float rxGain = ANTENNA_GAIN_DBM;
    
    budget.linkMargin = TX_POWER_DBM + txGain + rxGain - budget.pathLoss - fabs(rxSensitivity);
    
    // 5. Verificar viabilidade (margem > 3dB)
    budget.isViable = (budget.linkMargin > LINK_MARGIN_MIN_DB);
    
    // 6. Recomendar SF baseado na distância
    budget.recommendedSF = recommendSF(budget.currentDistance);
    
    _lastBudget = budget;
    
    DEBUG_PRINTF("[LinkBudget] Dist=%.1f km | PathLoss=%.1f dB | Margin=%.1f dB | SF=%d\n",
                 budget.currentDistance, budget.pathLoss, budget.linkMargin, budget.recommendedSF);
    
    return budget;
}

// ============================================================================
// Cálculo de Distância (Fórmula de Haversine)
// ============================================================================
float LinkBudgetCalculator::_calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    // Converter para radianos
    double lat1R = lat1 * PI / 180.0;
    double lat2R = lat2 * PI / 180.0;
    double dLat = (lat2 - lat1) * PI / 180.0;
    double dLon = (lon2 - lon1) * PI / 180.0;
    
    // Haversine
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1R) * cos(lat2R) *
               sin(dLon/2) * sin(dLon/2);
    
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    // Distância na superfície da Terra
    double distanceSurface = EARTH_RADIUS_KM * c;
    
    // Adicionar altitude do satélite (Pitágoras aproximado para LEO)
    double distance = sqrt(distanceSurface * distanceSurface + 
                           ORBITAL_ALTITUDE_KM * ORBITAL_ALTITUDE_KM);
    
    return (float)distance;
}

// ============================================================================
// Cálculo de Path Loss (Free Space Path Loss - FSPL)
// ============================================================================
float LinkBudgetCalculator::_calculatePathLoss(float distanceKm, uint32_t frequencyHz) {
    // FSPL (dB) = 20*log10(d) + 20*log10(f) + 20*log10(4π/c)
    // Simplificado: FSPL = 32.45 + 20*log10(d_km) + 20*log10(f_MHz)
    
    float distanceM = distanceKm * 1000.0f;
    float frequencyMHz = frequencyHz / 1000000.0f;
    
    float fspl = 20.0f * log10(distanceM) + 
                 20.0f * log10(frequencyMHz) - 
                 147.55f;  // Constante (20*log10(4π/c))
    
    return fspl;
}

// ============================================================================
// Sensibilidade do Receptor LoRa (baseado no datasheet SX1276)
// ============================================================================
float LinkBudgetCalculator::_calculateRxSensitivity(uint8_t sf, uint32_t bandwidth) {
    // Valores típicos SX1276 @ 915MHz (datasheet Semtech)
    // Sensibilidade degrada com SF menor e BW maior
    
    float sensitivity = -174.0f + 10.0f * log10(bandwidth) + 6.0f + (sf - 6) * 2.5f;
    
    // Valores práticos conhecidos (BW = 125kHz):
    // SF7: -124 dBm | SF8: -127 dBm | SF9: -130 dBm
    // SF10: -133 dBm | SF11: -135 dBm | SF12: -137 dBm
    
    return sensitivity;
}

// ============================================================================
// Recomendação de SF baseado na Distância
// ============================================================================
int8_t LinkBudgetCalculator::recommendSF(float distance) {
    // Tabela empírica baseada em testes LoRa-Satélite (papers)
    
    if (distance < 500.0f) {
        return 7;   // Curta distância: SF7 (mais rápido)
    } else if (distance < 800.0f) {
        return 8;
    } else if (distance < 1100.0f) {
        return 9;
    } else if (distance < 1400.0f) {
        return 10;
    } else if (distance < 1800.0f) {
        return 11;
    } else {
        return 12;  // Longa distância: SF12 (mais confiável)
    }
}