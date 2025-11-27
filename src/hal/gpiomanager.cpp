#include "../include/hal/gpiomanager.h"

GPIOManager& GPIOManager::getInstance() {
    if (!instance) {
        instance = new GPIOManager();
#ifdef CONFIG_FREERTOS_UNICORE
        instance->mutex = xSemaphoreCreateMutex();
#endif
    }
    return *instance;
}

bool GPIOManager::init() {
    Serial.println("[GPIO] Manager initialized");
    return true;
}

void GPIOManager::setPinMode(uint8_t pin, uint8_t mode) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
#endif
    pinMode(pin, mode);
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
}

bool GPIOManager::digitalRead(uint8_t pin) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
#endif
    bool value = digitalRead(pin);
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
    return value;
}

void GPIOManager::digitalWrite(uint8_t pin, bool value) {
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
#endif
    digitalWrite(pin, value);
#ifdef CONFIG_FREERTOS_UNICORE
    if (mutex) xSemaphoreGive(mutex);
#endif
}

bool GPIOManager::debounceRead(uint8_t pin, uint32_t debounce_ms) {
    bool last = digitalRead(pin);
    uint32_t last_time = millis();
    
    while (millis() - last_time < debounce_ms) {
        if (digitalRead(pin) != last) {
            last_time = millis();
            last = digitalRead(pin);
        }
        delay(1);
    }
    return last;
}

void GPIOManager::attachInterrupt(uint8_t pin, void (*ISR)(void), int mode) {
    attachInterrupt(digitalPinToInterrupt(pin), ISR, mode);
}

void GPIOManager::detachInterrupt(uint8_t pin) {
    detachInterrupt(digitalPinToInterrupt(pin));
}