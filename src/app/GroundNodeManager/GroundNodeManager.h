#ifndef GROUND_NODE_MANAGER_H
#define GROUND_NODE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "comm/CommunicationManager/CommunicationManager.h"

class GroundNodeManager {
public:
    GroundNodeManager();

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