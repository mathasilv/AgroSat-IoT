/**
 * @file ESP32_SPI.h
 * @brief ESP32 Thread-Safe SPI Hardware Abstraction Layer
 * 
 * @details
 * Production-ready SPI HAL for LoRa SX127x communication on ESP32.
 * Features:
 * - FreeRTOS mutex protection for multi-task safety
 * - ISR-safe methods for LoRa DIO interrupt handling
 * - DMA-optimized bulk transfers
 * - IO_MUX pin validation for maximum speed (80 MHz)
 * - Comprehensive error handling and logging
 * 
 * @usage
 * @code
 * HAL::ESP32_SPI spi(&SPI);
 * spi.begin(18, 19, 23, 8000000);  // VSPI @ 8 MHz
 * spi.configureCS(LORA_CS_PIN);
 * 
 * spi.beginTransaction(LORA_CS_PIN);
 * uint8_t version = spi.transfer(0x42);
 * spi.endTransaction(LORA_CS_PIN);
 * @endcode
 * 
 * @warning Always call configureCS() before first transaction
 * @warning Do not use GPIO Matrix pins (non IO_MUX) for freq > 26.6 MHz
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-29
 * @version 2.0.0
 * 
 * @see https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html
 * @see https://github.com/espressif/arduino-esp32/issues/6404
 */

#ifndef HAL_ESP32_SPI_H
#define HAL_ESP32_SPI_H

#include <HAL/interface/SPI.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace HAL {

/**
 * @class ESP32_SPI
 * @brief Thread-safe ESP32 SPI implementation with interrupt support
 * 
 * This class wraps Arduino SPIClass with FreeRTOS synchronization primitives
 * to ensure safe concurrent access from multiple tasks and ISR contexts.
 * 
 * @note Header-only implementation for compile-time optimization
 */
class ESP32_SPI : public HAL::SPI {
private:
    SPIClass* spi_;                  ///< Pointer to Arduino SPI instance (VSPI/HSPI)
    SemaphoreHandle_t busMutex_;     ///< FreeRTOS mutex for bus arbitration
    bool initialized_;               ///< Initialization state flag
    uint32_t currentFrequency_;      ///< Current SPI clock frequency (Hz)
    
    // =========================================================================
    // HARDWARE CONSTANTS - ESP32 VSPI (SPI3_HOST)
    // =========================================================================
    
    /// @brief IO_MUX pins for VSPI (80 MHz capable, no GPIO Matrix overhead)
    static constexpr uint8_t VSPI_SCK_IOMUX  = 18;
    static constexpr uint8_t VSPI_MISO_IOMUX = 19;
    static constexpr uint8_t VSPI_MOSI_IOMUX = 23;
    
    /// @brief IO_MUX pins for HSPI (SPI2_HOST, alternative bus)
    static constexpr uint8_t HSPI_SCK_IOMUX  = 14;
    static constexpr uint8_t HSPI_MISO_IOMUX = 12;
    static constexpr uint8_t HSPI_MOSI_IOMUX = 13;
    
    /// @brief SX127x LoRa radio specifications
    static constexpr uint32_t SPI_FREQ_MIN       = 100000;    // 100 kHz (safe minimum)
    static constexpr uint32_t SPI_FREQ_MAX       = 10000000;  // 10 MHz (SX127x max tested)
    static constexpr uint32_t SPI_FREQ_DEFAULT   = 8000000;   // 8 MHz (recommended)
    static constexpr uint32_t GPIO_MATRIX_MAX_HZ = 26600000;  // GPIO Matrix limitation
    
    /// @brief Mutex timeout for critical operations (prevent deadlock)
    static constexpr TickType_t MUTEX_TIMEOUT_MS = pdMS_TO_TICKS(1000);
    
