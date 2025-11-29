/**
 * @file SX127x_hal_adapter.h
 * @brief Hardware Abstraction Layer Adapter for SX127x Driver
 * @details Thread-safe SPI communication wrapper using AgroSat-IoT HAL
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-29
 * @version 1.0.0
 * 
 * This adapter provides:
 * - Single/burst register read/write operations
 * - Thread-safe SPI transactions (via HAL mutex)
 * - GPIO control for RESET and CS pins
 * - Timing utilities for mode transitions
 * 
 * @note All methods are inline for zero-overhead abstraction
 */

#ifndef SX127X_HAL_ADAPTER_H
#define SX127X_HAL_ADAPTER_H

#include <SPI.h>
#include <Arduino.h>
#include "SX127x_registers.h"
#include <HAL/interface/SPI.h>  // ← ADICIONAR ESTA LINHA (namespace HAL::SPI)

namespace SX127x {

/**
 * @class HalAdapter
 * @brief Adapter between SX127x driver and HAL::SPI interface
 * 
 * This class handles all low-level SPI communication and GPIO control
 * for the SX127x radio. It ensures thread-safety through the HAL::SPI
 * mutex and provides timing-accurate operations.
 * 
 * @warning Do not use this class directly - it's used internally by SX127x class
 */
class HalAdapter {
private:
    HAL::SPI* spi_;          ///< Pointer to HAL SPI instance
    uint8_t csPin_;          ///< Chip Select pin
    uint8_t resetPin_;       ///< Reset pin
    uint8_t dio0Pin_;        ///< DIO0 interrupt pin (optional)
    bool initialized_;       ///< Initialization state
    
    // SPI transaction flags (MSB determines read/write)
    static constexpr uint8_t SPI_WRITE_FLAG = 0x80;  ///< OR with address for write
    static constexpr uint8_t SPI_READ_FLAG  = 0x00;  ///< AND with 0x7F for read
    
public:
    /**
     * @brief Constructor - does not initialize hardware
     * @param spi Pointer to initialized HAL::SPI instance
     */
    explicit HalAdapter(HAL::SPI* spi)
        : spi_(spi)
        , csPin_(0)
        , resetPin_(0)
        , dio0Pin_(0)
        , initialized_(false)
    {
    }
    
    /**
     * @brief Configure GPIO pins (must be called before init)
     * 
     * @param csPin Chip Select pin (active LOW)
     * @param resetPin Reset pin (active LOW)
     * @param dio0Pin DIO0 interrupt pin (optional, 0 = not used)
     * 
     * @note CS pin should already be configured via HAL::SPI::configureCS()
     */
    inline void setPins(uint8_t csPin, uint8_t resetPin, uint8_t dio0Pin = 0) {
        csPin_ = csPin;
        resetPin_ = resetPin;
        dio0Pin_ = dio0Pin;
        
        // Configure RESET pin
        pinMode(resetPin_, OUTPUT);
        digitalWrite(resetPin_, HIGH);  // Idle state (not in reset)
        
        // Configure DIO0 if used
        if (dio0Pin_ != 0) {
            pinMode(dio0Pin_, INPUT);
        }
        
        log_d("[SX127x HAL] Pins configured: CS=%d, RST=%d, DIO0=%d", 
              csPin_, resetPin_, dio0Pin_);
    }
    
    /**
     * @brief Hardware reset of SX127x chip
     * 
     * Performs reset sequence according to datasheet:
     * 1. Pull RESET low for 10ms
     * 2. Release (high) and wait 10ms for PLL lock
     * 
     * @note This is blocking (20ms total)
     */
    inline void reset() {
        log_d("[SX127x HAL] Performing hardware reset...");
        
        digitalWrite(resetPin_, LOW);
        delay(Timing::RESET_PULSE_MS);
        
        digitalWrite(resetPin_, HIGH);
        delay(Timing::RESET_WAIT_MS);
        
        initialized_ = true;
        log_d("[SX127x HAL] Reset complete");
    }
    
    // ========================================================================
    // SINGLE REGISTER OPERATIONS
    // ========================================================================
    
