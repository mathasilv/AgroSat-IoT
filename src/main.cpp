/**
 * @file main.cpp
 * @brief Programa principal - Sistema Simplificado
 * @version 6.0.0
 * @date 2025-11-24
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include <HAL/platform/esp32/ESP32_I2C.h>
#include <HAL/board/ttgo_lora32_v21.h>
#include "TelemetryManager.h"
#include "Drivers/SX127x/LoRaCompatibility.h"

// HAL I2C global
HAL::ESP32_I2C halI2C;
HAL::ESP32_SPI halSPI;  

// TelemetryManager agora recebe o HAL I2C
TelemetryManager telemetry(halI2C);

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1000);
    
// Inicializar SPI CONCRETO
    if (!halSPI.begin(18, 19, 23, 8000000)) {  // VSPI @ 8MHz
        Serial.println("FALHA: SPI init!");
        return;
    }
    halSPI.configureCS(LORA_CS);               // ✅ Existe na implementação
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTF("[Main] Botão de controle: GPIO %d\n", BUTTON_PIN);
    DEBUG_PRINTLN("[Main]   - Pressionar: Toggle PREFLIGHT ↔ FLIGHT");
    DEBUG_PRINTLN("[Main]   - Segurar 3s: Ativar SAFE MODE");
    DEBUG_PRINTLN("");
    
    DEBUG_PRINTLN("[Main] Configurando Watchdog Timer (120s)...");
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);

    // Opcional: se quiser garantir o begin aqui, antes do _initI2CBus()
    // halI2C.begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
}

void loop() {
    esp_task_wdt_reset();
    
    telemetry.loop();
    
    delay(10);
}
