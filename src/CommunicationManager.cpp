/**
 * @file CommunicationManager.cpp
 * @brief CommunicationManager v7.1.0 - HAL SPI + GPIO (LoRa)
 * @version 7.1.0
 */

#include "CommunicationManager.h"
#include "hal/hal.h"

// ... [código anterior mantido até initLoRa()]

bool CommunicationManager::initLoRa() {
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━ INICIALIZANDO LORA (HAL SPI) ━━━━━");
    DEBUG_PRINTLN("[CommunicationManager] Board: TTGO LoRa32 V2.1 (T3 V1.6)");
    
    DEBUG_PRINTF("[CommunicationManager] Pinos SPI + GPIO:\n");
    DEBUG_PRINTF("[CommunicationManager]   SCK  = %d\n", LORA_SCK);
    DEBUG_PRINTF("[CommunicationManager]   MISO = %d\n", LORA_MISO);
    DEBUG_PRINTF("[CommunicationManager]   MOSI = %d\n", LORA_MOSI);
    DEBUG_PRINTF("[CommunicationManager]   CS   = %d\n", LORA_CS);
    DEBUG_PRINTF("[CommunicationManager]   RST  = %d\n", LORA_RST);
    DEBUG_PRINTF("[CommunicationManager]   DIO0 = %d\n", LORA_DIO0);
    
    // ✅ HAL GPIO: Reset LoRa
    HAL::gpio().pinMode(LORA_RST, OUTPUT);
    HAL::gpio().digitalWrite(LORA_RST, LOW);
    delay(10);
    HAL::gpio().digitalWrite(LORA_RST, HIGH);
    delay(100);
    DEBUG_PRINTLN("[CommunicationManager] Módulo LoRa resetado (HAL GPIO)");
    
    // ✅ HAL SPI: Inicializar SPI LoRa
    HAL::spi().begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[CommunicationManager] HAL SPI inicializado (LoRa)");
    
    // ✅ LoRa library com HAL pins
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    DEBUG_PRINTLN("[CommunicationManager] Pinos LoRa configurados (HAL)");
    
    DEBUG_PRINTF("[CommunicationManager] Tentando LoRa.begin(%.1f MHz)... ", LORA_FREQUENCY / 1E6);
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        DEBUG_PRINTLN("[CommunicationManager] Erro: Chip LoRa não respondeu");
        _loraInitialized = false;
        return false;
    }
    DEBUG_PRINTLN("OK!");
    
    // Configuração LoRa mantida igual...
    _configureLoRaParameters();
    
    // ... [resto da função mantido igual]
    
    _loraInitialized = true;
    
    DEBUG_PRINT("[CommunicationManager] Enviando pacote de teste... ");
    String testMsg = "AGROSAT_BOOT_v7.1.0_HAL_SPI";
    if (sendLoRa(testMsg)) {
        DEBUG_PRINTLN("OK!");
    } else {
        DEBUG_PRINTLN("FALHOU (mas LoRa está configurado)");
    }
    
    LoRa.receive();
    DEBUG_PRINTLN("[CommunicationManager] LoRa em modo RX (HAL SPI + GPIO)");
    DEBUG_PRINTLN("[CommunicationManager] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    return true;
}

// ... [resto do código mantido igual]