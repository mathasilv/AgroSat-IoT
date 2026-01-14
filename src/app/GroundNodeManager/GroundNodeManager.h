/**
 * @file GroundNodeManager.h
 * @brief Gerenciador de nós terrestres (ground nodes) da rede LoRa
 * 
 * @details Gerencia o buffer de ground nodes recebidos via LoRa:
 *          - Armazenamento de até MAX_GROUND_NODES nós simultâneos
 *          - Atualização de dados com cálculo de prioridade QoS
 *          - Limpeza automática de nós expirados (TTL)
 *          - Substituição inteligente por prioridade
 *          - Controle de flags de forwarding
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.2.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Estrutura do Buffer
 * ```
 * GroundNodeBuffer
 * ├── nodes[MAX_GROUND_NODES]  // Array de MissionData
 * ├── activeNodes              // Contador de nós ativos
 * ├── lastUpdate[]             // Timestamps por slot
 * └── totalPacketsCollected    // Estatística global
 * ```
 * 
 * ## Prioridade QoS
 * | Nível    | Valor | Condição                           |
 * |----------|--------------------------------------------|
 * | CRITICAL | 0     | Solo seco/encharcado, temp extrema |
 * | HIGH     | 1     | Link ruim, perdas, irrigação ativa |
 * | NORMAL   | 2     | Operação normal                    |
 * | LOW      | 3     | Dados antigos (>5 min)             |
 * 
 * @see PayloadManager::calculateNodePriority() para cálculo de QoS
 * @see MissionData para estrutura de dados do nó
 */

#ifndef GROUND_NODE_MANAGER_H
#define GROUND_NODE_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * @class GroundNodeManager
 * @brief Gerenciador de buffer de ground nodes com QoS
 */
class GroundNodeManager {
public:
    /**
     * @brief Construtor padrão
     */
    GroundNodeManager();

    //=========================================================================
    // GERENCIAMENTO DE NÓS
    //=========================================================================
    
    /**
     * @brief Atualiza ou adiciona um ground node no buffer
     * @param data Dados do nó recebidos via LoRa
     * @note Calcula prioridade QoS automaticamente
     * @note Se buffer cheio, substitui nó de menor prioridade
     */
    void updateNode(const MissionData& data);
    
    /**
     * @brief Remove nós expirados do buffer
     * @param now Timestamp atual (millis())
     * @param maxAgeMs Idade máxima em milissegundos (TTL)
     */
    void cleanup(unsigned long now, unsigned long maxAgeMs);
    
    /**
     * @brief Reseta flags de forwarding de todos os nós
     * @return Número de nós que tinham flag ativo
     */
    uint8_t resetForwardFlags();

    //=========================================================================
    // ACESSO AO BUFFER
    //=========================================================================
    
    /** @brief Acesso somente leitura ao buffer */
    const GroundNodeBuffer& buffer() const { return _buffer; }
    
    /** @brief Acesso read-write ao buffer */
    GroundNodeBuffer& buffer() { return _buffer; }

private:
    GroundNodeBuffer _buffer;    ///< Buffer de ground nodes
    
    /**
     * @brief Substitui nó de menor prioridade pelo novo
     * @param newData Dados do novo nó a inserir
     * @note Chamado quando buffer está cheio
     */
    void _replaceLowestPriorityNode(const MissionData& newData);
};

#endif // GROUND_NODE_MANAGER_H