/**
 * @file SX127x.cpp
 * @brief Implementation of SX127x LoRa Driver
 */

#include "SX127x.h"

namespace SX127x {

// ============================================================================
// LIFECYCLE
// ============================================================================

Radio::Radio(HAL::SPI* spi)
    : hal_(spi)
    , currentMode_(RadioMode::SLEEP)
    , lastError_(RadioError::OK)
    , initialized_(false)
    , packetInProgress_(false)
    , packetIndex_(0)
    , implicitHeaderMode_(false)
    , payloadLength_(0xFF)
    , rxPacketLength_(0)
    , rxPacketIndex_(0)
    , rxPacketRssi_(0)
    , rxPacketSnr_(0)
    , frequency_(0)
    , txPower_(17)
    , bandwidth_(125000)
    , spreadingFactor_(7)
    , codingRate_(5)
{
    log_d("[SX127x] Radio object created");
}

Radio::~Radio() {
    if (initialized_) {
        end();
    }
}

void Radio::setPins(uint8_t cs, uint8_t reset, uint8_t dio0) {
    hal_.setPins(cs, reset, dio0);
}

bool Radio::begin(uint32_t frequency) {
    log_i("[SX127x] Initializing radio...");
    
    // Hardware reset
    hal_.reset();
    
    // Verify chip presence
    if (!verifyChip()) {
        setError(RadioError::CHIP_NOT_FOUND);
        log_e("[SX127x] Chip verification failed!");
        return false;
    }
    
    log_i("[SX127x] Chip detected: SX127x version 0x%02X", getChipVersion());
    
    // Configure LoRa mode
    if (configureLoRaMode() != RadioError::OK) {
        log_e("[SX127x] Failed to configure LoRa mode");
        return false;
    }
    
    // Enter standby mode
    if (setMode(RadioMode::STANDBY) != RadioError::OK) {
        log_e("[SX127x] Failed to enter standby mode");
        return false;
    }
    
    // Set frequency
    if (setFrequency(frequency) != RadioError::OK) {
        log_e("[SX127x] Failed to set frequency");
        return false;
    }
    
    // Configure default parameters (compatible with sandeepmistry/LoRa defaults)
    setTxPower(17);                     // 17 dBm (PA_BOOST)
    setSignalBandwidth(125000);         // 125 kHz
    setSpreadingFactor(7);              // SF7
    setCodingRate4(5);                  // CR 4/5
    setPreambleLength(8);               // 8 symbols
    setSyncWord(0x12);                  // Private network
    enableCrc();                        // CRC enabled
    disableInvertIQ();                  // Normal IQ
    
    // Configure FIFO base addresses
    hal_.writeRegister(Registers::REG_FIFO_TX_BASE_ADDR, Hardware::FIFO_TX_BASE_ADDR_DEFAULT);
    hal_.writeRegister(Registers::REG_FIFO_RX_BASE_ADDR, Hardware::FIFO_RX_BASE_ADDR_DEFAULT);
    
    // Enable auto AGC
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_3, 
                        ModemConfig3::AGC_AUTO_ON, 
                        ModemConfig3::AGC_AUTO_ON);
    
    // Enable LNA boost for better sensitivity
    hal_.modifyRegister(Registers::REG_LNA,
                        Lna::LNA_BOOST_HF_MASK,
                        Lna::LNA_BOOST_HF_ON);
    
    initialized_ = true;
    
    log_i("[SX127x] Initialization complete");
    log_i("[SX127x]   Frequency: %.3f MHz", frequency / 1E6);
    log_i("[SX127x]   TX Power: %d dBm", txPower_);
    log_i("[SX127x]   Bandwidth: %.1f kHz", bandwidth_ / 1E3);
    log_i("[SX127x]   Spreading Factor: SF%d", spreadingFactor_);
    log_i("[SX127x]   Coding Rate: 4/%d", codingRate_);
    
    return true;
}

void Radio::end() {
    if (!initialized_) return;
    
    log_i("[SX127x] Shutting down radio...");
    
    // Enter sleep mode (lowest power)
    sleep();
    
    initialized_ = false;
    log_i("[SX127x] Radio shut down");
}

// ============================================================================
// CONFIGURATION - RF PARAMETERS
// ============================================================================

