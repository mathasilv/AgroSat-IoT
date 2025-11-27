#include "../include/hal/spimanager.h"
#include "../include/config.h"

SPIManager& SPIManager::getInstance() {
    if (!instance) {
        instance = new SPIManager();
    }
    return *instance;
}

bool SPIManager::init(int8_t mosi, int8_t miso, int8_t sck, int8_t freq) {
    if (initialized) return true;
    
    spi = new SPIClass(HSPI);
    spi->begin(sck, miso, mosi, -1);
    spi->setFrequency(freq);
    
#ifdef CONFIG_FREERTOS_UNICORE
    mutex = xSemaphoreCreateMutex();
#endif
    
    initialized = true;
    Serial.printf("[SPI] Initialized (MOSI:%d MISO:%d SCK:%d %dHz)\n", mosi, miso, sck, freq);
    return true;
}

bool SPIManager::transfer(uint8_t cs_pin, uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
    if (!tx_buf || !rx_buf || len == 0) return false;
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(SPI_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }
#endif
    
    digitalWrite(cs_pin, LOW);
    spi->transferBytes(tx_buf, rx_buf, len);
    digitalWrite(cs_pin, HIGH);
    
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    
    return true;
}

bool SPIManager::transfer(uint8_t cs_pin, const uint8_t* tx_buf, size_t len) {
    if (!tx_buf || len == 0) return false;
    
    uint8_t dummy[len];
    return transfer(cs_pin, (uint8_t*)tx_buf, dummy, len);
}

void SPIManager::setFrequency(uint32_t freq) {
    if (spi) spi->setFrequency(freq);
}

void SPIManager::setChipSelect(uint8_t cs_pin, bool state) {
    digitalWrite(cs_pin, state ? HIGH : LOW);
}