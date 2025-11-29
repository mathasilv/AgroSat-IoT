/**
 * @file SX127x_registers.h
 * @brief Complete SX127x Register Map and Constants
 * @details Based on Semtech SX1276/77/78/79 Datasheet Rev 7 (2020)
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-29
 * @version 1.0.0
 * 
 * @note All values are compile-time constants (constexpr) for zero runtime overhead
 * 
 * Reference Documents:
 * - SX1276/77/78/79 Datasheet (DS_SX1276-7-8-9_W_APP_V7.pdf)
 * - AN1200.13 - LoRa Modem Designer's Guide
 * - Semtech Application Note 1200.22 - LoRa Sensitivity
 */

#ifndef SX127X_REGISTERS_H
#define SX127X_REGISTERS_H

#include <stdint.h>

namespace SX127x {

// ============================================================================
// REGISTER ADDRESSES (Common to FSK/LoRa modes)
// ============================================================================

namespace Registers {
    // Common Configuration Registers
    constexpr uint8_t REG_FIFO                  = 0x00;  ///< FIFO R/W access
    constexpr uint8_t REG_OP_MODE               = 0x01;  ///< Operating mode & LoRa/FSK selection
    constexpr uint8_t REG_FRF_MSB               = 0x06;  ///< RF Carrier Frequency MSB
    constexpr uint8_t REG_FRF_MID               = 0x07;  ///< RF Carrier Frequency MID
    constexpr uint8_t REG_FRF_LSB               = 0x08;  ///< RF Carrier Frequency LSB
    constexpr uint8_t REG_PA_CONFIG             = 0x09;  ///< PA selection and output power
    constexpr uint8_t REG_PA_RAMP               = 0x0A;  ///< Control of PA ramp time
    constexpr uint8_t REG_OCP                   = 0x0B;  ///< Over Current Protection
    constexpr uint8_t REG_LNA                   = 0x0C;  ///< LNA settings
    constexpr uint8_t REG_DIO_MAPPING_1         = 0x40;  ///< DIO0-DIO3 mapping
    constexpr uint8_t REG_DIO_MAPPING_2         = 0x41;  ///< DIO4-DIO5 mapping + CLK out
    constexpr uint8_t REG_VERSION               = 0x42;  ///< Semtech chip version (0x12 for SX1276)
    constexpr uint8_t REG_TCXO                  = 0x4B;  ///< TCXO or XTAL input
    constexpr uint8_t REG_PA_DAC                = 0x4D;  ///< High power PA settings
    
    // LoRa Mode Specific Registers
    constexpr uint8_t REG_FIFO_ADDR_PTR         = 0x0D;  ///< FIFO SPI pointer
    constexpr uint8_t REG_FIFO_TX_BASE_ADDR     = 0x0E;  ///< FIFO TX base address
    constexpr uint8_t REG_FIFO_RX_BASE_ADDR     = 0x0F;  ///< FIFO RX base address
    constexpr uint8_t REG_FIFO_RX_CURRENT_ADDR  = 0x10;  ///< FIFO RX current address
    constexpr uint8_t REG_IRQ_FLAGS_MASK        = 0x11;  ///< IRQ mask
    constexpr uint8_t REG_IRQ_FLAGS             = 0x12;  ///< IRQ flags
    constexpr uint8_t REG_RX_NB_BYTES           = 0x13;  ///< Number of received bytes
    constexpr uint8_t REG_RX_HEADER_CNT_MSB     = 0x14;  ///< Valid header counter MSB
    constexpr uint8_t REG_RX_HEADER_CNT_LSB     = 0x15;  ///< Valid header counter LSB
    constexpr uint8_t REG_RX_PACKET_CNT_MSB     = 0x16;  ///< Valid packet counter MSB
    constexpr uint8_t REG_RX_PACKET_CNT_LSB     = 0x17;  ///< Valid packet counter LSB
    constexpr uint8_t REG_MODEM_STAT            = 0x18;  ///< Modem status
    constexpr uint8_t REG_PKT_SNR_VALUE         = 0x19;  ///< Last packet SNR (dB)
    constexpr uint8_t REG_PKT_RSSI_VALUE        = 0x1A;  ///< Last packet RSSI
    constexpr uint8_t REG_RSSI_VALUE            = 0x1B;  ///< Current RSSI (channel)
    constexpr uint8_t REG_HOP_CHANNEL           = 0x1C;  ///< FHSS status
    constexpr uint8_t REG_MODEM_CONFIG_1        = 0x1D;  ///< BW, CR, Implicit Header
    constexpr uint8_t REG_MODEM_CONFIG_2        = 0x1E;  ///< SF, CRC, RX timeout
    constexpr uint8_t REG_SYMB_TIMEOUT_LSB      = 0x1F;  ///< RX timeout LSB
    constexpr uint8_t REG_PREAMBLE_MSB          = 0x20;  ///< Preamble length MSB
    constexpr uint8_t REG_PREAMBLE_LSB          = 0x21;  ///< Preamble length LSB
    constexpr uint8_t REG_PAYLOAD_LENGTH        = 0x22;  ///< Payload length
    constexpr uint8_t REG_MAX_PAYLOAD_LENGTH    = 0x23;  ///< Max payload length
    constexpr uint8_t REG_HOP_PERIOD            = 0x24;  ///< FHSS hop period
    constexpr uint8_t REG_FIFO_RX_BYTE_ADDR     = 0x25;  ///< FIFO RX byte address
    constexpr uint8_t REG_MODEM_CONFIG_3        = 0x26;  ///< Low data rate optimize, AGC
    constexpr uint8_t REG_DETECTION_OPTIMIZE    = 0x31;  ///< LoRa detection optimize (SF6 support)
    constexpr uint8_t REG_INVERT_IQ             = 0x33;  ///< Invert IQ signals
    constexpr uint8_t REG_DETECTION_THRESHOLD   = 0x37;  ///< LoRa detection threshold
    constexpr uint8_t REG_SYNC_WORD             = 0x39;  ///< LoRa sync word
} // namespace Registers

// ============================================================================
// OPERATING MODES (REG_OP_MODE - 0x01)
// ============================================================================

namespace OpMode {
    // Long Range Mode (LoRa vs FSK)
    constexpr uint8_t LONG_RANGE_MODE           = 0x80;  ///< 1=LoRa, 0=FSK/OOK
    constexpr uint8_t MODULATION_TYPE_MASK      = 0x60;  ///< FSK modulation type
    constexpr uint8_t ACCESS_SHARED_REG         = 0x40;  ///< Access to FSK registers in LoRa mode
    
