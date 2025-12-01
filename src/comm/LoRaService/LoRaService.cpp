#include "LoRaService.h"

LoRaService::LoRaService() :
    _initialized(false),
    _enabled(true),
    _lastRSSI(0),
    _lastSNR(0.0f),
    _packetsSent(0),
    _packetsFailed(0),
    _lastTx(0),
    _currentSpreadingFactor(LORA_SPREADING_FACTOR),
    _txFailureCount(0),
    _lastTxFailure(0)
{}

bool LoRaService::begin() {
    if (!init()) {
        return false;
    }
    _configureParameters();
    LoRa.receive();  // entra em RX
    return true;
}

bool LoRaService::init() {
    DEBUG_PRINTLN("[LoRaService] Inicializando LoRa (LilyGO TTGO LoRa32)");

    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);

    DEBUG_PRINTLN("[LoRaService] Modulo LoRa resetado");

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[LoRaService] SPI inicializado");

    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTLN("[LoRaService] Pinos configurados CS/RST/DIO0");

    DEBUG_PRINTF("[LoRaService] Tentando LoRa.begin(%.1f MHz)...\n",
                 LORA_FREQUENCY / 1E6);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRaService] FALHOU! Chip LoRa nao respondeu");
        _initialized = false;
        return false;
    }

    DEBUG_PRINTLN("[LoRaService] OK!");
    _initialized = true;
    return true;
}

bool LoRaService::retryInit(uint8_t maxAttempts) {
    DEBUG_PRINTF("[LoRaService] Tentando reinicializar LoRa (max %d tentativas)...\n",
                 maxAttempts);

    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
        DEBUG_PRINTF("[LoRaService] Tentativa %d/%d...\n", attempt, maxAttempts);

        if (init()) {
            _configureParameters();
            LoRa.receive();
            DEBUG_PRINTLN("[LoRaService] LoRa reinicializado com sucesso");
            return true;
        }

        delay(1000);
    }

    DEBUG_PRINTLN("[LoRaService] LoRa falhou apos todas as tentativas");
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

bool LoRaService::_validatePayloadSize(size_t size) const {
    return size > 0 && size <= LORA_MAX_PAYLOAD_SIZE;
}

bool LoRaService::_isChannelFree() const {
    const int RSSI_THRESHOLD = -90;   // mesmo critério do código atual
    const uint8_t CAD_CHECKS = 3;

    for (uint8_t i = 0; i < CAD_CHECKS; i++) {
        int rssi = LoRa.rssi();
        if (rssi > RSSI_THRESHOLD) {
            DEBUG_PRINTF("[LoRaService] Canal ocupado (RSSI=%d dBm)\n", rssi);
            delay(random(50, 200)); // backoff aleatório
            return false;
        }
        delay(10);
    }
    return true;
}

bool LoRaService::send(const String& data) {
    if (!_enabled) {
        DEBUG_PRINTLN("[LoRaService] LoRa desabilitado via flag");
        return false;
    }
    if (!_initialized) {
        DEBUG_PRINTLN("[LoRaService] LoRa nao inicializado");
        _packetsFailed++;
        return false;
    }
    if (!_validatePayloadSize(data.length())) {
        DEBUG_PRINTF("[LoRaService] Payload muito grande: %d bytes (max %d)\n",
                     data.length(), LORA_MAX_PAYLOAD_SIZE);
        _packetsFailed++;
        return false;
    }

    unsigned long now = millis();
    if (now - _lastTx < LORA_MIN_INTERVAL_MS) {
        uint32_t waitTime = LORA_MIN_INTERVAL_MS - (now - _lastTx);
        DEBUG_PRINTF("[LoRaService] Aguardando duty cycle: %lu ms\n", waitTime);
        if (waitTime > 30000) {
            // proteção contra espera absurda
            return false;
        }
        delay(waitTime);
        now = millis();
    }

    if (!_isChannelFree()) {
        DEBUG_PRINTLN("[LoRaService] TX adiado: canal ocupado");
        return false;
    }

    DEBUG_PRINTLN("[LoRaService] TRANSMITINDO LORA");
    DEBUG_PRINTF("[LoRaService] Payload: %s\n", data.c_str());
    DEBUG_PRINTF("[LoRaService] Tamanho: %d bytes\n", data.length());

    uint32_t txTimeout = (_currentSpreadingFactor >= 11)
        ? LORA_TX_TIMEOUT_MS_SAFE
        : LORA_TX_TIMEOUT_MS_NORMAL;

    unsigned long txStart = millis();
    LoRa.beginPacket();
    LoRa.print(data);
    bool success = (LoRa.endPacket(true) == 1);
    unsigned long txDuration = millis() - txStart;

    if (txDuration > txTimeout) {
        DEBUG_PRINTF("[LoRaService] Timeout TX LoRa: %lu ms > %lu ms\n",
                     txDuration, txTimeout);
        _packetsFailed++;
        LoRa.receive();
        return false;
    }

    if (success) {
        _packetsSent++;
        _lastTx = now;
        _txFailureCount = 0;
        DEBUG_PRINTF("[LoRaService] Pacote enviado (%lu ms), total=%d\n",
                     txDuration, _packetsSent);
    } else {
        _packetsFailed++;
        _txFailureCount++;
        _lastTxFailure = now;
        DEBUG_PRINTLN("[LoRaService] Falha na transmissao LoRa");

        uint32_t backoff = min<uint32_t>(1000U * (1U << _txFailureCount), 8000U);
        DEBUG_PRINTF("[LoRaService] Backoff %lu ms\n", backoff);
        LoRa.receive();
        delay(backoff);
        return false;
    }

    delay(10);
    LoRa.receive();
    DEBUG_PRINTLN("[LoRaService] LoRa voltou ao modo RX");
    return success;
}

