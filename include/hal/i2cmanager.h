#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

#ifdef CONFIG_FREERTOS_UNICORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * @brief Gerenciador singleton para I2C
 * @details Thread-safe, inicialização lazy, timeout handling
 */
class I2CManager {
private:
    // Singleton
    static I2CManager* instance;
    I2CManager() = default;
    
    // Estado interno
    bool initialized = false;
    TwoWire* wire = nullptr;
    static constexpr uint32_t I2C_TIMEOUT_MS = 100;
    
#ifdef CONFIG_FREERTOS_UNICORE
    SemaphoreHandle_t mutex = nullptr;
#endif
    
public:
    // Acesso ao singleton
    static I2CManager& getInstance() {
        if (!instance) {
            instance = new I2CManager();
        }
        return *instance;
    }
    
    // Delete copy/move constructors
    I2CManager(const I2CManager&) = delete;
    I2CManager& operator=(const I2CManager&) = delete;
    I2CManager(I2CManager&&) = delete;
    I2CManager& operator=(I2CManager&&) = delete;
    
    /**
     * @brief Inicializa I2C com pinos customizáveis
     * @param sda Pino SDA (default GPIO21)
     * @param scl Pino SCL (default GPIO22)
     * @param freq Frequência (default 400kHz)
     * @return true se sucesso
     */
    bool init(uint8_t sda = 21, uint8_t scl = 22, uint32_t freq = 400000);
    
    /**
     * @brief Escreve registro único
     */
    bool write(uint8_t addr, uint8_t reg, uint8_t data);
    
    /**
     * @brief Escreve múltiplos bytes
     */
    bool write(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len);
    
    /**
     * @brief Lê múltiplos bytes de registro
     */
    bool read(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
    
    /**
     * @brief Verifica presença de dispositivo
     */
    bool probe(uint8_t addr);
    
    /**
     * @brief Escaneia barramento I2C
     */
    void scan();
    
    /**
     * @brief Reset do barramento
     */
    void reset();
    
    /**
     * @brief Estado de inicialização
     */
    bool isInitialized() const { return initialized; }
};

// Inicialização singleton
inline I2CManager* I2CManager::instance = nullptr;