    /**
     * @brief Write single byte to register
     * 
     * @param address Register address (0x00-0xFF)
     * @param value Value to write
     * 
     * SPI Transaction format:
     * - MOSI: [address | 0x80] [value]
     * - MISO: [ignored] [ignored]
     * 
     * @note Thread-safe via HAL::SPI mutex
     */
    inline void writeRegister(uint8_t address, uint8_t value) {
        if (!initialized_) {
            log_e("[SX127x HAL] Write failed - not initialized");
            return;
        }
        
        spi_->beginTransaction(csPin_);
        
        // Send address with write flag (MSB=1)
        spi_->transfer(address | SPI_WRITE_FLAG);
        
        // Send value
        spi_->transfer(value);
        
        spi_->endTransaction(csPin_);
        
        #ifdef SX127X_DEBUG_SPI
        log_v("[SX127x HAL] WRITE: 0x%02X <- 0x%02X", address, value);
        #endif
    }
    
    /**
     * @brief Read single byte from register
     * 
     * @param address Register address (0x00-0xFF)
     * @return Register value
     * 
     * SPI Transaction format:
     * - MOSI: [address & 0x7F] [0x00]
     * - MISO: [ignored] [value]
     * 
     * @note Thread-safe via HAL::SPI mutex
     */
    inline uint8_t readRegister(uint8_t address) {
        if (!initialized_) {
            log_e("[SX127x HAL] Read failed - not initialized");
            return 0xFF;
        }
        
        spi_->beginTransaction(csPin_);
        
        // Send address with read flag (MSB=0)
        spi_->transfer(address & ~SPI_WRITE_FLAG);
        
        // Read value
        uint8_t value = spi_->transfer(0x00);
        
        spi_->endTransaction(csPin_);
        
        #ifdef SX127X_DEBUG_SPI
        log_v("[SX127x HAL] READ: 0x%02X -> 0x%02X", address, value);
        #endif
        
        return value;
    }
    
    /**
     * @brief Modify register bits using read-modify-write
     * 
     * @param address Register address
     * @param mask Bit mask (1 = modify, 0 = preserve)
     * @param value New value (only bits set in mask are applied)
     * 
     * Example: Set bit 3 without changing others
     * @code
     * modifyRegister(0x01, 0x08, 0x08);  // Set bit 3
     * modifyRegister(0x01, 0x08, 0x00);  // Clear bit 3
     * @endcode
     * 
     * @note This is NOT atomic - use carefully in multi-threaded code
     */
    inline void modifyRegister(uint8_t address, uint8_t mask, uint8_t value) {
        uint8_t current = readRegister(address);
        uint8_t modified = (current & ~mask) | (value & mask);
        writeRegister(address, modified);
        
        #ifdef SX127X_DEBUG_SPI
        log_v("[SX127x HAL] MODIFY: 0x%02X: 0x%02X -> 0x%02X (mask=0x%02X)",
              address, current, modified, mask);
        #endif
    }
    
    // ========================================================================
    // BURST OPERATIONS (for FIFO access)
    // ========================================================================
    
    /**
     * @brief Write multiple bytes to FIFO
     * 
     * @param data Pointer to data buffer
     * @param length Number of bytes to write (1-255)
     * 
     * @warning Buffer must remain valid during SPI transaction
     * @note Thread-safe via HAL::SPI mutex
     */
    inline void writeFifo(const uint8_t* data, size_t length) {
        if (!initialized_ || data == nullptr || length == 0) {
            log_e("[SX127x HAL] FIFO write failed - invalid parameters");
            return;
        }
        
        if (length > Hardware::MAX_PAYLOAD_LENGTH) {
            log_e("[SX127x HAL] FIFO write failed - length %d > max %d",
                  length, Hardware::MAX_PAYLOAD_LENGTH);
            return;
        }
        
        spi_->beginTransaction(csPin_);
        
        // Send FIFO address with write flag
        spi_->transfer(Registers::REG_FIFO | SPI_WRITE_FLAG);
        
        // Write data bytes
        for (size_t i = 0; i < length; i++) {
            spi_->transfer(data[i]);
        }
        
        spi_->endTransaction(csPin_);
        
        #ifdef SX127X_DEBUG_SPI
        log_v("[SX127x HAL] FIFO WRITE: %d bytes", length);
        #endif
    }
    