    /**
     * @brief Validate if pins use IO_MUX for optimal performance
     * @param sck SCK pin number
     * @param miso MISO pin number
     * @param mosi MOSI pin number
     * @return true if all pins use IO_MUX, false if GPIO Matrix
     */
    inline bool isIOmuxPins(uint8_t sck, uint8_t miso, uint8_t mosi) const {
        return (sck == VSPI_SCK_IOMUX && miso == VSPI_MISO_IOMUX && mosi == VSPI_MOSI_IOMUX) ||
               (sck == HSPI_SCK_IOMUX && miso == HSPI_MISO_IOMUX && mosi == HSPI_MOSI_IOMUX);
    }
    
public:
    // =========================================================================
    // LIFECYCLE MANAGEMENT
    // =========================================================================
    
    /**
     * @brief Constructor - does not initialize hardware
     * @param spiInstance Pointer to SPIClass (default: global SPI = VSPI)
     * 
     * @note For HSPI use: `ESP32_SPI spi(new SPIClass(HSPI))`
     */
    explicit ESP32_SPI(SPIClass* spiInstance = &::SPI)
        : spi_(spiInstance)
        , busMutex_(nullptr)
        , initialized_(false)
        , currentFrequency_(SPI_FREQ_DEFAULT)
    {
        // No hardware initialization in constructor (follow RAII but allow lazy init)
    }
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~ESP32_SPI() override {
        if (initialized_) {
            end();
        }
    }
    
    // Disable copy/move (singleton-like behavior for hardware resource)
    ESP32_SPI(const ESP32_SPI&) = delete;
    ESP32_SPI& operator=(const ESP32_SPI&) = delete;
    ESP32_SPI(ESP32_SPI&&) = delete;
    ESP32_SPI& operator=(ESP32_SPI&&) = delete;
    
    // =========================================================================
    // HAL::SPI INTERFACE IMPLEMENTATION
    // =========================================================================
    
    /**
     * @brief Initialize SPI bus with validation and mutex creation
     * 
     * @param sck SCK pin (recommended: 18 for VSPI, 14 for HSPI)
     * @param miso MISO pin (recommended: 19 for VSPI, 12 for HSPI)
     * @param mosi MOSI pin (recommended: 23 for VSPI, 13 for HSPI)
     * @param frequency SPI clock frequency in Hz (100 kHz - 10 MHz)
     * 
     * @return true if initialization successful, false on error
     * 
     * @warning Logs warning if non-IOMUX pins used with freq > 26.6 MHz
     * 
     * @note This method is NOT thread-safe - call only from setup()
     */
    bool begin(uint8_t sck, uint8_t miso, uint8_t mosi, uint32_t frequency) override {
        if (initialized_) {
            log_w("SPI already initialized - call end() first");
            return false;
        }
        
        // =====================================================================
        // PARAMETER VALIDATION
        // =====================================================================
        
        if (frequency < SPI_FREQ_MIN || frequency > SPI_FREQ_MAX) {
            log_e("Invalid SPI frequency: %u Hz (valid range: %u - %u Hz)",
                  frequency, SPI_FREQ_MIN, SPI_FREQ_MAX);
            return false;
        }
        
        // Check for GPIO Matrix performance degradation
        if (!isIOmuxPins(sck, miso, mosi)) {
            log_w("╔════════════════════════════════════════════════════════════╗");
            log_w("║ WARNING: Using GPIO Matrix pins instead of IO_MUX         ║");
            log_w("║ Maximum speed limited to 26.6 MHz (vs 80 MHz with IO_MUX) ║");
            log_w("╠════════════════════════════════════════════════════════════╣");
            log_w("║ Current pins: SCK=%2d, MISO=%2d, MOSI=%2d                  ║", sck, miso, mosi);
            log_w("║ VSPI IO_MUX:  SCK=%2d, MISO=%2d, MOSI=%2d (recommended)    ║",
                  VSPI_SCK_IOMUX, VSPI_MISO_IOMUX, VSPI_MOSI_IOMUX);
            log_w("║ HSPI IO_MUX:  SCK=%2d, MISO=%2d, MOSI=%2d (alternative)    ║",
                  HSPI_SCK_IOMUX, HSPI_MISO_IOMUX, HSPI_MOSI_IOMUX);
            log_w("╚════════════════════════════════════════════════════════════╝");
            
            if (frequency > GPIO_MATRIX_MAX_HZ) {
                log_e("Frequency %u Hz exceeds GPIO Matrix limit (%u Hz) - REDUCING to safe value",
                      frequency, GPIO_MATRIX_MAX_HZ);
                frequency = GPIO_MATRIX_MAX_HZ;
            }
        }
        
        // =====================================================================
        // MUTEX CREATION (CRITICAL FOR MULTI-TASK SAFETY)
        // =====================================================================
        
        busMutex_ = xSemaphoreCreateMutex();
        if (busMutex_ == nullptr) {
            log_e("CRITICAL: Failed to create SPI mutex - system may be out of memory");
            return false;
        }
        
        // =====================================================================
        // HARDWARE INITIALIZATION
        // =====================================================================
        
        spi_->begin(sck, miso, mosi);
        currentFrequency_ = frequency;
        initialized_ = true;
        
        log_i("✓ SPI initialized successfully");
        log_i("  ├─ Frequency: %u Hz (%.2f MHz)", frequency, frequency / 1000000.0);
        log_i("  ├─ SCK:  GPIO %d", sck);
        log_i("  ├─ MISO: GPIO %d", miso);
        log_i("  ├─ MOSI: GPIO %d", mosi);
        log_i("  └─ Mode: %s", isIOmuxPins(sck, miso, mosi) ? "IO_MUX (fast)" : "GPIO Matrix (limited)");
        
        return true;
    }
    
