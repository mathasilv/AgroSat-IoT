#include "../include/hal/i2cmanager.h"
#include "../include/config.h"

I2CManager::I2CManager() {
#ifdef CONFIG_FREERTOS_UNICORE
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        Serial.println("[I2C] ERROR: Mutex creation failed");
    }
#endif
}

I2CManager::~I2CManager() {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
#endif
    if (wire) {
        wire->end();
    }
}

I2CManager& I2CManager::getInstance() {
    if (!instance) {
        instance = new I2CManager();
    }
    return *instance;
}

bool I2CManager::init(uint8_t sda, uint8_t scl, uint32_t freq) {
    if (initialized) return true;
    
    wire = &Wire;
    wire->begin(sda, scl);
    wire->setClock(freq);
    delay(10);
    
    initialized = true;
    Serial.printf("[I2C] Initialized (SDA:%d SCL:%d %ldHz)\n", sda, scl, freq);
    return true;
}

bool I2CManager::write(uint8_t addr, uint8_t reg, uint8_t data) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        Serial.println("[I2C] Mutex timeout");
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
    
    wire->beginTransmission(addr);
    wire->write(reg);
    uint8_t error = wire->endTransmission(false);
    if (error != 0) {
#ifdef CONFIG_FREERTOS_UNICORE
        if (mutex) xSemaphoreGive(mutex);
#endif
        return false;
    }
    
    uint8_t received = wire->requestFrom(addr, (uint8_t)len);
    if (received != len) {
#ifdef CONFIG_FREERTOS_UNICORE
        if (mutex) xSemaphoreGive(mutex);
#endif
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        data[i] = wire->read();
    }
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    return true;
}

bool I2CManager::probe(uint8_t addr) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
#endif
    
    wire->beginTransmission(addr);
    uint8_t result = wire->endTransmission();
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    
    return result == 0;
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