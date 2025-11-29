/**
 * @file LoRaCompatibility.cpp
 * @brief Global singleton initialization - CORRIGIDO
 */

#include "LoRaCompatibility.h"
#include "lib/HAL/src/HAL/platform/esp32/ESP32_SPI.h"  // Caminho correto para HAL

// ✅ INSTÂNCIA CONCRETA (não abstrata!)
HAL::ESP32_SPI halSPI_global;

// Instância global SX127x (passa ponteiro da implementação concreta)
SX127x::Radio SX127x::LoRa(&halSPI_global);

// Wrapper global LoRaClass
LoRaClass LoRa;