RadioError Radio::setFrequency(uint32_t frequency) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Validate frequency range (137-1020 MHz for SX1276/77/78/79)
    if (frequency < 137000000 || frequency > 1020000000) {
        log_e("[SX127x] Invalid frequency: %lu Hz", frequency);
        return setError(RadioError::INVALID_FREQUENCY);
    }
    
    // Calculate FRF register value
    uint32_t frf = Frequency::calculateFrf(frequency);
    
    // Write to 3 registers (MSB, MID, LSB)
    hal_.writeRegister(Registers::REG_FRF_MSB, (frf >> 16) & 0xFF);
    hal_.writeRegister(Registers::REG_FRF_MID, (frf >> 8) & 0xFF);
    hal_.writeRegister(Registers::REG_FRF_LSB, frf & 0xFF);
    
    frequency_ = frequency;
    
    log_d("[SX127x] Frequency set to %.3f MHz (FRF=0x%06X)", 
          frequency / 1E6, frf);
    
    return RadioError::OK;
}

RadioError Radio::setTxPower(int8_t power) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Validate power range (2-20 dBm)
    if (power < 2 || power > 20) {
        log_e("[SX127x] Invalid TX power: %d dBm (valid: 2-20)", power);
        return setError(RadioError::INVALID_TX_POWER);
    }
    
    // Always use PA_BOOST pin (required for TTGO LoRa32)
    // PA_BOOST supports 2-20 dBm
    
    uint8_t paConfig = PaConfig::PA_SELECT_PABOOST;
    
    if (power <= 17) {
        // Standard mode: 2-17 dBm
        // Pout = 17 - (15 - OutputPower)
        // OutputPower = Pout - 2
        uint8_t outputPower = power - 2;
        paConfig |= (outputPower & PaConfig::OUTPUT_POWER_MASK);
        
        // Disable high power mode
        hal_.writeRegister(Registers::REG_PA_DAC, PaDac::PA_DAC_NORMAL);
        
    } else {
        // High power mode: 18-20 dBm
        // Pout = 17 - (15 - OutputPower) + 3
        // OutputPower = Pout - 5
        uint8_t outputPower = power - 5;
        paConfig |= (outputPower & PaConfig::OUTPUT_POWER_MASK);
        
        // Enable +20 dBm mode
        hal_.writeRegister(Registers::REG_PA_DAC, PaDac::PA_DAC_HIGH_POWER);
    }
    
    // Set MaxPower to 7 (max value)
    paConfig |= (0x07 << 4);
    
    hal_.writeRegister(Registers::REG_PA_CONFIG, paConfig);
    
    // Configure OCP (Over Current Protection)
    // For PA_BOOST, max current is 120 mA (87 + 5 * 6 = 117 mA)
    hal_.writeRegister(Registers::REG_OCP, 0x20 | 0x0B);  // Enable OCP, Imax = 120 mA
    
    txPower_ = power;
    
    log_d("[SX127x] TX Power set to %d dBm (PA_CONFIG=0x%02X)", 
          power, paConfig);
    
    return RadioError::OK;
}

RadioError Radio::setSignalBandwidth(uint32_t bw) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    uint8_t bwValue;
    
    // Map bandwidth to register value
    switch (bw) {
        case 7800:    bwValue = ModemConfig1::BW_7_8_KHZ;   break;
        case 10400:   bwValue = ModemConfig1::BW_10_4_KHZ;  break;
        case 15600:   bwValue = ModemConfig1::BW_15_6_KHZ;  break;
        case 20800:   bwValue = ModemConfig1::BW_20_8_KHZ;  break;
        case 31250:   bwValue = ModemConfig1::BW_31_25_KHZ; break;
        case 41700:   bwValue = ModemConfig1::BW_41_7_KHZ;  break;
        case 62500:   bwValue = ModemConfig1::BW_62_5_KHZ;  break;
        case 125000:  bwValue = ModemConfig1::BW_125_KHZ;   break;
        case 250000:  bwValue = ModemConfig1::BW_250_KHZ;   break;
        case 500000:  bwValue = ModemConfig1::BW_500_KHZ;   break;
        default:
            log_e("[SX127x] Invalid bandwidth: %lu Hz", bw);
            return setError(RadioError::INVALID_BANDWIDTH);
    }
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_1,
                        ModemConfig1::BW_MASK,
                        bwValue);
    
    bandwidth_ = bw;
    
    // Update LDR optimization
    if (shouldEnableLowDataRateOptimize()) {
        hal_.modifyRegister(Registers::REG_MODEM_CONFIG_3,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE);
        log_d("[SX127x] Low Data Rate Optimize enabled");
    } else {
        hal_.modifyRegister(Registers::REG_MODEM_CONFIG_3,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE,
                            0x00);
    }
    
    log_d("[SX127x] Bandwidth set to %.1f kHz", bw / 1E3);
    
    return RadioError::OK;
}

