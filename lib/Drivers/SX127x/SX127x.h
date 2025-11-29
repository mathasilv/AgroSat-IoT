/**
 * @file SX127x.h
 * @brief Complete SX127x LoRa Driver for AgroSat-IoT CubeSat
 * @details Drop-in replacement for sandeepmistry/arduino-LoRa library
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-29
 * @version 1.0.0
 * 
 * Features:
 * - Thread-safe SPI communication (FreeRTOS mutex)
 * - Compatible API with sandeepmistry/LoRa (migration-friendly)
 * - Low-power optimized for CubeSat applications
 * - No dynamic memory allocation (RAII-safe)
 * - Comprehensive error handling
 * - Support for all LoRa modes (SF6-SF12, BW 7.8kHz-500kHz)
 * 
 * Usage:
 * @code
 * SX127x::Radio radio(&halSPI);
 * 
 * radio.setPins(LORA_CS, LORA_RST, LORA_DIO0);
 * if (!radio.begin(915E6)) {
 *     Serial.println("LoRa init failed!");
 *     return;
 * }
 * 
 * radio.setSpreadingFactor(7);
 * radio.setSignalBandwidth(125E3);
 * radio.setTxPower(17);
 * 
 * radio.beginPacket();
 * radio.write((uint8_t*)"Hello", 5);
 * radio.endPacket();
 * @endcode
 */

#ifndef SX127X_H
#define SX127X_H

#include <Arduino.h>
#include <HAL/interface/SPI.h>
#include "SX127x_registers.h"
#include "SX127x_hal_adapter.h"

namespace SX127x {

/**
 * @enum RadioError
 * @brief Error codes for SX127x operations
 */
enum class RadioError : uint8_t {
    OK = 0,                      ///< Success
    CHIP_NOT_FOUND,              ///< SPI communication failed or wrong chip
    INVALID_FREQUENCY,           ///< Frequency out of range
    INVALID_BANDWIDTH,           ///< Bandwidth not supported
    INVALID_SPREADING_FACTOR,    ///< SF out of range (6-12)
    INVALID_CODING_RATE,         ///< CR out of range (5-8)
    INVALID_TX_POWER,            ///< TX power out of range
    INVALID_PREAMBLE_LENGTH,     ///< Preamble too short/long
    INVALID_SYNC_WORD,           ///< Sync word reserved
    PAYLOAD_TOO_LARGE,           ///< Payload > 255 bytes
    TX_TIMEOUT,                  ///< Transmission timeout
    RX_TIMEOUT,                  ///< Reception timeout
    CRC_ERROR,                   ///< Received packet CRC failed
    NOT_INITIALIZED,             ///< begin() not called
    ALREADY_IN_TX,               ///< beginPacket() called twice
    ALREADY_IN_RX,               ///< Already in RX mode
    INVALID_MODE                 ///< Invalid operating mode
};

/**
 * @enum RadioMode
 * @brief Current operating mode of the radio
 */
enum class RadioMode : uint8_t {
    SLEEP = 0,                   ///< Sleep mode (lowest power)
    STANDBY,                     ///< Standby mode
    TX,                          ///< Transmit mode
    RX_CONTINUOUS,               ///< Receive continuous mode
    RX_SINGLE,                   ///< Receive single packet mode
    CAD                          ///< Channel Activity Detection
};

/**
 * @class Radio
 * @brief Main SX127x LoRa radio driver class
 * 
 * This class provides a high-level API for LoRa communication,
 * compatible with the sandeepmistry/arduino-LoRa library for easy migration.
 */
class Radio {
public:
    // ========================================================================
    // LIFECYCLE
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param spi Pointer to initialized HAL::SPI instance
     * 
     * @note Does not initialize hardware - call begin() to start
     */
    explicit Radio(HAL::SPI* spi);
    
    /**
     * @brief Destructor - ensures radio is in sleep mode
     */
    ~Radio();
    
