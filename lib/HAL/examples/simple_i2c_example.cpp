/**
 * @file simple_i2c_example.cpp
 * @brief Exemplo simples de uso do HAL I2C
 * @details Demonstra como ler sensores usando abstração de hardware
 * 
 * Este exemplo NÃO compila diretamente - é apenas referência!
 * Copie o código para seu main.cpp conforme necessário.
 */

#include <Arduino.h>
#include "hal/platform/esp32/ESP32_I2C.h"
#include "hal/board/ttgo_lora32_v21.h"

// Criar instância global do HAL I2C
HAL::ESP32_I2C* i2c_bus = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== AgroSat-IoT HAL Example ===");
    Serial.printf("Board: %s\n", BOARD_NAME);
    
    // Criar e inicializar HAL I2C
    i2c_bus = new HAL::ESP32_I2C(&Wire);
    i2c_bus->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    Serial.println("I2C initialized!");
    
    // === Exemplo 1: MPU9250 IMU ===
    Serial.println("\n--- MPU9250 Test ---");
    
    // Wake up MPU9250
    if (i2c_bus->writeRegister(0x68, 0x6B, 0x00)) {
        Serial.println("MPU9250 woke up!");
        
        delay(100);
        
        // Read WHO_AM_I
        uint8_t whoami = i2c_bus->readRegister(0x68, 0x75);
        Serial.printf("WHO_AM_I: 0x%02X (expected 0x71)\n", whoami);
        
        // Read temperature
        uint8_t temp_data[2];
        i2c_bus->write(0x68, (const uint8_t[]){0x41}, 1);  // Point to TEMP_OUT_H
        if (i2c_bus->read(0x68, temp_data, 2)) {
            int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
            float temp_c = temp_raw / 340.0 + 36.53;
            Serial.printf("Temperature: %.2f°C\n", temp_c);
        }
    }
    
    // === Exemplo 2: BMP280 Pressure Sensor ===
    Serial.println("\n--- BMP280 Test ---");
    
    uint8_t bmp_id = i2c_bus->readRegister(0x76, 0xD0);
    Serial.printf("BMP280 ID: 0x%02X (expected 0x58)\n", bmp_id);
    
    // === Exemplo 3: Scan I2C Bus ===
    Serial.println("\n--- I2C Bus Scan ---");
    Serial.println("Scanning for devices...");
    
    int deviceCount = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        // Try to write 0 bytes to check ACK
        if (i2c_bus->write(addr, nullptr, 0)) {
            Serial.printf("Device found at 0x%02X\n", addr);
            deviceCount++;
        }
    }
    
    Serial.printf("Scan complete. Found %d devices.\n", deviceCount);
}

void loop() {
    // Read MPU9250 temperature periodically
    uint8_t temp_data[2];
    i2c_bus->write(0x68, (const uint8_t[]){0x41}, 1);
    
    if (i2c_bus->read(0x68, temp_data, 2)) {
        int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
        float temp_c = temp_raw / 340.0 + 36.53;
        Serial.printf("[%lu] MPU Temp: %.2f°C\n", millis(), temp_c);
    }
    
    delay(1000);
}

/**
 * NOTAS IMPORTANTES:
 * 
 * 1. Este exemplo usa HAL::I2C - código PORTÁVEL!
 *    O mesmo código funcionará em STM32 mudando apenas:
 *    HAL::ESP32_I2C -> HAL::STM32_I2C
 * 
 * 2. Endereços I2C dos sensores AgroSat-IoT:
 *    - MPU9250: 0x68
 *    - BMP280: 0x76 ou 0x77
 *    - SI7021: 0x40
 *    - CCS811: 0x5A ou 0x5B
 *    - DS3231: 0x68 (conflita com MPU - usar I2C mux ou diferentes buses)
 * 
 * 3. Para usar em produção, migre para SensorManager com dependency injection
 */