RadioError Radio::setSpreadingFactor(uint8_t sf) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Validate SF range (6-12)
    if (sf < 6 || sf > 12) {
        log_e("[SX127x] Invalid spreading factor: %d (valid: 6-12)", sf);
        return setError(RadioError::INVALID_SPREADING_FACTOR);
    }
    
    // Map SF to register value
    uint8_t sfValue = (sf << 4);
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_2,
                        ModemConfig2::SF_MASK,
                        sfValue);
    
    // SF6 requires special settings
    if (sf == 6) {
        // Implicit header mode required for SF6
        implicitHeaderMode();
        
        // Detection optimize for SF6
        hal_.writeRegister(Registers::REG_DETECTION_OPTIMIZE, 0xC5);
        hal_.writeRegister(Registers::REG_DETECTION_THRESHOLD, 0x0C);
        
        log_w("[SX127x] SF6 mode - implicit header required");
    } else {
        // Normal detection settings for SF7-SF12
        hal_.writeRegister(Registers::REG_DETECTION_OPTIMIZE, 0xC3);
        hal_.writeRegister(Registers::REG_DETECTION_THRESHOLD, 0x0A);
    }
    
    spreadingFactor_ = sf;
    
    // Update LDR optimization
    if (shouldEnableLowDataRateOptimize()) {
        hal_.modifyRegister(Registers::REG_MODEM_CONFIG_3,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE);
        log_d("[SX127x] Low Data Rate Optimize enabled");
    } else {
        hal_.modifyRegister(Registers::REG_MODEM_CONFIG_3,
                            ModemConfig3::LOW_DATA_RATE_OPTIMIZE,
                            0x00);
    }
    
    log_d("[SX127x] Spreading Factor set to SF%d", sf);
    
    return RadioError::OK;
}

RadioError Radio::setCodingRate4(uint8_t denominator) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Validate CR denominator (5-8 for 4/5, 4/6, 4/7, 4/8)
    if (denominator < 5 || denominator > 8) {
        log_e("[SX127x] Invalid coding rate: 4/%d (valid: 4/5 to 4/8)", denominator);
        return setError(RadioError::INVALID_CODING_RATE);
    }
    
    // Map CR to register value (CR = denominator - 4)
    uint8_t crValue = ((denominator - 4) << 1);
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_1,
                        ModemConfig1::CR_MASK,
                        crValue);
    
    codingRate_ = denominator;
    
    log_d("[SX127x] Coding Rate set to 4/%d", denominator);
    
    return RadioError::OK;
}

RadioError Radio::setPreambleLength(uint16_t length) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Minimum preamble length is 6 symbols
    if (length < 6) {
        log_e("[SX127x] Preamble too short: %d (min 6)", length);
        return setError(RadioError::INVALID_PREAMBLE_LENGTH);
    }
    
    // Write MSB and LSB
    hal_.writeRegister(Registers::REG_PREAMBLE_MSB, (length >> 8) & 0xFF);
    hal_.writeRegister(Registers::REG_PREAMBLE_LSB, length & 0xFF);
    
    log_d("[SX127x] Preamble length set to %d symbols", length);
    
    return RadioError::OK;
}

RadioError Radio::setSyncWord(uint8_t syncWord) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Reserved sync words (avoid)
    if (syncWord == 0x00 || syncWord == 0xFF) {
        log_w("[SX127x] Sync word 0x%02X is reserved", syncWord);
    }
    
    hal_.writeRegister(Registers::REG_SYNC_WORD, syncWord);
    
    log_d("[SX127x] Sync word set to 0x%02X", syncWord);
    
    return RadioError::OK;
}

// ============================================================================
// CONFIGURATION - PACKET SETTINGS
// ============================================================================