    // Device Modes (bits 2-0)
    constexpr uint8_t MODE_MASK                 = 0x07;
    constexpr uint8_t MODE_SLEEP                = 0x00;  ///< Sleep (lowest power)
    constexpr uint8_t MODE_STDBY                = 0x01;  ///< Standby
    constexpr uint8_t MODE_FSTX                 = 0x02;  ///< Frequency synthesis TX
    constexpr uint8_t MODE_TX                   = 0x03;  ///< Transmit
    constexpr uint8_t MODE_FSRX                 = 0x04;  ///< Frequency synthesis RX
    constexpr uint8_t MODE_RXCONTINUOUS         = 0x05;  ///< Receive continuous
    constexpr uint8_t MODE_RXSINGLE             = 0x06;  ///< Receive single
    constexpr uint8_t MODE_CAD                  = 0x07;  ///< Channel Activity Detection
} // namespace OpMode

// ============================================================================
// PA CONFIGURATION (REG_PA_CONFIG - 0x09)
// ============================================================================

namespace PaConfig {
    constexpr uint8_t PA_SELECT_MASK            = 0x80;
    constexpr uint8_t PA_SELECT_RFO             = 0x00;  ///< RFO pin (max +14 dBm)
    constexpr uint8_t PA_SELECT_PABOOST         = 0x80;  ///< PA_BOOST pin (max +20 dBm)
    constexpr uint8_t MAX_POWER_MASK            = 0x70;  ///< Max power (bits 6-4)
    constexpr uint8_t OUTPUT_POWER_MASK         = 0x0F;  ///< Output power (bits 3-0)
} // namespace PaConfig

// ============================================================================
// PA DAC (REG_PA_DAC - 0x4D) - High Power +20dBm Operation
// ============================================================================

namespace PaDac {
    constexpr uint8_t PA_DAC_NORMAL             = 0x84;  ///< Default (max +17 dBm)
    constexpr uint8_t PA_DAC_HIGH_POWER         = 0x87;  ///< Enable +20 dBm (PA_BOOST only)
} // namespace PaDac

// ============================================================================
// LNA GAIN (REG_LNA - 0x0C)
// ============================================================================

namespace Lna {
    constexpr uint8_t LNA_GAIN_MASK             = 0xE0;
    constexpr uint8_t LNA_GAIN_G1               = 0x20;  ///< Max gain
    constexpr uint8_t LNA_GAIN_G2               = 0x40;
    constexpr uint8_t LNA_GAIN_G3               = 0x60;
    constexpr uint8_t LNA_GAIN_G4               = 0x80;
    constexpr uint8_t LNA_GAIN_G5               = 0xA0;
    constexpr uint8_t LNA_GAIN_G6               = 0xC0;  ///< Min gain
    constexpr uint8_t LNA_BOOST_LF_MASK         = 0x18;
    constexpr uint8_t LNA_BOOST_HF_MASK         = 0x03;
    constexpr uint8_t LNA_BOOST_HF_ON           = 0x03;  ///< +150% LNA current (better sensitivity)
    constexpr uint8_t LNA_BOOST_HF_OFF          = 0x00;
} // namespace Lna

// ============================================================================
// IRQ FLAGS (REG_IRQ_FLAGS - 0x12)
// ============================================================================

namespace IrqFlags {
    constexpr uint8_t RX_TIMEOUT                = 0x80;  ///< RX timeout
    constexpr uint8_t RX_DONE                   = 0x40;  ///< Packet received
    constexpr uint8_t PAYLOAD_CRC_ERROR         = 0x20;  ///< CRC error
    constexpr uint8_t VALID_HEADER              = 0x10;  ///< Valid LoRa header
    constexpr uint8_t TX_DONE                   = 0x08;  ///< Transmission done
    constexpr uint8_t CAD_DONE                  = 0x04;  ///< CAD complete
    constexpr uint8_t FHSS_CHANGE_CHANNEL       = 0x02;  ///< FHSS changed channel
    constexpr uint8_t CAD_DETECTED              = 0x01;  ///< CAD detected signal
    
