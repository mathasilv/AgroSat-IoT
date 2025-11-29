/**
 * @file main.cpp
 * @brief Programa principal - Sistema Simplificado
 * @version 6.0.0
 * @date 2025-11-24
 */
#include <Arduino.h>

#include <HAL/platform/esp32/ESP32_I2C.h>
#include <HAL/platform/esp32/ESP32_SPI.h>
#include <HAL/board/ttgo_lora32_v21.h>

#include "TelemetryManager.h"
#include "SensorManager.h"
#include "CommunicationManager.h"
#include "DisplayManager.h"
#include "PowerManager.h"
#include "SystemHealth.h"


// HAL I2C global
HAL::ESP32_I2C halI2C;
HAL::ESP32_SPI halSPI;  

// TelemetryManager agora recebe o HAL I2C
TelemetryManager telemetry(halI2C);

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1000);
    
    DEBUG_PRINTLN("[Main] Inicializando HAL...");
    
    // HAL para sensores I2C
    if (!halI2C.begin(21, 22, 400000)) {
        DEBUG_PRINTLN("FALHA: I2C!");
        while(1);
    }
    
    // HAL para LoRa SPI
    if (!halSPI.begin(5, 19, 27, 8000000UL)) {
        DEBUG_PRINTLN("FALHA: SPI!");
        while(1);
    }
    halSPI.configureCS(LORA_CS);
    
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
    
    if (!telemetry.begin()) {  // SEM parâmetros!
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