void Radio::enableCrc() {
    if (!initialized_) return;
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_2,
                        ModemConfig2::RX_PAYLOAD_CRC_ON,
                        ModemConfig2::RX_PAYLOAD_CRC_ON);
    
    log_d("[SX127x] CRC enabled");
}

void Radio::disableCrc() {
    if (!initialized_) return;
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_2,
                        ModemConfig2::RX_PAYLOAD_CRC_ON,
                        0x00);
    
    log_d("[SX127x] CRC disabled");
}

void Radio::enableInvertIQ() {
    if (!initialized_) return;
    
    hal_.writeRegister(Registers::REG_INVERT_IQ, 0x66);
    hal_.writeRegister(Registers::REG_INVERT_IQ + 1, 0x19);  // InvertIQ2
    
    log_d("[SX127x] IQ inversion enabled");
}

void Radio::disableInvertIQ() {
    if (!initialized_) return;
    
    hal_.writeRegister(Registers::REG_INVERT_IQ, 0x27);
    hal_.writeRegister(Registers::REG_INVERT_IQ + 1, 0x1D);  // InvertIQ2
    
    log_d("[SX127x] IQ inversion disabled");
}

void Radio::implicitHeaderMode(uint8_t payloadLength) {
    if (!initialized_) return;
    
    implicitHeaderMode_ = true;
    payloadLength_ = payloadLength;
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_1,
                        ModemConfig1::IMPLICIT_HEADER_MODE,
                        ModemConfig1::IMPLICIT_HEADER_MODE);
    
    hal_.writeRegister(Registers::REG_PAYLOAD_LENGTH, payloadLength);
    
    log_d("[SX127x] Implicit header mode enabled (length=%d)", payloadLength);
}

void Radio::explicitHeaderMode() {
    if (!initialized_) return;
    
    implicitHeaderMode_ = false;
    
    hal_.modifyRegister(Registers::REG_MODEM_CONFIG_1,
                        ModemConfig1::IMPLICIT_HEADER_MODE,
                        0x00);
    
    log_d("[SX127x] Explicit header mode enabled");
}

// ============================================================================
// TRANSMISSION
// ============================================================================

RadioError Radio::beginPacket(bool implicitHeader) {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    if (packetInProgress_) {
        log_w("[SX127x] Packet already in progress");
        return setError(RadioError::ALREADY_IN_TX);
    }
    
    // Enter standby mode
    if (setMode(RadioMode::STANDBY) != RadioError::OK) {
        return lastError_;
    }
    
    // Set implicit/explicit header mode for this packet
    if (implicitHeader) {
        implicitHeaderMode();
    } else if (implicitHeaderMode_) {
        // Only switch back if currently in implicit mode
        explicitHeaderMode();
    }
    
    // Reset FIFO address pointer to TX base
    hal_.writeRegister(Registers::REG_FIFO_ADDR_PTR, Hardware::FIFO_TX_BASE_ADDR_DEFAULT);
    
    // Clear packet state
    packetIndex_ = 0;
    packetInProgress_ = true;
    
    log_d("[SX127x] Packet transmission started (%s header)",
          implicitHeader ? "implicit" : "explicit");
    
    return RadioError::OK;
}

size_t Radio::write(const uint8_t* data, size_t length) {
    if (!packetInProgress_) {
        log_e("[SX127x] No packet in progress - call beginPacket() first");
        return 0;
    }
    
    // Check if payload would exceed maximum
    if (packetIndex_ + length > Hardware::MAX_PAYLOAD_LENGTH) {
        size_t available = Hardware::MAX_PAYLOAD_LENGTH - packetIndex_;
        log_w("[SX127x] Payload truncated: %d -> %d bytes", length, available);
        length = available;
    }
    
    if (length == 0) return 0;
    
    // Write to FIFO
    hal_.writeFifo(data, length);
    packetIndex_ += length;
    
    log_v("[SX127x] Wrote %d bytes to FIFO (total: %d)", length, packetIndex_);
    
    return length;
}

size_t Radio::write(uint8_t byte) {
    return write(&byte, 1);
}

size_t Radio::print(const String& str) {
    return write((const uint8_t*)str.c_str(), str.length());
}

size_t Radio::print(const char* str) {
    return write((const uint8_t*)str, strlen(str));
}

