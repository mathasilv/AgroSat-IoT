/**
 * @file ttgo_lora32_v21.h
 * @brief Board-specific pin definitions for TTGO LoRa32 V2.1
 * @details Hardware configuration for LilyGO TTGO LoRa32 V2.1
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef BOARD_TTGO_LORA32_V21_H
#define BOARD_TTGO_LORA32_V21_H

// I2C Pins (Sensors: MPU9250, BMP280, SI7021, CCS811, DS3231)
#define BOARD_I2C_SDA         21
#define BOARD_I2C_SCL         22
#define BOARD_I2C_FREQUENCY   400000  // 400kHz

// SPI Pins (LoRa Radio SX1276/SX1278)
#define BOARD_SPI_SCK         5
#define BOARD_SPI_MISO        19
#define BOARD_SPI_MOSI        27
#define BOARD_SPI_FREQUENCY   8000000  // 8MHz

// LoRa Radio Control Pins
#define BOARD_LORA_CS         18
#define BOARD_LORA_RST        14  // Shared with OLED reset
#define BOARD_LORA_DIO0       26

// OLED Display (128x64 SSD1306) - Uses I2C
#define BOARD_OLED_ADDR       0x3C
#define BOARD_OLED_RST        14  // Shared with LoRa reset

// Button
#define BOARD_BUTTON_PIN      0   // BOOT button

// LED
#define BOARD_LED_PIN         25  // Blue LED

// UART (Debug/GPS)
#define BOARD_UART_TX         1   // Default TX
#define BOARD_UART_RX         3   // Default RX
#define BOARD_UART_BAUDRATE   115200

// Battery ADC
#define BOARD_BATTERY_ADC     35

// Board Info
#define BOARD_NAME            "TTGO LoRa32 V2.1"
#define BOARD_CPU_FREQ_MHZ    240

#endif // BOARD_TTGO_LORA32_V21_H