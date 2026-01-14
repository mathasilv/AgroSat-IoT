/**
 * @file GroundNodeManager.cpp
 * @brief Implementação do gerenciador de ground nodes
 */

#include "GroundNodeManager.h"
#include "comm/PayloadManager/PayloadManager.h"

GroundNodeManager::GroundNodeManager() {
    memset(&_buffer, 0, sizeof(GroundNodeBuffer));
    _buffer.activeNodes = 0;
    _buffer.totalPacketsCollected = 0;
}

void GroundNodeManager::updateNode(const MissionData& data) {
    int existingIndex = -1;

    for (uint8_t i = 0; i < _buffer.activeNodes; i++) {
        if (_buffer.nodes[i].nodeId == data.nodeId) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex >= 0) {
        MissionData& existingNode = _buffer.nodes[existingIndex];

        if (data.sequenceNumber > existingNode.sequenceNumber) {
            DEBUG_PRINTF("[GroundNodeManager] Node %u atualizado (seq %u -> %u)\n",
                         data.nodeId, existingNode.sequenceNumber, data.sequenceNumber);

            existingNode = data;
            existingNode.lastLoraRx = millis();
            existingNode.forwarded  = false;
            existingNode.retransmissionTime = 0; 
            
            existingNode.priority = PayloadManager::calculateNodePriority(data);

            _buffer.lastUpdate[existingIndex] = millis();
            _buffer.totalPacketsCollected++;

        } else if (data.sequenceNumber == existingNode.sequenceNumber) {
            DEBUG_PRINTF("[GroundNodeManager] Node %u duplicado (seq %u), ignorando\n",
                         data.nodeId, data.sequenceNumber);
        }
    } else {
        if (_buffer.activeNodes < MAX_GROUND_NODES) {
            uint8_t newIndex        = _buffer.activeNodes;
            _buffer.nodes[newIndex] = data;
            _buffer.nodes[newIndex].lastLoraRx = millis();
            _buffer.nodes[newIndex].forwarded  = false;
            _buffer.nodes[newIndex].retransmissionTime = 0;
            
            _buffer.nodes[newIndex].priority = PayloadManager::calculateNodePriority(data);

            _buffer.lastUpdate[newIndex] = millis();
            _buffer.activeNodes++;
            _buffer.totalPacketsCollected++;

            DEBUG_PRINTF("[GroundNodeManager] Node %u novo (slot %d) | Total: %d/%d\n",
                         data.nodeId, newIndex, _buffer.activeNodes, MAX_GROUND_NODES);
        } else {
            _replaceLowestPriorityNode(data);
        }
    }
}

void GroundNodeManager::_replaceLowestPriorityNode(const MissionData& newData) {
    uint8_t replaceIndex   = 0;
    uint8_t lowestPriority = 255;
    unsigned long oldestTime = ULONG_MAX;

    for (uint8_t i = 0; i < MAX_GROUND_NODES; i++) {
        if (_buffer.nodes[i].priority < lowestPriority ||
            (_buffer.nodes[i].priority == lowestPriority &&
             _buffer.lastUpdate[i] < oldestTime)) {

            lowestPriority = _buffer.nodes[i].priority;
            oldestTime     = _buffer.lastUpdate[i];
            replaceIndex   = i;
        }
    }

    DEBUG_PRINTF("[GroundNodeManager] Buffer cheio: trocando Node %u (pri=%d) por %u (pri=%d)\n",
                 _buffer.nodes[replaceIndex].nodeId,
                 _buffer.nodes[replaceIndex].priority,
                 newData.nodeId,
                 newData.priority);

    _buffer.nodes[replaceIndex] = newData;
    _buffer.nodes[replaceIndex].lastLoraRx = millis();
    _buffer.nodes[replaceIndex].forwarded  = false;
    _buffer.nodes[replaceIndex].retransmissionTime = 0;
    
    _buffer.nodes[replaceIndex].priority = PayloadManager::calculateNodePriority(newData);
    _buffer.lastUpdate[replaceIndex]     = millis();
}

void GroundNodeManager::cleanup(unsigned long now, unsigned long maxAgeMs) {
    uint8_t removedCount = 0;

    for (uint8_t i = 0; i < _buffer.activeNodes; i++) {
        unsigned long age = now - _buffer.lastUpdate[i];

        if (age > maxAgeMs) {
            DEBUG_PRINTF("[GroundNodeManager] Node %u removido (inativo)\n",
                         _buffer.nodes[i].nodeId);

            for (uint8_t j = i; j < _buffer.activeNodes - 1; j++) {
                _buffer.nodes[j]       = _buffer.nodes[j + 1];
                _buffer.lastUpdate[j]  = _buffer.lastUpdate[j + 1];
            }

            _buffer.activeNodes--;
            removedCount++;
            i--;
        }
    }

    if (removedCount > 0) {
        DEBUG_PRINTF("[GroundNodeManager] Limpeza: %d no(s) removido(s)\n", removedCount);
    }
}

uint8_t GroundNodeManager::resetForwardFlags() {
    uint8_t resetCount = 0;
    for (uint8_t i = 0; i < _buffer.activeNodes; i++) {
        if (_buffer.nodes[i].forwarded) {
            _buffer.nodes[i].forwarded = false;
            resetCount++;
        }
    }
    return resetCount;
}