bool Radio::endPacket(bool async) {
    if (!packetInProgress_) {
        log_e("[SX127x] No packet in progress");
        setError(RadioError::ALREADY_IN_TX);
        return false;
    }
    
    // Validate payload size
    if (packetIndex_ == 0) {
        log_e("[SX127x] Cannot send empty packet");
        packetInProgress_ = false;
        setError(RadioError::PAYLOAD_TOO_LARGE);
        return false;
    }
    
    // Write payload length (explicit header mode only)
    if (!implicitHeaderMode_) {
        hal_.writeRegister(Registers::REG_PAYLOAD_LENGTH, packetIndex_);
    }
    
    log_d("[SX127x] Transmitting %d bytes (async=%d)...", packetIndex_, async);
    
    // Configure DIO0 for TX_DONE
    configureDio0(true);
    
    // Clear TX_DONE flag
    hal_.writeRegister(Registers::REG_IRQ_FLAGS, IrqFlags::TX_DONE);
    
    // Enter TX mode
    if (setMode(RadioMode::TX) != RadioError::OK) {
        packetInProgress_ = false;
        return false;
    }
    
    packetInProgress_ = false;
    
    // Non-blocking mode: return immediately
    if (async) {
        log_d("[SX127x] TX started (async mode)");
        return true;
    }
    
    // Blocking mode: wait for TX_DONE
    uint32_t timeout = (spreadingFactor_ >= 11) ? 
                       Timing::TX_TIMEOUT_MS_LONG : 
                       Timing::TX_TIMEOUT_MS_NORMAL;
    
    if (!waitTxDone(timeout)) {
        log_e("[SX127x] TX timeout!");
        setError(RadioError::TX_TIMEOUT);
        
        // Force back to standby
        setMode(RadioMode::STANDBY);
        return false;
    }
    
    log_d("[SX127x] TX complete");
    
    // Return to standby (saves power vs continuous TX)
    setMode(RadioMode::STANDBY);
    
    return true;
}

// ============================================================================
// RECEPTION
// ============================================================================

RadioError Radio::receive() {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    // Configure DIO0 for RX_DONE
    configureDio0(false);
    
    // Clear IRQ flags
    clearIrqFlags();
    
    // Reset FIFO address pointer to RX base
    hal_.writeRegister(Registers::REG_FIFO_ADDR_PTR, Hardware::FIFO_RX_BASE_ADDR_DEFAULT);
    
    // Enter RX continuous mode
    RadioError err = setMode(RadioMode::RX_CONTINUOUS);
    
    if (err == RadioError::OK) {
        log_d("[SX127x] Entered RX continuous mode");
    }
    
    return err;
}

int Radio::parsePacket() {
    if (!initialized_) return 0;
    
    // Check IRQ flags
    uint8_t irqFlags = hal_.readRegister(Registers::REG_IRQ_FLAGS);
    
    // Clear all IRQ flags
    hal_.writeRegister(Registers::REG_IRQ_FLAGS, IrqFlags::CLEAR_ALL);
    
    // Check for CRC error
    if (irqFlags & IrqFlags::PAYLOAD_CRC_ERROR) {
        log_w("[SX127x] RX packet with CRC error");
        setError(RadioError::CRC_ERROR);
        return 0;
    }
    
    // Check for RX_DONE
    if (!(irqFlags & IrqFlags::RX_DONE)) {
        return 0;  // No packet received
    }
    
    // Read packet length
    rxPacketLength_ = hal_.readRegister(Registers::REG_RX_NB_BYTES);
    
    // Read FIFO current address
    uint8_t fifoRxCurrentAddr = hal_.readRegister(Registers::REG_FIFO_RX_CURRENT_ADDR);
    
    // Set FIFO address pointer to packet start
    hal_.writeRegister(Registers::REG_FIFO_ADDR_PTR, fifoRxCurrentAddr);
    
    // Reset read index
    rxPacketIndex_ = 0;
    
    // Read RSSI and SNR
    rxPacketRssi_ = hal_.readRegister(Registers::REG_PKT_RSSI_VALUE);
    rxPacketSnr_ = (int8_t)hal_.readRegister(Registers::REG_PKT_SNR_VALUE);
    
    log_d("[SX127x] RX packet: %d bytes, RSSI=%d dBm, SNR=%.1f dB",
          rxPacketLength_, packetRssi(), packetSnr());
    
    return rxPacketLength_;
}

