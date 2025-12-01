#pragma once

#include <Arduino.h>
#include "config.h"      // para MAX_GROUND_NODES, MissionData, GroundNodeBuffer
#include "CommunicationManager.h"

class GroundNodeManager {
public:
    GroundNodeManager();

    // Chamado quando chega um MissionData via LoRa
    void updateNode(const MissionData& data, CommunicationManager& comm);

    // Remove nós inativos há mais de maxAge ms
    void cleanup(unsigned long now, unsigned long maxAgeMs);

    // Reseta flags 'forwarded' periodicamente
    uint8_t resetForwardFlags();

    // Acesso ao buffer para TX / storage
    const GroundNodeBuffer& buffer() const { return _buffer; }
    GroundNodeBuffer& buffer() { return _buffer; }

private:
    GroundNodeBuffer _buffer;

    void _replaceLowestPriorityNode(const MissionData& newData, CommunicationManager& comm);
};
