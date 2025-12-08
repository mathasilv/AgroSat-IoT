#ifndef GROUND_NODE_MANAGER_H
#define GROUND_NODE_MANAGER_H

#include <Arduino.h>
#include "config.h"

// Forward Declaration para quebrar ciclo de dependência
class CommunicationManager;

class GroundNodeManager {
public:
    GroundNodeManager();

    // Apenas declaração da referência, não precisa do include aqui
    void updateNode(const MissionData& data, CommunicationManager& comm);
    void cleanup(unsigned long now, unsigned long maxAgeMs);
    uint8_t resetForwardFlags();

    const GroundNodeBuffer& buffer() const { return _buffer; }
    GroundNodeBuffer& buffer() { return _buffer; }

private:
    GroundNodeBuffer _buffer;
    void _replaceLowestPriorityNode(const MissionData& newData, CommunicationManager& comm);
};

#endif