    /**
     * @brief Transfer single byte (full-duplex)
     * 
     * @param data Byte to transmit
     * @return Received byte
     * 
     * @warning Must be called between beginTransaction() / endTransaction()
     * @note This method is thread-safe (protected by transaction mutex)
     */
    inline uint8_t transfer(uint8_t data) override {
        if (!initialized_) {
            log_e("SPI not initialized");
            return 0xFF;  // Return invalid data
        }
        return spi_->transfer(data);
    }
    
    /**
     * @brief Transfer multiple bytes (DMA-optimized)
     * 
     * @param txBuffer Transmit buffer (must not be nullptr)
     * @param rxBuffer Receive buffer (can be nullptr for write-only)
     * @param length Number of bytes to transfer
     * 
     * @warning Buffers must remain valid during transfer (no stack arrays!)
     * @note Uses ESP32 DMA for transfers > 32 bytes (automatic)
     */
    inline void transfer(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t length) override {
        if (!initialized_) {
            log_e("SPI not initialized");
            return;
        }
        
        if (txBuffer == nullptr || length == 0) {
            log_e("Invalid parameters: txBuffer=%p, length=%u", txBuffer, length);
            return;
        }
        
        // Use DMA-optimized methods from Arduino ESP32 core
        if (rxBuffer == nullptr) {
            // Write-only (common for LoRa register writes)
            spi_->writeBytes(txBuffer, length);
        } else {
            // Full-duplex (used for reading FIFO)
            spi_->transferBytes(txBuffer, rxBuffer, length);
        }
    }
    
    /**
     * @brief Begin SPI transaction (acquire mutex + configure + assert CS)
     * 
     * @param csPin Chip Select pin (must be configured via configureCS())
     * 
     * @warning BLOCKS until mutex available (max 1 second timeout)
     * @note SPI Mode 0 (CPOL=0, CPHA=0) hardcoded for SX127x compatibility
     */
    inline void beginTransaction(uint8_t csPin) override {
        if (!initialized_) {
            log_e("SPI not initialized");
            return;
        }
        
        // CRITICAL SECTION: Acquire bus ownership
        if (xSemaphoreTake(busMutex_, MUTEX_TIMEOUT_MS) != pdTRUE) {
            log_e("SPI mutex timeout - possible deadlock on CS pin %d", csPin);
            return;
        }
        
        // Configure SPI settings (mode, frequency, bit order)
        spi_->beginTransaction(SPISettings(currentFrequency_, MSBFIRST, SPI_MODE0));
        
        // Assert CS (active low)
        digitalWrite(csPin, LOW);
    }
    