int Radio::available() {
    return rxPacketLength_ - rxPacketIndex_;
}

int Radio::read() {
    if (available() <= 0) return -1;
    
    uint8_t byte = hal_.readRegister(Registers::REG_FIFO);
    rxPacketIndex_++;
    
    return byte;
}

size_t Radio::readBytes(uint8_t* buffer, size_t length) {
    if (buffer == nullptr) return 0;
    
    size_t bytesAvailable = available();
    if (length > bytesAvailable) {
        length = bytesAvailable;
    }
    
    if (length == 0) return 0;
    
    // Read from FIFO
    hal_.readFifo(buffer, length);
    rxPacketIndex_ += length;
    
    return length;
}

int Radio::peek() {
    if (available() <= 0) return -1;
    
    // Save current FIFO pointer
    uint8_t currentAddr = hal_.readRegister(Registers::REG_FIFO_ADDR_PTR);
    
    // Read byte
    uint8_t byte = hal_.readRegister(Registers::REG_FIFO);
    
    // Restore FIFO pointer (peek doesn't advance)
    hal_.writeRegister(Registers::REG_FIFO_ADDR_PTR, currentAddr);
    
    return byte;
}

// ============================================================================
// PACKET STATISTICS
// ============================================================================

int Radio::packetRssi() {
    // Calculate actual RSSI in dBm
    // For HF port (PA_BOOST): RSSI = -157 + RssiValue
    int rssi = RssiSnr::RSSI_OFFSET_HF + rxPacketRssi_;
    
    // Adjust for SNR if negative (weak signal)
    if (rxPacketSnr_ < 0) {
        rssi += (rxPacketSnr_ / 4);  // SNR is in 0.25 dB steps
    }
    
    return rssi;
}

float Radio::packetSnr() {
    // SNR is stored as 2's complement in 0.25 dB steps
    return rxPacketSnr_ * RssiSnr::SNR_SCALE;
}

int Radio::rssi() {
    if (!initialized_) return 0;
    
    // Read current RSSI (channel, not packet)
    uint8_t rssiValue = hal_.readRegister(Registers::REG_RSSI_VALUE);
    
    // Calculate RSSI in dBm
    return RssiSnr::RSSI_OFFSET_HF + rssiValue;
}

int32_t Radio::packetFrequencyError() {
    if (!initialized_) return 0;
    
    // Read frequency error from registers 0x28-0x2A
    // Note: These are undocumented registers, values are approximate
    int32_t freqError = 0;
    freqError = (int32_t)hal_.readRegister(0x28);
    freqError = (freqError << 8) | hal_.readRegister(0x29);
    freqError = (freqError << 8) | hal_.readRegister(0x2A);
    
    // Sign extend (20-bit value)
    if (freqError & 0x80000) {
        freqError |= 0xFFF00000;
    }
    
    // Convert to Hz (approximation)
    // FreqError (Hz) = (FreqErrorReg * 2^24) / (Fxtal * 2^20) * BW
    // Simplified for BW=125kHz: FreqError ≈ FreqErrorReg * 0.5
    return (int32_t)(freqError * 0.5);
}

// ============================================================================
// MODE CONTROL
// ============================================================================

RadioError Radio::sleep() {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    return setMode(RadioMode::SLEEP);
}

RadioError Radio::standby() {
    if (!initialized_) {
        return setError(RadioError::NOT_INITIALIZED);
    }
    
    return setMode(RadioMode::STANDBY);
}

RadioMode Radio::getMode() const {
    return currentMode_;
}

bool Radio::isTransmitting() {
    if (!initialized_) return false;
    
    // Check if in TX mode
    if (currentMode_ != RadioMode::TX) {
        return false;
    }
    
    // Check TX_DONE flag
    uint8_t irqFlags = hal_.readRegister(Registers::REG_IRQ_FLAGS);
    return !(irqFlags & IrqFlags::TX_DONE);
}

// ============================================================================
// ADVANCED - CHANNEL ACTIVITY DETECTION
// ============================================================================