    constexpr uint8_t CLEAR_ALL                 = 0xFF;  ///< Clear all IRQ flags
} // namespace IrqFlags

// ============================================================================
// MODEM CONFIG 1 (REG_MODEM_CONFIG_1 - 0x1D)
// ============================================================================

namespace ModemConfig1 {
    // Bandwidth
    constexpr uint8_t BW_MASK                   = 0xF0;
    constexpr uint8_t BW_7_8_KHZ                = 0x00;
    constexpr uint8_t BW_10_4_KHZ               = 0x10;
    constexpr uint8_t BW_15_6_KHZ               = 0x20;
    constexpr uint8_t BW_20_8_KHZ               = 0x30;
    constexpr uint8_t BW_31_25_KHZ              = 0x40;
    constexpr uint8_t BW_41_7_KHZ               = 0x50;
    constexpr uint8_t BW_62_5_KHZ               = 0x60;
    constexpr uint8_t BW_125_KHZ                = 0x70;  ///< Default
    constexpr uint8_t BW_250_KHZ                = 0x80;
    constexpr uint8_t BW_500_KHZ                = 0x90;
    
    // Coding Rate
    constexpr uint8_t CR_MASK                   = 0x0E;
    constexpr uint8_t CR_4_5                    = 0x02;  ///< CR = 4/5
    constexpr uint8_t CR_4_6                    = 0x04;  ///< CR = 4/6
    constexpr uint8_t CR_4_7                    = 0x06;  ///< CR = 4/7
    constexpr uint8_t CR_4_8                    = 0x08;  ///< CR = 4/8
    
    // Header Mode
    constexpr uint8_t IMPLICIT_HEADER_MODE      = 0x01;  ///< 1=implicit, 0=explicit
} // namespace ModemConfig1

// ============================================================================
// MODEM CONFIG 2 (REG_MODEM_CONFIG_2 - 0x1E)
// ============================================================================

namespace ModemConfig2 {
    // Spreading Factor
    constexpr uint8_t SF_MASK                   = 0xF0;
    constexpr uint8_t SF_6                      = 0x60;  ///< SF6 (requires special settings)
    constexpr uint8_t SF_7                      = 0x70;  ///< SF7
    constexpr uint8_t SF_8                      = 0x80;  ///< SF8
    constexpr uint8_t SF_9                      = 0x90;  ///< SF9
    constexpr uint8_t SF_10                     = 0xA0;  ///< SF10
    constexpr uint8_t SF_11                     = 0xB0;  ///< SF11
    constexpr uint8_t SF_12                     = 0xC0;  ///< SF12 (max range)
    
    // TX Mode
    constexpr uint8_t TX_CONTINUOUS_MODE        = 0x08;  ///< Continuous TX (testing)
    
    // CRC
    constexpr uint8_t RX_PAYLOAD_CRC_ON         = 0x04;  ///< Enable CRC check
    
    // RX Timeout (MSB bits 1-0)
    constexpr uint8_t SYMB_TIMEOUT_MSB_MASK     = 0x03;
} // namespace ModemConfig2

// ============================================================================
// MODEM CONFIG 3 (REG_MODEM_CONFIG_3 - 0x26)
// ============================================================================

namespace ModemConfig3 {
    constexpr uint8_t LOW_DATA_RATE_OPTIMIZE    = 0x08;  ///< Mandatory for SF11/SF12 @ BW125
    constexpr uint8_t AGC_AUTO_ON               = 0x04;  ///< Enable AGC
} // namespace ModemConfig3

// ============================================================================
// DIO MAPPING (REG_DIO_MAPPING_1 - 0x40)
// ============================================================================

namespace DioMapping {
    // DIO0 Mapping (bits 7-6)
    constexpr uint8_t DIO0_MASK                 = 0xC0;
    constexpr uint8_t DIO0_RX_DONE              = 0x00;  ///< RX mode: RxDone
    constexpr uint8_t DIO0_TX_DONE              = 0x40;  ///< TX mode: TxDone
    constexpr uint8_t DIO0_CAD_DONE             = 0x80;  ///< CAD mode: CadDone
    