bool LoRaService::receive(String& packet, int& rssi, float& snr) {
    if (!_initialized) {
        return false;
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        return false;
    }

    packet = "";
    while (LoRa.available()) {
        packet += char(LoRa.read());
    }

    rssi = LoRa.packetRssi();
    snr  = LoRa.packetSnr();

    const int   MIN_RSSI = -120;
    const float MIN_SNR  = -15.0f;

    if (rssi < MIN_RSSI) {
        DEBUG_PRINTF("[LoRaService] Pacote descartado (RSSI muito baixo: %d dBm)\n", rssi);
        return false;
    }
    if (snr < MIN_SNR) {
        DEBUG_PRINTF("[LoRaService] Pacote descartado (SNR muito baixo: %.1f dB)\n", snr);
        return false;
    }

    _lastRSSI = rssi;
    _lastSNR  = snr;

    DEBUG_PRINTLN("[LoRaService] PACOTE LORA RECEBIDO");
    DEBUG_PRINTF("[LoRaService] Dados: %s\n", packet.c_str());
    DEBUG_PRINTF("[LoRaService] RSSI: %d dBm\n", rssi);
    DEBUG_PRINTF("[LoRaService] SNR: %.1f dB\n", snr);
    DEBUG_PRINTF("[LoRaService] Tamanho: %d bytes\n", packet.length());

    return true;
}

bool LoRaService::isOnline() const {
    return _initialized;
}

int LoRaService::getLastRSSI() const {
    return _lastRSSI;
}

float LoRaService::getLastSNR() const {
    return _lastSNR;
}

void LoRaService::getStatistics(uint16_t& sent, uint16_t& failed) const {
    sent   = _packetsSent;
    failed = _packetsFailed;
}

void LoRaService::reconfigure(OperationMode mode) {
    if (!_initialized) {
        DEBUG_PRINTLN("[LoRaService] LoRa nao inicializado, ignorando reconfiguracao");
        return;
    }

    DEBUG_PRINTF("[LoRaService] Reconfigurando LoRa para modo %d...\n", mode);
    switch (mode) {
        case MODE_PREFLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[LoRaService] PRE-FLIGHT SF7, 17 dBm");
            break;

        case MODE_FLIGHT:
            LoRa.setSpreadingFactor(7);
            _currentSpreadingFactor = 7;
            LoRa.setTxPower(17);
            DEBUG_PRINTLN("[LoRaService] FLIGHT SF7, 17 dBm (HAB)");
            break;

        case MODE_SAFE:
            LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR_SAFE);
            _currentSpreadingFactor = LORA_SPREADING_FACTOR_SAFE;
            LoRa.setTxPower(20);
            DEBUG_PRINTLN("[LoRaService] SAFE SF12, 20 dBm");
            break;

        default:
            DEBUG_PRINTLN("[LoRaService] Modo desconhecido");
            break;
    }

    delay(10);
    LoRa.receive();
}

void LoRaService::adaptSpreadingFactor(float altitude) {
    if (isnan(altitude)) {
        return;
    }

    uint8_t newSF = _currentSpreadingFactor;

    if (altitude < 10000.0f)       newSF = 7;
    else if (altitude < 20000.0f)  newSF = 8;
    else if (altitude < 30000.0f)  newSF = 9;
    else                           newSF = 10;

    if (newSF != _currentSpreadingFactor) {
        DEBUG_PRINTF("[LoRaService] Ajustando SF %d -> %d (alt=%.0fm)\n",
                     _currentSpreadingFactor, newSF, altitude);
        LoRa.setSpreadingFactor(newSF);
        _currentSpreadingFactor = newSF;
        delay(10);
        LoRa.receive();
    }
}
