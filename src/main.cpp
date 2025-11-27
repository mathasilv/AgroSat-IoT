#include <Arduino.h>
#include <HAL/platform/esp32/ESP32_GPIO.h>
#include <HAL/board/ttgo_lora32_v21.h>

// GPIO HAL global
HAL::ESP32_GPIO gpio;

// Controle de blink
unsigned long lastBlink = 0;
bool ledState = false;

// Controle de debug periódico
unsigned long lastReport = 0;

// ============================================================================
// Leitura de bateria (ADC)
// ============================================================================

float readBatteryVoltage(uint16_t* rawOut = nullptr) {
    analogSetPinAttenuation(BOARD_BATTERY_ADC, ADC_11db);

    uint16_t raw = analogRead(BOARD_BATTERY_ADC);  // 0..4095
    if (rawOut) {
        *rawOut = raw;
    }

    float vAdc = (raw / 4095.0f) * 3.3f;

    constexpr float BATTERY_DIVIDER = 2.0f; // ajuste conforme seu divisor real
    float vBat = vAdc * BATTERY_DIVIDER;

    return vBat;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    delay(500);

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  AgroSat-IoT - HAL GPIO + Battery ADC"));
    Serial.println(F("========================================"));
    Serial.printf("BOARD_NAME       = %s\n", BOARD_NAME);
    Serial.printf("LED_PIN          = %d\n", BOARD_LED_PIN);
    Serial.printf("BUTTON_PIN       = %d\n", BOARD_BUTTON_PIN);
    Serial.printf("BATTERY_ADC_PIN  = %d\n", BOARD_BATTERY_ADC);
    Serial.println();

    // Configurar LED como saída via HAL
    gpio.pinMode(BOARD_LED_PIN, HAL::PinMode::Output);
    gpio.digitalWrite(BOARD_LED_PIN, HAL::PinState::Low);

    // Botão BOOT geralmente ligado ao GND, com pull-up interno
    gpio.pinMode(BOARD_BUTTON_PIN, HAL::PinMode::InputPullup);

    // ADC: apenas garante que é entrada analógica
    pinMode(BOARD_BATTERY_ADC, INPUT);
}

void loop() {
    unsigned long now = millis();

    // Blink LED via HAL GPIO a cada 500 ms
    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledState = !ledState;
        gpio.digitalWrite(BOARD_LED_PIN, ledState ? HAL::PinState::High : HAL::PinState::Low);
    }

    // A cada 2 segundos, reportar estado do botão + bateria
    if (now - lastReport >= 2000) {
        lastReport = now;

        HAL::PinState btn = gpio.digitalRead(BOARD_BUTTON_PIN);
        bool pressed = (btn == HAL::PinState::Low);

        uint16_t raw = 0;
        float vBat = readBatteryVoltage(&raw);

        Serial.println(F("---- Status ----"));
        Serial.printf("Button (BOOT) pressed: %s\n", pressed ? "YES" : "no");
        Serial.printf("Battery ADC raw      : %u / 4095\n", raw);
        Serial.printf("Battery voltage est. : %.2f V\n", vBat);
        Serial.println();
    }
}
