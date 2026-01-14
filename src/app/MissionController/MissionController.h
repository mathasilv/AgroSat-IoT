/**
 * @file MissionController.h
 * @brief Controlador de ciclo de vida de missão com persistência
 * 
 * @details Gerencia o ciclo de vida completo de uma missão de coleta:
 *          - Transições de estado (PREFLIGHT → FLIGHT → POSTFLIGHT)
 *          - Persistência de estado na NVS para recuperação após reset
 *          - Cálculo de estatísticas de link (RSSI, SNR, packet loss)
 *          - Timestamps UTC para sincronização
 *          - Integração com GroundNodeManager para estatísticas
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Estados de Missão
 * ```
 * ┌──────────┐    start()    ┌──────────┐    stop()     ┌────────────┐
 * │ PREFLIGHT│──────────────►│  FLIGHT  │──────────────►│ POSTFLIGHT │
 * └──────────┘               └──────────┘               └────────────┘
 *      │                          │                           │
 *      │         timeout/         │                           │
 *      │         error            │                           │
 *      │◄─────────────────────────┘                           │
 *      │◄─────────────────────────────────────────────────────┘
 * ```
 * 
 * ## Persistência (NVS)
 * | Chave         | Tipo     | Descrição              |
 * |---------------|----------|------------------------|
 * | mission_active| bool     | Missão em andamento    |
 * | start_time    | uint32_t | Timestamp de início    |
 * 
 * @see GroundNodeManager para estatísticas de nós
 * @see RTCManager para timestamps UTC
 */

#ifndef MISSION_CONTROLLER_H
#define MISSION_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Forward declarations
class RTCManager;
class GroundNodeManager;

/**
 * @class MissionController
 * @brief Controlador de estado e ciclo de vida de missão
 */
class MissionController {
public:
    /**
     * @brief Construtor com dependências injetadas
     * @param rtc Referência ao RTCManager para timestamps
     * @param nodes Referência ao GroundNodeManager para estatísticas
     */
    MissionController(RTCManager& rtc, GroundNodeManager& nodes);
    
    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o controlador e restaura estado da NVS
     * @return true se inicializado (pode restaurar missão ativa)
     */
    bool begin();
    
    /**
     * @brief Inicia uma nova missão
     * @return true se transição para FLIGHT bem sucedida
     * @return false se já existe missão ativa
     */
    bool start();
    
    /**
     * @brief Finaliza a missão atual
     * @return true se transição para POSTFLIGHT bem sucedida
     * @return false se não há missão ativa
     */
    bool stop();
    
    //=========================================================================
    // GETTERS DE STATUS
    //=========================================================================
    
    /** @brief Missão está ativa (modo FLIGHT)? */
    bool isActive() const { return _active; }
    
    /**
     * @brief Retorna duração da missão em milissegundos
     * @return Tempo desde start() ou 0 se inativa
     */
    uint32_t getDuration() const;
    
    /** @brief Retorna timestamp UTC de início da missão */
    uint32_t getStartTimestamp() const { return _startTimestampUTC; }
    
private:
    //=========================================================================
    // DEPENDÊNCIAS
    //=========================================================================
    RTCManager& _rtc;            ///< Referência ao RTC para timestamps
    GroundNodeManager& _nodes;   ///< Referência aos ground nodes
    Preferences _prefs;          ///< Handle para NVS
    
    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _active;                ///< Missão ativa?
    unsigned long _startTime;    ///< millis() no início
    uint32_t _startTimestampUTC; ///< Unix timestamp de início
    
    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /** @brief Imprime estatísticas da missão no Serial */
    void _printStatistics();
    
    /** @brief Salva estado atual na NVS */
    void _saveState();
    
    /** @brief Limpa estado da NVS */
    void _clearState();
    
    /**
     * @brief Calcula estatísticas de link dos ground nodes
     * @param[out] avgRSSI RSSI médio (dBm)
     * @param[out] bestRSSI Melhor RSSI (dBm)
     * @param[out] worstRSSI Pior RSSI (dBm)
     * @param[out] avgSNR SNR médio (dB)
     * @param[out] packetLossRate Taxa de perda de pacotes (%)
     */
    void _calculateLinkStats(int& avgRSSI, int& bestRSSI, int& worstRSSI,
                             float& avgSNR, float& packetLossRate);
};

#endif