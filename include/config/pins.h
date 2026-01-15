/**
 * @file pins.h
 * @brief Mapeamento de pinos GPIO para o hardware AgroSat-IoT
 * 
 * @details Definições de pinos para a placa LilyGo T3 V1.6.1 (ESP32 + LoRa):
 *          - GPS via UART2
 *          - LoRa SX1276 via VSPI
 *          - SD Card via HSPI
 *          - Sensores I2C via Wire
 *          - ADC para bateria
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Diagrama de Conexões
 * ```
 * ESP32 (LilyGo T3 V1.6.1)
 * ┌─────────────────────────────────────┐
 * │                                     │
 * │  GPS (UART2)     LoRa (VSPI)        │
 * │  ├─ RX: GPIO34   ├─ SCK:  GPIO5     │
 * │  └─ TX: GPIO12   ├─ MISO: GPIO19    │
 * │                  ├─ MOSI: GPIO27    │
 * │  I2C (Wire)      ├─ CS:   GPIO18    │
 * │  ├─ SDA: GPIO21  ├─ RST:  GPIO23    │
 * │  └─ SCL: GPIO22  └─ DIO0: GPIO26    │
 * │                                     │
 * │  SD Card (HSPI)  Outros             │
 * │  ├─ CS:   GPIO13 ├─ LED:  GPIO25    │
 * │  ├─ MOSI: GPIO15 ├─ BTN:  GPIO4     │
 * │  ├─ MISO: GPIO2  └─ VBAT: GPIO35    │
 * │  └─ CLK:  GPIO14                    │
 * └─────────────────────────────────────┘
 * ```
 * 
 * @warning GPIO2 usado pelo SD Card - não usar para boot
 * @warning GPIO34-39 são somente entrada (sem pull-up interno)
 */

#ifndef PINS_H
#define PINS_H

//=============================================================================
// GPS (UART2)
//=============================================================================
#define GPS_RX_PIN 34           ///< RX do GPS (entrada, somente leitura)
#define GPS_TX_PIN 12           ///< TX para GPS (saída)
#define GPS_BAUD_RATE 9600      ///< Baudrate padrão NEO-6M/M8N

//=============================================================================
// LORA SX1276 (VSPI)
//=============================================================================
#define LORA_CS 18              ///< Chip Select (NSS)
#define LORA_RST 23             ///< Reset (ativo baixo)
#define LORA_DIO0 26            ///< Interrupção RX/TX Done 

//=============================================================================
// SD CARD (HSPI)
//=============================================================================
#define SD_CS 13                ///< Chip Select
#define SD_MOSI 15              ///< Master Out Slave In
#define SD_MISO 2               ///< Master In Slave Out (cuidado: strapping pin)
#define SD_SCLK 14              ///< SPI Clock

//=============================================================================
// I2C SENSORS (Wire)
//=============================================================================
#define SENSOR_I2C_SDA 21       ///< I2C Data
#define SENSOR_I2C_SCL 22       ///< I2C Clock

//=============================================================================
// POWER MANAGEMENT
//=============================================================================
#define BATTERY_PIN 35          ///< ADC para tensão da bateria (via divisor) 

//=============================================================================
// GPIO AUXILIARES
//=============================================================================
#ifndef LED_BUILTIN
#define LED_BUILTIN 25          ///< LED onboard
#endif
#define BUTTON_PIN 4            ///< Botão de usuário (INPUT_PULLUP)

//=============================================================================
// ENDEREÇOS I2C DOS SENSORES
//=============================================================================
#define MPU9250_ADDRESS 0x69    ///< IMU (AD0=HIGH)

#endif // PINS_H