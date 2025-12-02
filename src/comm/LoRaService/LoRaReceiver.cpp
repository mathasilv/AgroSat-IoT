#include "LoRaReceiver.h"

LoRaReceiver::LoRaReceiver() : _lastRSSI(0), _lastSNR(0.0) {}

bool LoRaReceiver::receive(String& packet, int& rssi, float& snr) {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return false;

    packet = "";
    while (LoRa.available()) {
        packet += (char)LoRa.read();
    }

    _lastRSSI = LoRa.packetRssi();
    _lastSNR = LoRa.packetSnr();
    
    rssi = _lastRSSI;
    snr = _lastSNR;

    return true;
}