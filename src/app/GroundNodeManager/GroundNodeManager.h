#ifndef GROUND_NODE_MANAGER_H
#define GROUND_NODE_MANAGER_H

#include <Arduino.h>
#include "config.h"

// CommunicationManager removido daqui - não é mais necessário para prioridade

class GroundNodeManager {
public:
    GroundNodeManager();

    // FIX: Removemos o argumento 'comm'
    void updateNode(const MissionData& data);
    void cleanup(unsigned long now, unsigned long maxAgeMs);
    uint8_t resetForwardFlags();

    const GroundNodeBuffer& buffer() const { return _buffer; }
    GroundNodeBuffer& buffer() { return _buffer; }

private:
    GroundNodeBuffer _buffer;
    
    // FIX: Removemos o argumento 'comm'
    void _replaceLowestPriorityNode(const MissionData& newData);
};

#endif