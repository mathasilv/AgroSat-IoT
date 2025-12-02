/**
 * @file LoRaService.cpp
 * @brief Implementação do serviço LoRa (orquestrador)
 */

#include "LoRaService.h"

LoRaService::LoRaService() :
    _transmitter(),
    _receiver(),
    _initialized(false),
    _enabled(true),
    _currentSpreadingFactor(LORA_SPREADING_FACTOR)
{}

bool LoRaService::begin() {
    if (!init()) {
        return false;
    }
    _configureParameters();
    LoRa.receive();
    return true;
}

bool LoRaService::init() {
    DEBUG_PRINTLN("[LoRaService] Inicializando LoRa (LilyGO TTGO LoRa32)");

    // Reset
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);

    DEBUG_PRINTLN("[LoRaService] Módulo LoRa resetado");

    // SPI
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[LoRaService] SPI inicializado");

    // Pinos
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTLN("[LoRaService] Pinos configurados CS/RST/DIO0");

    // LoRa.begin
    DEBUG_PRINTF("[LoRaService] Tentando LoRa.begin(%.1f MHz)...\n",
                 LORA_FREQUENCY / 1E6);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRaService] FALHOU! Chip LoRa não respondeu");
        _initialized = false;
        return false;
    }

    DEBUG_PRINTLN("[LoRaService] ✓ OK!");
    _initialized = true;
    return true;
}

bool LoRaService::retryInit(uint8_t maxAttempts) {
    DEBUG_PRINTF("[LoRaService] Retry init (max %d tentativas)\n", maxAttempts);

    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
        DEBUG_PRINTF("[LoRaService] Tentativa %d/%d\n", attempt, maxAttempts);

        if (init()) {
            _configureParameters();
            LoRa.receive();
            DEBUG_PRINTLN("[LoRaService] ✓ LoRa reinicializado");
            return true;
        }

        delay(1000);
    }

    DEBUG_PRINTLN("[LoRaService] ✗ Falhou após todas tentativas");
    _initialized = false;
    return false;
}

void LoRaService::_configureParameters() {
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    _currentSpreadingFactor = LORA_SPREADING_FACTOR;

    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setCodingRate4(LORA_CODING_RATE);

    if (LORA_CRC_ENABLED) {
        LoRa.enableCrc();
    } else {
        LoRa.disableCrc();
    }

    LoRa.disableInvertIQ();

    DEBUG_PRINTF("[LoRaService] TX Power: %d dBm\n", LORA_TX_POWER);
    DEBUG_PRINTF("[LoRaService] BW: %.0f kHz\n", LORA_SIGNAL_BANDWIDTH / 1000.0f);
    DEBUG_PRINTF("[LoRaService] SF: %d\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("[LoRaService] Preamble: %d\n", LORA_PREAMBLE_LENGTH);
    DEBUG_PRINTF("[LoRaService] SyncWord: 0x%02X\n", LORA_SYNC_WORD);
    DEBUG_PRINTF("[LoRaService] CRC: %s\n", LORA_CRC_ENABLED ? "ON" : "OFF");
}

void LoRaService::enable(bool enable) {
    _enabled = enable;
    DEBUG_PRINTF("[LoRaService] LoRa %s\n", enable ? "HABILITADO" : "DESABILITADO");
}

bool LoRaService::send(const String& data) {
    if (!_enabled) {
        DEBUG_PRINTLN("[LoRaService] LoRa desabilitado");
        return false;
    }
    if (!_initialized) {
        DEBUG_PRINTLN("[LoRaService] LoRa não inicializado");
        return false;
    }

    return _transmitter.send(data, _currentSpreadingFactor);
}

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    if (!_initialized) {
        return false;
    }

    return _receiver.receive(packet, rssi, snr);
}

bool LoRaService::isOnline() const {
    return _initialized;
}

int LoRaService::getLastRSSI() const {
    return _receiver.getLastRSSI();
}

float LoRaService::getLastSNR() const {
    return _receiver.getLastSNR();
}

void LoRaService::getStatistics(uint16_t& sent, uint16_t& failed) const {
    _transmitter.getStatistics(sent, failed);
}

void LoRaService::reconfigure(OperationMode mode) {
    if (!_initialized) {
        DEBUG_PRINTLN("[LoRaService] LoRa não inicializado, ignorando reconfiguração");
        return;
    }

    DEBUG_PRINTF("[LoRaService] Reconfigurando para modo %d\n", mode);
    
    switch (mode) {
        case MODE_PREFLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[LoRaService] PRE-FLIGHT: SF7, 17dBm");
            break;

        case MODE_FLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[LoRaService] FLIGHT: SF7, 17dBm (HAB)");
            break;

        case MODE_SAFE:
            LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR_SAFE);
            _currentSpreadingFactor = LORA_SPREADING_FACTOR_SAFE;
            LoRa.setTxPower(20);
            DEBUG_PRINTLN("[LoRaService] SAFE: SF12, 20dBm");
            break;

        default:
            DEBUG_PRINTLN("[LoRaService] Modo desconhecido");
            break;
    }

    delay(10);
    LoRa.receive();
}

void LoRaService::adaptSpreadingFactor(float altitude) {
    _transmitter.adaptSpreadingFactor(altitude, _currentSpreadingFactor);
}