bool Radio::channelActivityDetection() {
    if (!initialized_) return false;
    
    // Configure DIO0 for CAD_DONE
    hal_.modifyRegister(Registers::REG_DIO_MAPPING_1,
                        DioMapping::DIO0_MASK,
                        DioMapping::DIO0_CAD_DONE);
    
    // Clear CAD flags
    hal_.writeRegister(Registers::REG_IRQ_FLAGS, 
                       IrqFlags::CAD_DONE | IrqFlags::CAD_DETECTED);
    
    // Enter CAD mode
    setMode(RadioMode::CAD);
    
    // Wait for CAD_DONE (typically 1-2 symbol periods)
    // For SF7 @ BW125: ~1 ms
    // For SF12 @ BW125: ~16 ms
    uint32_t timeout = 20;  // Conservative 20ms
    
    uint32_t startTime = millis();
    while (true) {
        uint8_t irqFlags = hal_.readRegister(Registers::REG_IRQ_FLAGS);
        
        if (irqFlags & IrqFlags::CAD_DONE) {
            // Clear flags
            hal_.writeRegister(Registers::REG_IRQ_FLAGS, 
                              IrqFlags::CAD_DONE | IrqFlags::CAD_DETECTED);
            
            // Return to standby
            setMode(RadioMode::STANDBY);
            
            // Check if activity detected
            bool detected = (irqFlags & IrqFlags::CAD_DETECTED);
            log_d("[SX127x] CAD: %s", detected ? "ACTIVITY" : "CLEAR");
            
            return detected;
        }
        
        if (millis() - startTime > timeout) {
            log_w("[SX127x] CAD timeout");
            setMode(RadioMode::STANDBY);
            return false;
        }
        
        delay(1);
    }
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

uint8_t Radio::getChipVersion() {
    return hal_.readRegister(Registers::REG_VERSION);
}

RadioError Radio::getLastError() const {
    return lastError_;
}

#ifdef SX127X_DEBUG_REGISTERS
void Radio::dumpRegisters() {
    hal_.dumpRegisters();
}
#endif

// ============================================================================
// PRIVATE METHODS
// ============================================================================

RadioError Radio::setMode(RadioMode mode) {
    uint8_t opMode = OpMode::LONG_RANGE_MODE;  // LoRa mode bit always set
    
    switch (mode) {
        case RadioMode::SLEEP:
            opMode |= OpMode::MODE_SLEEP;
            break;
        case RadioMode::STANDBY:
            opMode |= OpMode::MODE_STDBY;
            break;
        case RadioMode::TX:
            opMode |= OpMode::MODE_TX;
            break;
        case RadioMode::RX_CONTINUOUS:
            opMode |= OpMode::MODE_RXCONTINUOUS;
            break;
        case RadioMode::RX_SINGLE:
            opMode |= OpMode::MODE_RXSINGLE;
            break;
        case RadioMode::CAD:
            opMode |= OpMode::MODE_CAD;
            break;
        default:
            log_e("[SX127x] Invalid mode: %d", (int)mode);
            return setError(RadioError::INVALID_MODE);
    }
    
    // Write to OpMode register
    hal_.writeRegister(Registers::REG_OP_MODE, opMode);
    
    // Wait for mode transition (datasheet: ~100μs, we use 5ms to be safe)
    hal_.delayModeSwitch();
    
    currentMode_ = mode;
    
    log_v("[SX127x] Mode changed to %d", (int)mode);
    
    return RadioError::OK;
}

bool Radio::verifyChip() {
    uint8_t version = getChipVersion();
    
    // SX1276/77/78/79 all return 0x12
    if (version == Hardware::CHIP_VERSION_SX1276) {
        return true;
    }
    
    log_e("[SX127x] Unknown chip version: 0x%02X (expected 0x12)", version);
    return false;
}

RadioError Radio::configureLoRaMode() {
    // Read current OpMode
    uint8_t opMode = hal_.readRegister(Registers::REG_OP_MODE);
    
    // Set LoRa mode bit (bit 7 = 1)
    opMode |= OpMode::LONG_RANGE_MODE;
    
    // Enter sleep mode first (required for mode change)
    opMode = (opMode & ~OpMode::MODE_MASK) | OpMode::MODE_SLEEP;
    hal_.writeRegister(Registers::REG_OP_MODE, opMode);
    
    delay(10);  // Wait for mode transition
    
    // Verify LoRa mode is set
    opMode = hal_.readRegister(Registers::REG_OP_MODE);
    if (!(opMode & OpMode::LONG_RANGE_MODE)) {
        log_e("[SX127x] Failed to set LoRa mode");
        return RadioError::CHIP_NOT_FOUND;
    }
    
    log_d("[SX127x] LoRa mode configured");
    return RadioError::OK;
}

void Radio::configureDio0(bool forTx) {
    if (forTx) {
        // DIO0 = TxDone
        hal_.modifyRegister(Registers::REG_DIO_MAPPING_1,
                            DioMapping::DIO0_MASK,
                            DioMapping::DIO0_TX_DONE);
    } else {
        // DIO0 = RxDone
        hal_.modifyRegister(Registers::REG_DIO_MAPPING_1,
                            DioMapping::DIO0_MASK,
                            DioMapping::DIO0_RX_DONE);
    }
}

bool Radio::waitTxDone(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    
    while (true) {
        // Check IRQ flags
        uint8_t irqFlags = hal_.readRegister(Registers::REG_IRQ_FLAGS);
        
        if (irqFlags & IrqFlags::TX_DONE) {
            // Clear TX_DONE flag
            hal_.writeRegister(Registers::REG_IRQ_FLAGS, IrqFlags::TX_DONE);
            return true;
        }
        
        // Check timeout
        if (millis() - startTime > timeoutMs) {
            return false;
        }
        
        // Yield to other tasks
        delay(1);
    }
}

void Radio::clearIrqFlags() {
    hal_.writeRegister(Registers::REG_IRQ_FLAGS, IrqFlags::CLEAR_ALL);
}

bool Radio::shouldEnableLowDataRateOptimize() const {
    // Low Data Rate Optimization is mandatory when:
    // Symbol duration > 16 ms
    // Symbol duration = (2^SF) / BW
    
    // Calculate symbol duration in milliseconds
    float symbolDuration = (1 << spreadingFactor_) / (bandwidth_ / 1000.0);
    
    return (symbolDuration > 16.0);
}

RadioError Radio::setError(RadioError error) {
    lastError_ = error;
    
    // Log error (only in debug builds)
    #ifdef SX127X_DEBUG_ERRORS
    const char* errorStr = "UNKNOWN";
    switch (error) {
        case RadioError::OK:                        errorStr = "OK"; break;
        case RadioError::CHIP_NOT_FOUND:            errorStr = "CHIP_NOT_FOUND"; break;
        case RadioError::INVALID_FREQUENCY:         errorStr = "INVALID_FREQUENCY"; break;
        case RadioError::INVALID_BANDWIDTH:         errorStr = "INVALID_BANDWIDTH"; break;
        case RadioError::INVALID_SPREADING_FACTOR:  errorStr = "INVALID_SPREADING_FACTOR"; break;
        case RadioError::INVALID_CODING_RATE:       errorStr = "INVALID_CODING_RATE"; break;
        case RadioError::INVALID_TX_POWER:          errorStr = "INVALID_TX_POWER"; break;
        case RadioError::INVALID_PREAMBLE_LENGTH:   errorStr = "INVALID_PREAMBLE_LENGTH"; break;
        case RadioError::INVALID_SYNC_WORD:         errorStr = "INVALID_SYNC_WORD"; break;
        case RadioError::PAYLOAD_TOO_LARGE:         errorStr = "PAYLOAD_TOO_LARGE"; break;
        case RadioError::TX_TIMEOUT:                errorStr = "TX_TIMEOUT"; break;
        case RadioError::RX_TIMEOUT:                errorStr = "RX_TIMEOUT"; break;
        case RadioError::CRC_ERROR:                 errorStr = "CRC_ERROR"; break;
        case RadioError::NOT_INITIALIZED:           errorStr = "NOT_INITIALIZED"; break;
        case RadioError::ALREADY_IN_TX:             errorStr = "ALREADY_IN_TX"; break;
        case RadioError::ALREADY_IN_RX:             errorStr = "ALREADY_IN_RX"; break;
        case RadioError::INVALID_MODE:              errorStr = "INVALID_MODE"; break;
    }
    
    if (error != RadioError::OK) {
        log_e("[SX127x] Error: %s", errorStr);
    }
    #endif
    
    return error;
}

} // namespace SX127x
