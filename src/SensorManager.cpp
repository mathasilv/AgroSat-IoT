/**
 * @file SensorManager.cpp
 * @brief SensorManager v4.2.0 - BMP280 + scanI2C → HAL I2C
 */

// ... [código anterior mantido]

bool SensorManager::_softResetBMP280() {
    DEBUG_PRINTLN("[SensorManager] SOFT RESET BMP280 (HAL I2C)...");
    
    const uint8_t BMP280_RESET_REG = 0xE0;
    const uint8_t BMP280_RESET_CMD = 0xB6;
    
    // ✅ HAL I2C: Escrever 2 bytes (reg + cmd)
    uint8_t resetData[2] = {BMP280_RESET_REG, BMP280_RESET_CMD};
    if (!HAL::i2c().write(BMP280_ADDR_1, resetData, 2)) {
        DEBUG_PRINTLN("[SensorManager] Erro soft reset");
        return false;
    }
    
    DEBUG_PRINTLN("[SensorManager] Soft reset enviado, aguardando...");
    delay(100);
    
    // Verificar resposta
    return HAL::i2c().writeByte(BMP280_ADDR_1, 0x00);
}

bool SensorManager::_waitForBMP280Measurement() {
    const uint8_t BMP280_STATUS_REG = 0xF3;
    const uint8_t STATUS_MEASURING = 0x08;
    
    uint8_t maxRetries = 50;
    
    while (maxRetries--) {
        // ✅ HAL I2C: Ler status register
        uint8_t status;
        if (HAL::i2c().readRegisterByte(BMP280_ADDR_1, BMP280_STATUS_REG, &status)) {
            if ((status & STATUS_MEASURING) == 0) {
                return true;
            }
        }
        delay(1);
    }
    
    return false;
}

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Scanning I2C (HAL I2C)...");
    uint8_t count = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        // ✅ HAL I2C: Teste simples de presença
        if (HAL::i2c().writeByte(addr, 0x00)) {
            DEBUG_PRINTF("  Device at 0x%02X\n", addr);
            count++;
        }
    }
    
    DEBUG_PRINTF("[SensorManager] Found %d devices\n", count);
}

// ... [resto do código mantido igual]