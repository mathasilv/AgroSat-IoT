#include "../include/hal/i2cmanager.h"
#include "../include/config.h"
#include "Arduino.h"

bool I2CManager::init(uint8_t sda, uint8_t scl, uint32_t freq) {
    if (initialized) return true; // Já inicializado
    
    wire = &Wire;
    wire->begin(sda, scl);
    wire->setClock(freq);
    delay(10); // Estabilização
    
    initialized = true;
    Serial.println("[I2C] Initialized successfully (SDA:%d SCL:%d %ldHz)", sda, scl, freq);
    return true;
}

bool I2CManager::write(uint8_t addr, uint8_t reg, uint8_t data) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
#endif
    
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->write(data);
    uint8_t error = wire->endTransmission();
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    
    return error == 0;
}

bool I2CManager::write(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
#endif
    
    wire->beginTransmission(addr);
    wire->write(reg);
    for (size_t i = 0; i < len; i++) {
        wire->write(data[i]);
    }
    uint8_t error = wire->endTransmission();
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    
    return error == 0;
}

bool I2CManager::read(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
#endif
    
    // Escrever endereço do registro
    wire->beginTransmission(addr);
    wire->write(reg);
    uint8_t error = wire->endTransmission(false); // Repeated start
    if (error != 0) {
#ifdef CONFIG_FREERTOS_UNICORE
        if (mutex) xSemaphoreGive(mutex);
#endif
        return false;
    }
    
    // Solicitar dados
    uint8_t received = wire->requestFrom(addr, (uint8_t)len);
    if (received != len) {
#ifdef CONFIG_FREERTOS_UNICORE
        if (mutex) xSemaphoreGive(mutex);
#endif
        return false;
    }
    
    // Ler bytes
    for (size_t i = 0; i < len; i++) {
        data[i] = wire->read();
    }
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    return true;
}

bool I2CManager::probe(uint8_t addr) {
    wire->beginTransmission(addr);
    return wire->endTransmission() == 0;
}

void I2CManager::scan() {
    Serial.println("[I2C] Scanning bus...");
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (probe(addr)) {
            Serial.printf("  Device found at 0x%02X\n", addr);
            count++;
        }
    }
    Serial.printf("[I2C] Scan complete: %d devices found\n", count);
}

void I2CManager::reset() {
    initialized = false;
    if (wire) {
        wire->end();
        delay(100);
    }
}