    /**
     * @brief Read multiple bytes from FIFO
     * 
     * @param buffer Pointer to destination buffer
     * @param length Number of bytes to read (1-255)
     * 
     * @warning Buffer must be at least 'length' bytes
     * @note Thread-safe via HAL::SPI mutex
     */
    inline void readFifo(uint8_t* buffer, size_t length) {
        if (!initialized_ || buffer == nullptr || length == 0) {
            log_e("[SX127x HAL] FIFO read failed - invalid parameters");
            return;
        }
        
        spi_->beginTransaction(csPin_);
        
        // Send FIFO address with read flag
        spi_->transfer(Registers::REG_FIFO & ~SPI_WRITE_FLAG);
        
        // Read data bytes
        for (size_t i = 0; i < length; i++) {
            buffer[i] = spi_->transfer(0x00);
        }
        
        spi_->endTransaction(csPin_);
        
        #ifdef SX127X_DEBUG_SPI
        log_v("[SX127x HAL] FIFO READ: %d bytes", length);
        #endif
    }
    
    // ========================================================================
    // GPIO UTILITIES
    // ========================================================================
    
    /**
     * @brief Read DIO0 pin state (for polling mode)
     * 
     * @return true if DIO0 is HIGH, false if LOW or not configured
     * 
     * @note Used for TX_DONE and RX_DONE detection without interrupts
     */
    inline bool readDio0() const {
        if (dio0Pin_ == 0) return false;
        return digitalRead(dio0Pin_) == HIGH;
    }
    
    /**
     * @brief Wait for DIO0 to go HIGH (blocking with timeout)
     * 
     * @param timeoutMs Timeout in milliseconds
     * @return true if DIO0 went HIGH, false if timeout
     * 
     * @warning This is blocking - use only when no interrupt attached
     */
    inline bool waitForDio0(uint32_t timeoutMs) {
        if (dio0Pin_ == 0) {
            log_w("[SX127x HAL] DIO0 not configured, cannot wait");
            return false;
        }
        
        uint32_t startTime = millis();
        
        while (digitalRead(dio0Pin_) == LOW) {
            if (millis() - startTime > timeoutMs) {
                log_w("[SX127x HAL] DIO0 timeout (%lu ms)", timeoutMs);
                return false;
            }
            
            // Yield to other tasks (FreeRTOS)
            delay(1);
        }
        
        log_d("[SX127x HAL] DIO0 triggered after %lu ms", millis() - startTime);
        return true;
    }
    
    // ========================================================================
    // TIMING UTILITIES
    // ========================================================================
    
    /**
     * @brief Delay for mode transition stabilization
     * 
     * According to datasheet, switching between modes (SLEEP↔STDBY↔TX/RX)
     * requires ~100μs PLL lock time. We use conservative 5ms.
     */
    inline void delayModeSwitch() const {
        delay(Timing::MODE_SWITCH_MS);
    }
    
    /**
     * @brief Check if adapter is initialized
     * @return true if reset() was called
     */
    inline bool isInitialized() const {
        return initialized_;
    }
    
    // ========================================================================
    // REGISTER CACHE (OPTIONAL - for power optimization)
    // ========================================================================
    
    /**
     * @brief Register cache for frequently accessed values
     * 
     * Caching reduces SPI transactions, which saves power in sleep-heavy
     * applications like cubesats. Only use for registers that don't change
     * spontaneously (i.e., not IRQ flags or RSSI).
     * 
     * @note Currently disabled - enable only after testing
     */
    struct RegisterCache {
        uint8_t opMode;          ///< Cached REG_OP_MODE (0x01)
        uint8_t modemConfig1;    ///< Cached REG_MODEM_CONFIG_1 (0x1D)
        uint8_t modemConfig2;    ///< Cached REG_MODEM_CONFIG_2 (0x1E)
        uint8_t paConfig;        ///< Cached REG_PA_CONFIG (0x09)
        bool valid;              ///< Cache validity flag
        
        RegisterCache() : opMode(0), modemConfig1(0), modemConfig2(0), 
                          paConfig(0), valid(false) {}
    };
    
