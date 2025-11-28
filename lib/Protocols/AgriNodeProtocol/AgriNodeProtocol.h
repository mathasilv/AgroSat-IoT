// lib/Protocols/AgriNodeProtocol/AgriNodeProtocol.h - AgroSat Protocols
// Serialização/deserialização pacotes AgriNode

#pragma once

#include "../../../include/types.h"

namespace Protocols {
    class AgriNodeProtocol {
    public:
        static String createSatellitePayload(const TelemetryData& data);
        static String createRelayPayload(const TelemetryData& data, const GroundNodeBuffer& buffer);
        static bool decodeNodePacket(const String& hexPayload, MissionData& data);
        static bool validateChecksum(const String& packet);
        static uint8_t calculatePriority(const MissionData& node);
    };
}