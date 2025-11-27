#pragma once

/**
 * @file hal.h
 * @brief HAL Principal - Hardware Abstraction Layer AgroSat-IoT
 * @version 1.0.0
 */

#include "hal/i2c_manager.h"
#include "hal/spi_manager.h"
#include "hal/gpio_manager.h"
#include "hal/adc_manager.h"

/**
 * @brief Namespace HAL para fácil acesso aos managers
 */
namespace HAL {
    inline auto& i2c() { return I2CManager::getInstance(); }
    inline auto& spi() { return SPIManager::getInstance(); }
    inline auto& gpio() { return GPIOManager::getInstance(); }
    inline auto& adc() { return ADCHelper::getInstance(); }
}

/**
 * @brief Inicializar todos os HALs
 * @return true se todos inicializaram com sucesso
 */
inline bool halBegin() {
    return HAL::i2c().begin() &&
           HAL::spi().begin() &&
           HAL::gpio().begin();
}

/**
 * @brief Finalizar todos os HALs
 */
inline void halEnd() {
    HAL::i2c().end();
    HAL::spi().end();
    HAL::gpio().end();
}