    // Disable copy/move (hardware resource)
    Radio(const Radio&) = delete;
    Radio& operator=(const Radio&) = delete;
    Radio(Radio&&) = delete;
    Radio& operator=(Radio&&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Configure GPIO pins (must be called before begin)
     * 
     * @param cs Chip Select pin
     * @param reset Reset pin
     * @param dio0 DIO0 interrupt pin (0 = polling mode)
     * 
     * @note CS pin should already be configured via HAL::SPI::configureCS()
     */
    void setPins(uint8_t cs, uint8_t reset, uint8_t dio0 = 0);
    
    /**
     * @brief Initialize radio and verify chip presence
     * 
     * @param frequency RF frequency in Hz (e.g., 915000000 for 915 MHz)
     * @return true if initialization successful, false if chip not found
     * 
     * Initialization sequence:
     * 1. Hardware reset
     * 2. Verify chip version (0x12 for SX1276/77/78/79)
     * 3. Switch to LoRa mode
     * 4. Set frequency
     * 5. Configure default parameters (SF7, BW125, CR4/5)
     * 6. Enter standby mode
     */
    bool begin(uint32_t frequency);
    
    /**
     * @brief Put radio in sleep mode and release resources
     * 
     * @note Call this before destroying the object for clean shutdown
     */
    void end();
    
    // ========================================================================
    // CONFIGURATION - RF PARAMETERS
    // ========================================================================
    
    /**
     * @brief Set RF carrier frequency
     * 
     * @param frequency Frequency in Hz (137 MHz - 1020 MHz for SX1276/77/78/79)
     * @return Error code
     * 
     * Common ISM bands:
     * - 433 MHz: 433000000
     * - 868 MHz: 868000000 (Europe)
     * - 915 MHz: 915000000 (Americas)
     */
    RadioError setFrequency(uint32_t frequency);
    
    /**
     * @brief Set transmit power
     * 
     * @param power Power in dBm (2-20 dBm)
     * @return Error code
     * 
     * Power levels:
     * - 2-17 dBm: Standard operation
     * - 18-20 dBm: High power mode (requires PA_BOOST + PA_DAC)
     * 
     * @note Powers > 17 dBm consume significantly more current
     */
    RadioError setTxPower(int8_t power);
    
    /**
     * @brief Set signal bandwidth
     * 
     * @param bw Bandwidth in Hz
     * @return Error code
     * 
     * Valid values: 7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000
     * 
     * Trade-offs:
     * - Lower BW = better sensitivity, longer range, lower data rate
     * - Higher BW = worse sensitivity, shorter range, higher data rate
     */
    RadioError setSignalBandwidth(uint32_t bw);
    
    /**
     * @brief Set spreading factor
     * 
     * @param sf Spreading factor (6-12)
     * @return Error code
     * 
     * Trade-offs:
     * - Higher SF = better sensitivity, longer range, slower
     * - Lower SF = worse sensitivity, shorter range, faster
     * 
     * @note SF6 requires implicit header mode
     */
    RadioError setSpreadingFactor(uint8_t sf);
    
    /**
     * @brief Set coding rate (error correction)
     * 
     * @param denominator Denominator of coding rate (5-8 for 4/5, 4/6, 4/7, 4/8)
     * @return Error code
     * 
     * Trade-offs:
     * - Higher CR = better error correction, longer airtime
     * - Lower CR = less overhead, shorter airtime
     */
    RadioError setCodingRate4(uint8_t denominator);
    
    /**
     * @brief Set preamble length
     * 
     * @param length Preamble length in symbols (6-65535)
     * @return Error code
     * 
     * @note Transmitter and receiver must use same preamble length
     */
    RadioError setPreambleLength(uint16_t length);
    
    /**
     * @brief Set LoRa sync word
     * 
     * @param syncWord Sync word (0x00-0xFF)
     * @return Error code
     * 
     * Common values:
     * - 0x12: Private networks (default)
     * - 0x34: Public LoRaWAN networks
     * 
     * @note Transmitter and receiver must use same sync word
     */
    RadioError setSyncWord(uint8_t syncWord);
    
    // ========================================================================
    // CONFIGURATION - PACKET SETTINGS
    // ========================================================================
    
    /**
     * @brief Enable/disable CRC on payload
     * 
     * @note CRC is recommended for reliable communication
     */
    void enableCrc();
    void disableCrc();
    
    /**
     * @brief Enable/disable IQ inversion
     * 
     * @note Used for downlink/uplink differentiation in LoRaWAN
     */
    void enableInvertIQ();
    void disableInvertIQ();
    
    /**
     * @brief Enable implicit header mode
     * 
     * In implicit header mode, payload length, coding rate, and CRC presence
     * are fixed and known by both transmitter and receiver.
     * 
     * @param payloadLength Fixed payload length (required in implicit mode)
     * 
     * @note Required for SF6
     */
    void implicitHeaderMode(uint8_t payloadLength = 0xFF);
    
    /**
     * @brief Enable explicit header mode (default)
     * 
     * In explicit header mode, packet metadata (length, CR, CRC) is transmitted
     * in the header, allowing variable-length packets.
     */
    void explicitHeaderMode();
    
    // ========================================================================
    // TRANSMISSION
    // ========================================================================
    
    /**
     * @brief Begin packet transmission
     * 
     * @param implicitHeader Use implicit header mode for this packet (optional)
     * @return Error code
     * 
     * @note Must call endPacket() to actually transmit
     */
    RadioError beginPacket(bool implicitHeader = false);
    
    /**
     * @brief Write data to packet buffer
     * 
     * @param data Pointer to data
     * @param length Data length in bytes
     * @return Number of bytes written
     * 
     * @note Maximum 255 bytes per packet
     */
    size_t write(const uint8_t* data, size_t length);
    
    /**
     * @brief Write single byte to packet buffer
     * 
     * @param byte Byte to write
     * @return 1 if successful, 0 if buffer full
     */
    size_t write(uint8_t byte);
    
    /**
     * @brief Print string to packet buffer (Arduino compatibility)
     * 
     * @param str String to write
     * @return Number of bytes written
     */
    size_t print(const String& str);
    size_t print(const char* str);
    
    /**
     * @brief End packet and transmit
     * 
     * @param async If true, returns immediately (non-blocking)
     *              If false, waits for TX_DONE (blocking)
     * @return true if transmission started/completed successfully
     * 
     * @note Blocking mode waits up to 15 seconds for SF11/SF12
     */
    bool endPacket(bool async = false);
    
    // ========================================================================
    // RECEPTION
    // ========================================================================
    
    /**
     * @brief Enter receive mode (continuous)
     * 
     * Radio will continuously listen for packets until mode is changed.
     * Use parsePacket() to check for received packets.
     * 
     * @return Error code
     */
    RadioError receive();
    
    /**
     * @brief Check for received packet
     * 
     * @return Number of bytes in received packet, 0 if no packet
     * 
     * @note Must be called before read() to detect new packets
     */
    int parsePacket();
    
    /**
     * @brief Get number of bytes available to read
     * 
     * @return Number of bytes available
     */
    int available();
    
    /**
     * @brief Read single byte from received packet
     * 
     * @return Byte value, or -1 if no data available
     */
    int read();
    
    /**
     * @brief Read multiple bytes from received packet
     * 
     * @param buffer Destination buffer
     * @param length Maximum bytes to read
     * @return Number of bytes actually read
     */
    size_t readBytes(uint8_t* buffer, size_t length);
    
    /**
     * @brief Peek at next byte without removing it
     * 
     * @return Next byte value, or -1 if no data
     */
    int peek();
    
    // ========================================================================
    // PACKET STATISTICS
    // ========================================================================
    
    /**
     * @brief Get RSSI of last received packet
     * 
     * @return RSSI in dBm (-164 to 0)
     */
    int packetRssi();
    
    /**
     * @brief Get SNR of last received packet
     * 
     * @return SNR in dB (-20 to +10 typically)
     */
    float packetSnr();
    
    /**
     * @brief Get current channel RSSI (without receiving packet)
     * 
     * @return RSSI in dBm
     * 
     * @note Useful for channel activity detection (CAD)
     */
    int rssi();
    
    /**
     * @brief Get frequency error of last packet
     * 
     * @return Frequency error in Hz
     */
    int32_t packetFrequencyError();
    
    // ========================================================================
    // MODE CONTROL
    // ========================================================================
    
    /**
     * @brief Put radio in sleep mode (lowest power consumption)
     * 
     * @return Error code
     * 
     * @note Wake up time: ~1 ms
     */
    RadioError sleep();
    
    /**
     * @brief Put radio in standby mode
     * 
     * @return Error code
     * 
     * @note Lower power than RX, faster wake than sleep
     */
    RadioError standby();
    
    /**
     * @brief Get current operating mode
     * 
     * @return Current mode
     */
    RadioMode getMode() const;
    
    /**
     * @brief Check if radio is transmitting
     * 
     * @return true if TX in progress
     */
    bool isTransmitting();
    
    // ========================================================================
    // ADVANCED - CHANNEL ACTIVITY DETECTION
    // ========================================================================
    
    /**
     * @brief Perform Channel Activity Detection
     * 
     * @return true if LoRa preamble detected, false if channel clear
     * 
     * @note CAD takes ~1-2 symbol periods (~1-16 ms depending on SF)
     */
    bool channelActivityDetection();
    
    // ========================================================================
    // DIAGNOSTICS
    // ========================================================================
    
    /**
     * @brief Get chip version
     * 
     * @return Version register value (0x12 for SX1276/77/78/79)
     */
    uint8_t getChipVersion();
    
    /**
     * @brief Get last error code
     * 
     * @return Last error that occurred
     */
    RadioError getLastError() const;
    
    /**
     * @brief Dump all registers to Serial (debug only)
     */
    #ifdef SX127X_DEBUG_REGISTERS
    void dumpRegisters();
    #endif
    
private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================
    
    HalAdapter hal_;                 ///< HAL adapter for SPI/GPIO
    RadioMode currentMode_;          ///< Current operating mode
    RadioError lastError_;           ///< Last error code
    bool initialized_;               ///< Initialization flag
    
    // Packet state
    bool packetInProgress_;          ///< TX packet being built
    uint8_t packetIndex_;            ///< Current write position in FIFO
    bool implicitHeaderMode_;        ///< Implicit header flag
    uint8_t payloadLength_;          ///< Fixed payload length (implicit mode)
    
    // RX state
    uint8_t rxPacketLength_;         ///< Length of last received packet
    uint8_t rxPacketIndex_;          ///< Current read position
    int16_t rxPacketRssi_;           ///< RSSI of last packet
    int8_t rxPacketSnr_;             ///< SNR of last packet
    
    // Configuration cache (for validation)
    uint32_t frequency_;             ///< Current frequency (Hz)
    int8_t txPower_;                 ///< Current TX power (dBm)
    uint32_t bandwidth_;             ///< Current bandwidth (Hz)
    uint8_t spreadingFactor_;        ///< Current SF (6-12)
    uint8_t codingRate_;             ///< Current CR denominator (5-8)
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Set operating mode
     * @param mode Target mode
     * @return Error code
     */
    RadioError setMode(RadioMode mode);
    
    /**
     * @brief Verify chip is SX127x via version register
     * @return true if valid chip detected
     */
    bool verifyChip();
    
    /**
     * @brief Configure LoRa mode (vs FSK)
     * @return Error code
     */
    RadioError configureLoRaMode();
    
    /**
     * @brief Configure DIO0 for TX_DONE or RX_DONE
     * @param forTx true for TX_DONE, false for RX_DONE
     */
    void configureDio0(bool forTx);
    
    /**
     * @brief Wait for TX_DONE with timeout
     * @param timeoutMs Timeout in milliseconds
     * @return true if TX completed, false if timeout
     */
    bool waitTxDone(uint32_t timeoutMs);
    
    /**
     * @brief Clear all IRQ flags
     */
    void clearIrqFlags();
    
    /**
     * @brief Calculate optimal low data rate optimization setting
     * @return true if LDR should be enabled
     */
    bool shouldEnableLowDataRateOptimize() const;
    
    /**
     * @brief Set error code and log
     * @param error Error code to set
     */
    RadioError setError(RadioError error);
};

} // namespace SX127x

#endif // SX127X_H