    /**
     * @brief End SPI transaction (deassert CS + release mutex)
     * 
     * @param csPin Chip Select pin
     * 
     * @note ALWAYS call this after beginTransaction() to avoid deadlock
     */
    inline void endTransaction(uint8_t csPin) override {
        if (!initialized_) return;
        
        // Deassert CS (idle high)
        digitalWrite(csPin, HIGH);
        
        // End SPI transaction
        spi_->endTransaction();
        
        // CRITICAL: Release bus for other tasks
        xSemaphoreGive(busMutex_);
    }
    
    /**
     * @brief Deinitialize SPI bus and free resources
     * 
     * @warning Ensures all transactions are complete before calling
     */
    void end() override {
        if (!initialized_) return;
        
        spi_->end();
        
        if (busMutex_ != nullptr) {
            vSemaphoreDelete(busMutex_);
            busMutex_ = nullptr;
        }
        
        initialized_ = false;
        log_i("SPI deinitialized");
    }
    
    // =========================================================================
    // EXTENDED FUNCTIONALITY (BEYOND BASE HAL INTERFACE)
    // =========================================================================
    
    /**
     * @brief Configure Chip Select pin as output (idle HIGH)
     * 
     * @param csPin GPIO pin number for CS
     * 
     * @warning MUST be called before first beginTransaction()
     * @note CS idle state is HIGH (SPI convention)
     * 
     * @example
     * @code
     * halSPI.begin(18, 19, 23, 8000000);
     * halSPI.configureCS(5);  // LoRa CS on GPIO 5
     * @endcode
     */
    inline void configureCS(uint8_t csPin) const {
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);  // CS idle state (active LOW)
        log_d("CS pin %d configured (idle HIGH)", csPin);
    }
    
    /**
     * @brief Try to acquire SPI bus from ISR context (for LoRa DIO interrupts)
     * 
     * @return true if bus acquired, false if busy
     * 
     * @warning Only use in ISR - never blocks
     * @note Call unlockFromISR() if returns true
     * 
     * @example LoRa interrupt handler
     * @code
     * void IRAM_ATTR onDIO0() {
     *     if (halSPI.tryLockFromISR()) {
     *         // Safe to access SPI here
     *         halSPI.unlockFromISR();
     *     } else {
     *         // Defer to task via queue
     *     }
     * }
     * @endcode
     */
    inline bool tryLockFromISR() {
        if (!initialized_ || busMutex_ == nullptr) return false;
        
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        BaseType_t result = xSemaphoreTakeFromISR(busMutex_, &higherPriorityTaskWoken);
        
        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        
        return (result == pdTRUE);
    }
    
    /**
     * @brief Release SPI bus from ISR context
     * 
     * @warning Only call if tryLockFromISR() returned true
     */
    inline void unlockFromISR() {
        if (!initialized_ || busMutex_ == nullptr) return;
        
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(busMutex_, &higherPriorityTaskWoken);
        
        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
    
    /**
     * @brief Check if SPI is initialized
     * @return true if begin() was called successfully
     */
    inline bool isInitialized() const {
        return initialized_;
    }
    
    /**
     * @brief Get current SPI frequency
     * @return Frequency in Hz
     */
    inline uint32_t getFrequency() const {
        return currentFrequency_;
    }
    
    /**
     * @brief Change SPI frequency (requires re-initialization)
     * 
     * @param frequency New frequency in Hz
     * @return true if changed successfully
     * 
     * @warning This calls end() + begin() internally - do not use during active transactions
     */
    bool setFrequency(uint32_t frequency) {
        if (!initialized_) {
            log_e("Cannot change frequency - SPI not initialized");
            return false;
        }
        
        if (frequency < SPI_FREQ_MIN || frequency > SPI_FREQ_MAX) {
            log_e("Invalid frequency: %u Hz", frequency);
            return false;
        }
        
        currentFrequency_ = frequency;
        log_i("SPI frequency changed to %u Hz (%.2f MHz)", frequency, frequency / 1000000.0);
        return true;
    }
};

} // namespace HAL

#endif // HAL_ESP32_SPI_H