    // DIO1 Mapping (bits 5-4)
    constexpr uint8_t DIO1_MASK                 = 0x30;
    constexpr uint8_t DIO1_RX_TIMEOUT           = 0x00;  ///< RX mode: RxTimeout
    constexpr uint8_t DIO1_FHSS_CHANGE_CHANNEL  = 0x10;  ///< FHSS: FhssChangeChannel
    constexpr uint8_t DIO1_CAD_DETECTED         = 0x20;  ///< CAD mode: CadDetected
} // namespace DioMapping

// ============================================================================
// FREQUENCY CALCULATION
// ============================================================================

namespace Frequency {
    constexpr uint32_t FXOSC                    = 32000000;  ///< 32 MHz crystal
    constexpr uint32_t FSTEP                    = 61;        ///< FXOSC / 2^19 ≈ 61.03515625 Hz
    
    /**
     * @brief Convert frequency in Hz to 24-bit register value
     * @param freqHz Frequency in Hz (e.g., 915000000 for 915 MHz)
     * @return 24-bit FRF value to write to REG_FRF_MSB/MID/LSB
     */
    constexpr uint32_t calculateFrf(uint32_t freqHz) {
        return (uint32_t)((uint64_t)freqHz * (1 << 19) / FXOSC);
    }
    
    // Common ISM Band Frequencies
    constexpr uint32_t FREQ_433_MHZ             = 433000000;
    constexpr uint32_t FREQ_868_MHZ             = 868000000;
    constexpr uint32_t FREQ_915_MHZ             = 915000000;
} // namespace Frequency

// ============================================================================
// HARDWARE CONSTANTS
// ============================================================================

namespace Hardware {
    constexpr uint8_t CHIP_VERSION_SX1276       = 0x12;  ///< Expected value in REG_VERSION
    constexpr uint8_t CHIP_VERSION_SX1277       = 0x12;
    constexpr uint8_t CHIP_VERSION_SX1278       = 0x12;
    constexpr uint8_t CHIP_VERSION_SX1279       = 0x12;
    
    constexpr uint16_t FIFO_SIZE                 = 256;   // ✅ uint16_t ao invés de uint8_t
    constexpr uint8_t MAX_PAYLOAD_LENGTH        = 255;
    
    constexpr uint8_t FIFO_TX_BASE_ADDR_DEFAULT = 0x00;  ///< TX FIFO base (start)
    constexpr uint8_t FIFO_RX_BASE_ADDR_DEFAULT = 0x00;  ///< RX FIFO base (shared with TX)
} // namespace Hardware

// ============================================================================
// TIMING CONSTANTS (all in milliseconds)
// ============================================================================

namespace Timing {
    constexpr uint32_t RESET_PULSE_MS           = 10;    ///< Reset pulse width
    constexpr uint32_t RESET_WAIT_MS            = 10;    ///< Wait after reset
    constexpr uint32_t MODE_SWITCH_MS           = 5;     ///< Mode switch stabilization
    constexpr uint32_t PLL_LOCK_MS              = 100;   ///< PLL lock timeout
    constexpr uint32_t RX_TIMEOUT_MS            = 3000;  ///< RX timeout (single mode)
    constexpr uint32_t TX_TIMEOUT_MS_NORMAL     = 5000;  ///< TX timeout (SF7-SF10)
    constexpr uint32_t TX_TIMEOUT_MS_LONG       = 15000; ///< TX timeout (SF11-SF12)
} // namespace Timing

// ============================================================================
// RSSI / SNR CALCULATION CONSTANTS
// ============================================================================

namespace RssiSnr {
    // RSSI offset depends on HF/LF port and frequency
    // For 860-1020 MHz (SX1276/77/78/79):
    constexpr int16_t RSSI_OFFSET_HF            = -157;  ///< HF port (PA_BOOST)
    constexpr int16_t RSSI_OFFSET_LF            = -164;  ///< LF port (RFO)
    
    // SNR is stored as 2's complement in 1/4 dB steps
    constexpr float SNR_SCALE                   = 0.25f; ///< SNR = value * 0.25 dB
} // namespace RssiSnr

} // namespace SX127x

#endif // SX127X_REGISTERS_H
