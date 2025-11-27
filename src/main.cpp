/**
 * @file test_hal_basic.cpp
 * @brief Teste básico do HAL I2C
 * @details Use este arquivo para validar o HAL antes de migrar managers
 * 
 * COMO USAR:
 * 1. Comente o conteúdo do src/main.cpp
 * 2. Copie este código para src/main.cpp
 * 3. Compile e faça upload: pio run -t upload -t monitor
 * 4. Verifique se sensores são detectados
 * 
 * RESULTADO ESPERADO:
 * - I2C scan deve mostrar: 0x40 (SI7021), 0x68 (MPU9250), 0x76 (BMP280)
 * - MPU9250 WHO_AM_I deve ser 0x71
 * - Temperatura MPU deve variar entre 30-40°C
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#include <Arduino.h>
#include "hal/platform/esp32/ESP32_I2C.h"
#include "hal/board/ttgo_lora32_v21.h"

// HAL global
HAL::ESP32_I2C* i2c_bus = nullptr;

// Sensor addresses
const uint8_t MPU9250_ADDR = 0x68;
const uint8_t BMP280_ADDR  = 0x76;
const uint8_t SI7021_ADDR  = 0x40;
const uint8_t CCS811_ADDR  = 0x5A;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Aguardar monitor serial abrir
    
    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("  AgroSat-IoT - HAL I2C Test");
    Serial.println("========================================");
    Serial.printf("Board: %s\n", BOARD_NAME);
    Serial.printf("I2C Pins: SDA=%d, SCL=%d\n", BOARD_I2C_SDA, BOARD_I2C_SCL);
    Serial.printf("I2C Freq: %d Hz\n", BOARD_I2C_FREQUENCY);
    Serial.println("");
    
    // ========================================
    // TESTE 1: Criar e inicializar HAL I2C
    // ========================================
    Serial.println("[TEST 1] Initializing HAL I2C...");
    
    i2c_bus = new HAL::ESP32_I2C(&Wire);
    
    if (i2c_bus->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY)) {
        Serial.println("  ✅ HAL I2C initialized successfully");
    } else {
        Serial.println("  ❌ HAL I2C initialization FAILED");
        while(1) { delay(1000); }  // Stop here
    }
    
    delay(100);
    
    // ========================================
    // TESTE 2: Scan I2C Bus
    // ========================================
    Serial.println("\n[TEST 2] Scanning I2C bus...");
    
    int deviceCount = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_bus->write(addr, nullptr, 0)) {
            Serial.printf("  Found device at 0x%02X", addr);
            
            // Identificar dispositivo
            switch (addr) {
                case 0x68: Serial.println(" - MPU9250/MPU6050 ✅"); break;
                case 0x76:
                case 0x77: Serial.println(" - BMP280 ✅"); break;
                case 0x40: Serial.println(" - SI7021 ✅"); break;
                case 0x5A:
                case 0x5B: Serial.println(" - CCS811 ✅"); break;
                default:   Serial.println(" - Unknown"); break;
            }
            deviceCount++;
        }
    }
    
    Serial.printf("\n  Total devices found: %d\n", deviceCount);
    
    if (deviceCount == 0) {
        Serial.println("  ⚠️ WARNING: No I2C devices detected!");
        Serial.println("  Check wiring and sensor power.");
    }
    
    // ========================================
    // TESTE 3: MPU9250 Communication
    // ========================================
    Serial.println("\n[TEST 3] Testing MPU9250...");
    
    // Wake up MPU9250 (PWR_MGMT_1 register)
    Serial.println("  Sending wake-up command...");
    if (i2c_bus->writeRegister(MPU9250_ADDR, 0x6B, 0x00)) {
        Serial.println("  ✅ Wake-up command sent");
    } else {
        Serial.println("  ❌ Wake-up command FAILED");
    }
    
    delay(100);
    
    // Read WHO_AM_I register (0x75)
    Serial.println("  Reading WHO_AM_I register...");
    uint8_t whoami = i2c_bus->readRegister(MPU9250_ADDR, 0x75);
    Serial.printf("  WHO_AM_I: 0x%02X ", whoami);
    
    if (whoami == 0x71) {
        Serial.println("(MPU9250 detected!) ✅");
    } else if (whoami == 0x68) {
        Serial.println("(MPU6050 detected!) ✅");
    } else {
        Serial.println("❌ FAILED - unexpected value");
    }
    
    // ========================================
    // TESTE 4: BMP280 Communication
    // ========================================
    Serial.println("\n[TEST 4] Testing BMP280...");
    
    uint8_t bmp_id = i2c_bus->readRegister(BMP280_ADDR, 0xD0);
    Serial.printf("  Chip ID: 0x%02X ", bmp_id);
    
    if (bmp_id == 0x58) {
        Serial.println("(BMP280 detected!) ✅");
    } else if (bmp_id == 0x60) {
        Serial.println("(BME280 detected!) ✅");
    } else {
        Serial.println("❌ FAILED - unexpected value");
    }
    
    // ========================================
    // TESTE 5: SI7021 Communication
    // ========================================
    Serial.println("\n[TEST 5] Testing SI7021...");
    
    // Soft reset
    const uint8_t CMD_RESET = 0xFE;
    if (i2c_bus->write(SI7021_ADDR, &CMD_RESET, 1)) {
        Serial.println("  ✅ Reset command sent");
        delay(15);
        
        // Read user register
        const uint8_t CMD_READ_USER = 0xE7;
        uint8_t userReg = i2c_bus->readRegister(SI7021_ADDR, CMD_READ_USER);
        Serial.printf("  User register: 0x%02X ✅\n", userReg);
    } else {
        Serial.println("  ❌ SI7021 communication FAILED");
    }
    
    // ========================================
    // Summary
    // ========================================
    Serial.println("\n========================================");
    Serial.println("  HAL I2C Test Complete!");
    Serial.println("========================================");
    Serial.println("\nStarting continuous temperature reading...");
    Serial.println("(Press reset to re-run tests)\n");
}

void loop() {
    // Read MPU9250 temperature continuously
    
    // Point to TEMP_OUT_H register (0x41)
    uint8_t reg = 0x41;
    i2c_bus->write(MPU9250_ADDR, &reg, 1);
    
    // Read 2 bytes (TEMP_OUT_H, TEMP_OUT_L)
    uint8_t temp_data[2];
    if (i2c_bus->read(MPU9250_ADDR, temp_data, 2)) {
        int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
        float temp_celsius = (temp_raw / 340.0) + 36.53;
        
        Serial.printf("[%7lu ms] MPU9250 Temperature: %5.2f °C\n", 
                     millis(), temp_celsius);
    } else {
        Serial.println("[ERROR] Failed to read temperature");
    }
    
    delay(1000);  // Read every 1 second
}

/**
 * ========================================
 * NOTAS IMPORTANTES
 * ========================================
 * 
 * 1. Este teste valida que o HAL funciona corretamente
 *    ANTES de migrar os managers complexos.
 * 
 * 2. Se todos os testes passarem, você pode prosseguir
 *    para a Fase 2 (migrar SensorManager).
 * 
 * 3. Se algum teste falhar:
 *    - Verifique conexões físicas dos sensores
 *    - Verifique se os pinos I2C estão corretos
 *    - Verifique alimentação dos sensores (3.3V)
 * 
 * 4. Endereços I2C esperados:
 *    MPU9250: 0x68 (padrão) ou 0x69 (AD0=1)
 *    BMP280:  0x76 (padrão) ou 0x77 (SDO=1)
 *    SI7021:  0x40 (fixo)
 *    CCS811:  0x5A (padrão) ou 0x5B (ADDR=1)
 */