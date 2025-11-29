/**
 * @file LoRaCompatibility.cpp
 * @brief Global singleton initialization - CORRIGIDO
 */

#include "LoRaCompatibility.h"
#include <HAL/platform/esp32/ESP32_SPI.h>   // usa HAL com -Ilib

// ✅ INSTÂNCIA CONCRETA (não abstrata!)
HAL::ESP32_SPI halSPI_global;

// Instância global SX127x (passa ponteiro da implementação concreta)
SX127x::Radio SX127x::LoRa(&halSPI_global);

// Wrapper global LoRaClass
LoRaClass LoRa;
