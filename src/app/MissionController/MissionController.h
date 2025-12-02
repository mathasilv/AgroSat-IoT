/**
 * @file MissionController.h
 * @brief Controlador de ciclo de vida de missão HAB
 * @version 1.0.0
 * @date 2025-12-01
 * 
 * Responsabilidades:
 * - Gerenciar início/fim de missão
 * - Calcular duração e estatísticas
 * - Gerar relatórios de missão
 */

#ifndef MISSION_CONTROLLER_H
#define MISSION_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "core/RTCManager/RTCManager.h"
#include "core/DisplayManager/DisplayManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"

class MissionController {
public:
    /**
     * @brief Construtor
     * @param rtc Referência ao gerenciador RTC
     * @param display Referência ao display
     * @param nodes Referência ao gerenciador de nós terrestres
     */
    MissionController(
        RTCManager& rtc, 
        DisplayManager& display, 
        GroundNodeManager& nodes
    );
    
    /**
     * @brief Inicia uma nova missão
     * @return true se iniciou com sucesso, false se já estava em andamento
     */
    bool start();
    
    /**
     * @brief Encerra a missão atual e gera relatório
     * @return true se encerrou com sucesso, false se não havia missão ativa
     */
    bool stop();
    
    /**
     * @brief Verifica se há missão ativa
     * @return true se missão está em andamento
     */
    bool isActive() const { return _active; }
    
    /**
     * @brief Obtém duração da missão em milissegundos
     * @return Duração desde o início (0 se não iniciada)
     */
    uint32_t getDuration() const;
    
    /**
     * @brief Obtém timestamp UTC de início da missão
     * @return Unix timestamp UTC do início (0 se não iniciada)
     */
    uint32_t getStartTimestamp() const { return _startTimestampUTC; }
    
private:
    RTCManager& _rtc;
    DisplayManager& _display;
    GroundNodeManager& _nodes;
    
    bool _active;
    unsigned long _startTime;        // millis() do início
    uint32_t _startTimestampUTC;     // Unix timestamp UTC do início
    
    /**
     * @brief Imprime estatísticas finais da missão
     */
    void _printStatistics();
    
    /**
     * @brief Calcula estatísticas de link LoRa dos nós
     * @param avgRSSI RSSI médio (saída)
     * @param bestRSSI Melhor RSSI (saída)
     * @param worstRSSI Pior RSSI (saída)
     * @param avgSNR SNR médio (saída)
     * @param packetLossRate Taxa de perda de pacotes % (saída)
     */
    void _calculateLinkStats(
        int& avgRSSI, 
        int& bestRSSI, 
        int& worstRSSI,
        float& avgSNR,
        float& packetLossRate
    );
};

#endif // MISSION_CONTROLLER_H