    // Cache instance (currently unused - implement in phase 2 if needed)
    // RegisterCache cache_;
    
    // ========================================================================
    // DEBUG HELPERS
    // ========================================================================
    
    /**
     * @brief Dump all LoRa registers to serial (debug only)
     * 
     * Prints all registers from 0x00 to 0x4D in hex format.
     * Useful for debugging configuration issues.
     * 
     * @note Only compile when SX127X_DEBUG_REGISTERS is defined
     */
    #ifdef SX127X_DEBUG_REGISTERS
    inline void dumpRegisters() {
        log_i("[SX127x HAL] ========== REGISTER DUMP ==========");
        
        // Common registers (0x00-0x0C)
        log_i("  FIFO         (0x00): 0x%02X", readRegister(0x00));
        log_i("  OpMode       (0x01): 0x%02X", readRegister(0x01));
        log_i("  FrfMsb       (0x06): 0x%02X", readRegister(0x06));
        log_i("  FrfMid       (0x07): 0x%02X", readRegister(0x07));
        log_i("  FrfLsb       (0x08): 0x%02X", readRegister(0x08));
        log_i("  PaConfig     (0x09): 0x%02X", readRegister(0x09));
        log_i("  PaRamp       (0x0A): 0x%02X", readRegister(0x0A));
        log_i("  Ocp          (0x0B): 0x%02X", readRegister(0x0B));
        log_i("  Lna          (0x0C): 0x%02X", readRegister(0x0C));
        
        // LoRa registers (0x0D-0x26)
        log_i("  FifoAddrPtr  (0x0D): 0x%02X", readRegister(0x0D));
        log_i("  FifoTxBase   (0x0E): 0x%02X", readRegister(0x0E));
        log_i("  FifoRxBase   (0x0F): 0x%02X", readRegister(0x0F));
        log_i("  FifoRxCurr   (0x10): 0x%02X", readRegister(0x10));
        log_i("  IrqFlagsMask (0x11): 0x%02X", readRegister(0x11));
        log_i("  IrqFlags     (0x12): 0x%02X", readRegister(0x12));
        log_i("  RxNbBytes    (0x13): 0x%02X", readRegister(0x13));
        log_i("  ModemStat    (0x18): 0x%02X", readRegister(0x18));
        log_i("  PktSnrValue  (0x19): 0x%02X", readRegister(0x19));
        log_i("  PktRssiValue (0x1A): 0x%02X", readRegister(0x1A));
        log_i("  RssiValue    (0x1B): 0x%02X", readRegister(0x1B));
        log_i("  ModemCfg1    (0x1D): 0x%02X", readRegister(0x1D));
        log_i("  ModemCfg2    (0x1E): 0x%02X", readRegister(0x1E));
        log_i("  SymbTimeout  (0x1F): 0x%02X", readRegister(0x1F));
        log_i("  PreambleMsb  (0x20): 0x%02X", readRegister(0x20));
        log_i("  PreambleLsb  (0x21): 0x%02X", readRegister(0x21));
        log_i("  PayloadLen   (0x22): 0x%02X", readRegister(0x22));
        log_i("  ModemCfg3    (0x26): 0x%02X", readRegister(0x26));
        
        // Detection and sync
        log_i("  DetectOpt    (0x31): 0x%02X", readRegister(0x31));
        log_i("  InvertIQ     (0x33): 0x%02X", readRegister(0x33));
        log_i("  DetectThresh (0x37): 0x%02X", readRegister(0x37));
        log_i("  SyncWord     (0x39): 0x%02X", readRegister(0x39));
        
        // DIO and version
        log_i("  DioMapping1  (0x40): 0x%02X", readRegister(0x40));
        log_i("  DioMapping2  (0x41): 0x%02X", readRegister(0x41));
        log_i("  Version      (0x42): 0x%02X", readRegister(0x42));
        log_i("  PaDac        (0x4D): 0x%02X", readRegister(0x4D));
        
        log_i("[SX127x HAL] ===================================");
    }
    #endif // SX127X_DEBUG_REGISTERS
};

} // namespace SX127x

#endif // SX127X_HAL_ADAPTER_H
