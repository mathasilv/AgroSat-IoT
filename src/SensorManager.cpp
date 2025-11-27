/**
 * @file SensorManager.cpp
 * @brief SensorManager v4.1.0 - SI7021 migrado para HAL I2C
 */

#include "SensorManager.h"
#include <algorithm>
#include "DisplayManager.h"
#include <string.h>

// ... [código anterior mantido até _updateSI7021()]

void SensorManager::_updateSI7021() {
    if (!_si7021Online) return;
    
    uint32_t currentTime = millis();
    if (currentTime - _lastSI7021Read < SI7021_READ_INTERVAL) return;
    
    // ========================================
    // PASSO 1: UMIDADE (0xF5) - HAL I2C ✅
    // ========================================
    if (!HAL::i2c().writeByte(SI7021_ADDRESS, 0xF5)) return;
    
    delay(50);
    
    bool humiditySuccess = false;
    delay(50);  // Total 100ms
    
    uint8_t rawHumData[3];
    if (HAL::i2c().read(SI7021_ADDRESS, rawHumData, 3)) {
        uint16_t rawHum = (rawHumData[0] << 8) | rawHumData[1];
        
        if (rawHum != 0xFFFF && rawHum != 0x0000) {
            float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
            
            if (hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
                _humidity = hum;
                _lastSI7021Read = currentTime;
                humiditySuccess = true;
            }
        }
    }
    
    if (!humiditySuccess) {
        static uint8_t failCount = 0;
        failCount++;
        if (failCount >= 10) {
            DEBUG_PRINTLN("[SensorManager] SI7021: 10 falhas umidade");
            failCount = 0;
        }
        return;
    }
    
    // ========================================
    // PASSO 2: TEMPERATURA (0xF3) - HAL I2C ✅
    // ========================================
    delay(30);
    
    if (!HAL::i2c().writeByte(SI7021_ADDRESS, 0xF3)) return;
    
    delay(80);
    
    uint8_t rawTempData[2];
    if (HAL::i2c().read(SI7021_ADDRESS, rawTempData, 2)) {
        uint16_t rawTemp = (rawTempData[0] << 8) | rawTempData[1];
        
        if (rawTemp != 0xFFFF && rawTemp != 0x0000) {
            float temp = ((175.72 * rawTemp) / 65536.0) - 46.85;
            
            if (_validateReading(temp, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
                _temperatureSI = temp;
                _si7021TempValid = true;
                _si7021TempFailures = 0;
            } else {
                _si7021TempValid = false;
                _si7021TempFailures++;
                
                if (_si7021TempFailures >= MAX_TEMP_FAILURES) {
                    DEBUG_PRINTLN("[SensorManager] SI7021: Temp falhas consecutivas");
                }
            }
        }
    }
}

bool SensorManager::_initSI7021() {
    DEBUG_PRINTLN("[SensorManager] Inicializando SI7021 (HAL I2C)...");
    
    // Verificar presença
    if (!HAL::i2c().writeByte(SI7021_ADDRESS, 0x00)) {
        DEBUG_PRINTLN("[SensorManager] SI7021: Não detectado");
        return false;
    }
    
    DEBUG_PRINTLN("[SensorManager] SI7021: Detectado HAL I2C");
    
    // Software Reset
    HAL::i2c().writeByte(SI7021_ADDRESS, 0xFE);
    delay(50);
    
    // Configurar User Register
    uint8_t configData[2] = {0xE6, 0x00};
    HAL::i2c().write(SI7021_ADDRESS, configData, 2);
    delay(20);
    
    // Testar umidade
    HAL::i2c().writeByte(SI7021_ADDRESS, 0xF5);
    delay(20);
    
    bool success = false;
    uint8_t rawData[3];
    
    for (uint8_t retry = 0; retry < 20; retry++) {
        if (HAL::i2c().read(SI7021_ADDRESS, rawData, 3)) {
            uint16_t rawHum = (rawData[0] << 8) | rawData[1];
            
            if (rawHum != 0xFFFF && rawHum != 0x0000) {
                float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
                
                if (hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
                    DEBUG_PRINTF("[SensorManager] SI7021 OK (%.1f%% RH)\n", hum);
                    success = true;
                    break;
                }
            }
        }
        delay(10);
    }
    
    return success;
}

// ... [resto do código mantido igual]