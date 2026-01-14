/**
 * @file GPSManager.h
 * @brief Gerenciador do módulo GPS NEO-M8N/NEO-6M
 * 
 * @details Driver para módulos GPS u-blox com suporte a:
 *          - Parsing de sentenças NMEA via TinyGPS++
 *          - Coordenadas geográficas (latitude/longitude)
 *          - Altitude GPS (acima do nível do mar)
 *          - Contagem de satélites visíveis
 *          - Validação de coordenadas (range check)
 *          - Detecção de perda de fix
 *          - Filtro de suavização (média móvel exponencial)
 *          - Detecção de saltos anômalos
 *          - Verificação de HDOP (qualidade do fix)
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.3.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Módulos Suportados
 * | Modelo    | Frequência | Canais | Precisão  |
 * |-----------|------------|--------|----------|
 * | NEO-6M    | 5 Hz       | 50     | 2.5m CEP |
 * | NEO-M8N   | 10 Hz      | 72     | 2.0m CEP |
 * | NEO-M9N   | 25 Hz      | 92     | 1.5m CEP |
 * 
 * ## Sentenças NMEA Processadas
 * - **GGA**: Posição e altitude
 * - **RMC**: Posição, velocidade e data/hora
 * - **GSV**: Satélites visíveis
 * 
 * @note Usa Serial2 (UART1) do ESP32
 * @note Baudrate padrão: 9600 (configurável em config.h)
 */

#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config.h"

/**
 * @class GPSManager
 * @brief Driver para módulos GPS u-blox (NEO-6M/M8N/M9N)
 */
class GPSManager {
public:
    /**
     * @brief Construtor padrão
     */
    GPSManager();

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa a UART nos pinos definidos em config.h
     * @return true sempre (Serial.begin não retorna erro no ESP32)
     * @note Usa Serial2 com pinos GPS_RX_PIN e GPS_TX_PIN
     */
    bool begin();

    /**
     * @brief Processa dados NMEA da Serial
     * @note Deve ser chamado frequentemente (idealmente a cada 100ms)
     */
    void update();

    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @brief Retorna latitude em graus decimais (-90 a +90) */
    double getLatitude() const { return _latitude; }
    
    /** @brief Retorna longitude em graus decimais (-180 a +180) */
    double getLongitude() const { return _longitude; }
    
    /** @brief Retorna altitude GPS em metros (acima do nível do mar) */
    float getAltitude() const { return _altitude; }
    
    /** @brief Retorna número de satélites usados no fix */
    uint8_t getSatellites() const { return _satellites; }
    
    //=========================================================================
    // STATUS
    //=========================================================================
    
    /**
     * @brief Verifica se há fix GPS válido
     * @return true se posição válida e recente
     * @return false se sem fix ou coordenadas inválidas
     */
    bool hasFix() const { return _hasFix; }
    
private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    TinyGPSPlus _gps;            ///< Parser NMEA TinyGPS++
    HardwareSerial* _serial;     ///< Ponteiro para Serial2

    //=========================================================================
    // CACHE DE DADOS
    //=========================================================================
    double _latitude;            ///< Latitude em graus decimais
    double _longitude;           ///< Longitude em graus decimais
    double _prevLatitude;        ///< Latitude anterior (para detecção de saltos)
    double _prevLongitude;       ///< Longitude anterior (para detecção de saltos)
    float _altitude;             ///< Altitude GPS raw (m)
    float _filteredAltitude;     ///< Altitude filtrada/suavizada (m)
    float _hdop;                 ///< Horizontal Dilution of Precision
    float _speed;                ///< Velocidade calculada (km/h)
    uint8_t _satellites;         ///< Satélites no fix
    bool _hasFix;                ///< Fix válido?
    bool _isFirstFix;            ///< Primeiro fix após inicialização?
    
    uint32_t _lastEncoded;       ///< Timestamp última decodificação
    uint32_t _lastValidFix;      ///< Timestamp último fix válido
    
    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /**
     * @brief Valida coordenadas contra limites físicos
     * @param lat Latitude (-90 a +90)
     * @param lng Longitude (-180 a +180)
     * @return true se coordenadas válidas
     * @note Rejeita (0,0) se poucos satélites
     */
    bool _isValidCoordinate(double lat, double lng) const;
    
    /**
     * @brief Calcula distância entre dois pontos usando fórmula de Haversine
     * @param lat1 Latitude ponto 1
     * @param lon1 Longitude ponto 1
     * @param lat2 Latitude ponto 2
     * @param lon2 Longitude ponto 2
     * @return Distância em metros
     */
    double _haversineDistance(double lat1, double lon1, double lat2, double lon2) const;
    
    /**
     * @brief Detecta se houve um salto anômalo na posição
     * @param newLat Nova latitude
     * @param newLng Nova longitude
     * @param dtMs Delta tempo em milissegundos
     * @return true se o salto é anômalo (velocidade impossível)
     */
    bool _isAnomalousJump(double newLat, double newLng, uint32_t dtMs);
    
    /**
     * @brief Aplica filtro de média móvel exponencial
     * @param current Valor atual filtrado
     * @param newValue Novo valor raw
     * @param alpha Fator de suavização (0-1, menor = mais suave)
     * @return Valor filtrado
     */
    float _exponentialFilter(float current, float newValue, float alpha) const;
    
    //=========================================================================
    // CONSTANTES DE CONFIGURAÇÃO
    //=========================================================================
    static constexpr uint8_t MIN_SATELLITES = 4;       ///< Mínimo de satélites para fix
    static constexpr float MAX_HDOP = 5.0f;            ///< HDOP máximo aceitável
    static constexpr float MAX_SPEED_KMH = 500.0f;     ///< Velocidade máxima plausível (km/h)
    static constexpr float ALTITUDE_FILTER_ALPHA = 0.3f; ///< Suavização altitude (0.3 = 30% novo)
    static constexpr float POSITION_FILTER_ALPHA = 0.5f; ///< Suavização posição
};

#endif
