# 🚀 Guia de Migração para HAL - AgroSat-IoT

## 🎯 Objetivo

Migrar o firmware AgroSat-IoT da arquitetura acoplada ao ESP32 para uma arquitetura portável usando HAL (Hardware Abstraction Layer).

## 📋 Plano de Migração (Incremental)

### Fase 1: Testes Iniciais (Você está AQUI) ✅

**Objetivo**: Validar que o HAL funciona isoladamente

#### Passo 1.1: Testar HAL I2C Isolado

```cpp
// Criar arquivo: test/test_hal_i2c.cpp
// Ou adicionar no main.cpp temporariamente

#include <Arduino.h>
#include "hal/platform/esp32/ESP32_I2C.h"
#include "hal/board/ttgo_lora32_v21.h"

HAL::ESP32_I2C* i2c_bus = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== HAL I2C Test ===");
    
    // 1. Criar HAL I2C
    i2c_bus = new HAL::ESP32_I2C(&Wire);
    
    // 2. Inicializar com pinos da board
    if (i2c_bus->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY)) {
        Serial.println("✅ HAL I2C initialized");
    }
    
    // 3. Scan I2C bus
    Serial.println("\nScanning I2C bus...");
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_bus->write(addr, nullptr, 0)) {
            Serial.printf("  Found device at 0x%02X\n", addr);
            count++;
        }
    }
    Serial.printf("Found %d devices\n", count);
    
    // 4. Testar MPU9250
    Serial.println("\nTesting MPU9250...");
    
    // Wake up
    if (i2c_bus->writeRegister(0x68, 0x6B, 0x00)) {
        Serial.println("  ✅ MPU9250 wake up OK");
        delay(100);
        
        // Read WHO_AM_I
        uint8_t whoami = i2c_bus->readRegister(0x68, 0x75);
        Serial.printf("  WHO_AM_I: 0x%02X (expected 0x71)\n", whoami);
        
        if (whoami == 0x71) {
            Serial.println("  ✅ MPU9250 detected!");
        }
    }
}

void loop() {
    // Read temperature
    uint8_t temp_data[2];
    i2c_bus->write(0x68, (const uint8_t[]){0x41}, 1);
    
    if (i2c_bus->read(0x68, temp_data, 2)) {
        int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
        float temp_c = temp_raw / 340.0 + 36.53;
        Serial.printf("[%lu] MPU Temp: %.2f°C\n", millis(), temp_c);
    }
    
    delay(1000);
}
```

**Resultado esperado**:
- I2C scan deve encontrar seus sensores (0x68, 0x76, 0x40, etc.)
- MPU9250 WHO_AM_I = 0x71
- Temperatura do MPU deve variar ~30-40°C

#### Passo 1.2: Compilar e Testar

```bash
# No terminal:
git checkout feature/hal-implementation
pio run -t upload -t monitor
```

**Se funcionar**: Prossiga para Fase 2  
**Se falhar**: Revise conexões I2C e endereços

---

### Fase 2: Migrar SensorManager (Parcial)

**Objetivo**: Refatorar apenas os métodos de I2C raw do SensorManager

#### Passo 2.1: Modificar Header

```cpp
// include/SensorManager.h

#include "hal/interface/I2C.h"  // +++ ADICIONAR

class SensorManager {
public:
    // +++ MODIFICAR construtor
    explicit SensorManager(HAL::I2C* i2c_hal);
    
    // Resto permanece igual...
    
private:
    HAL::I2C* _i2c_hal;  // +++ ADICIONAR
    
    // Objetos de hardware (manter por enquanto)
    MPU9250_WE _mpu9250;
    Adafruit_BMP280 _bmp280;
    Adafruit_CCS811 _ccs811;
    
    // Resto permanece igual...
};
```

#### Passo 2.2: Modificar Construtor

```cpp
// src/SensorManager.cpp

SensorManager::SensorManager(HAL::I2C* i2c_hal)
    : _i2c_hal(i2c_hal),  // +++ Injetar HAL
      _mpu9250(0x68),
      _seaLevelPressure(1013.25),
      // ... resto igual
{
    // Corpo do construtor permanece igual
}
```

#### Passo 2.3: Migrar scanI2C() (método simples)

```cpp
// src/SensorManager.cpp

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorMgr] Scanning I2C bus...");
    
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        // ANTES:
        // Wire.beginTransmission(address);
        // error = Wire.endTransmission();
        
        // DEPOIS:
        if (_i2c_hal->write(address, nullptr, 0)) {
            DEBUG_PRINTF("Device at 0x%02X\n", address);
            deviceCount++;
        }
    }
    
    DEBUG_PRINTF("Found %d devices\n", deviceCount);
}
```

#### Passo 2.4: Migrar _initSI7021() (exemplo completo fornecido)

Copie do arquivo `src/SensorManager_HAL_Example.cpp` o método `_initSI7021()`.

#### Passo 2.5: Atualizar main.cpp

```cpp
// src/main.cpp

#include <Arduino.h>
#include "hal/platform/esp32/ESP32_I2C.h"  // +++ ADICIONAR
#include "hal/board/ttgo_lora32_v21.h"    // +++ ADICIONAR
#include "TelemetryManager.h"

// +++ CRIAR HAL GLOBAL
HAL::ESP32_I2C* i2c_hal = nullptr;

TelemetryManager telemetry;

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1000);
    
    // +++ INICIALIZAR HAL I2C ANTES DE TUDO
    Serial.println("[Main] Initializing HAL I2C...");
    i2c_hal = new HAL::ESP32_I2C(&Wire);
    i2c_hal->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // ... resto do setup
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO!");
    }
}
```

#### Passo 2.6: Atualizar TelemetryManager

```cpp
// include/TelemetryManager.h
#include "hal/interface/I2C.h"

class TelemetryManager {
public:
    // +++ Modificar construtor
    explicit TelemetryManager(HAL::I2C* i2c_hal);
    
private:
    HAL::I2C* _i2c_hal;  // +++ Adicionar
    // ...
};

// src/TelemetryManager.cpp
TelemetryManager::TelemetryManager(HAL::I2C* i2c_hal)
    : _i2c_hal(i2c_hal),
      _sensors(i2c_hal),  // +++ Passar HAL para SensorManager
      // ...
{
    // ...
}

// src/main.cpp
void setup() {
    // ...
    i2c_hal = new HAL::ESP32_I2C(&Wire);
    i2c_hal->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    // +++ Injetar HAL no telemetry
    TelemetryManager telemetry(i2c_hal);
    
    if (!telemetry.begin()) {
        // ...
    }
}
```

---

### Fase 3: Migrar CommunicationManager (LoRa SPI)

#### Passo 3.1: Preparar HAL SPI

```cpp
// src/main.cpp

HAL::ESP32_I2C* i2c_hal = nullptr;
HAL::ESP32_SPI* spi_hal = nullptr;  // +++ ADICIONAR
HAL::ESP32_GPIO* gpio_hal = nullptr;  // +++ ADICIONAR

void setup() {
    // I2C
    i2c_hal = new HAL::ESP32_I2C(&Wire);
    i2c_hal->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    // +++ SPI para LoRa
    spi_hal = new HAL::ESP32_SPI(&SPI);
    spi_hal->begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI, BOARD_SPI_FREQUENCY);
    
    // +++ GPIO para CS, RST, DIO0
    gpio_hal = new HAL::ESP32_GPIO();
    gpio_hal->pinMode(BOARD_LORA_CS, HAL::PinMode::OUTPUT);
    gpio_hal->digitalWrite(BOARD_LORA_CS, HAL::PinState::HIGH);
    
    // Passar HALs para managers
    TelemetryManager telemetry(i2c_hal, spi_hal, gpio_hal);
}
```

#### Passo 3.2: Modificar CommunicationManager

```cpp
// include/CommunicationManager.h

#include "hal/interface/SPI.h"
#include "hal/interface/GPIO.h"

class CommunicationManager {
public:
    CommunicationManager(HAL::SPI* spi, HAL::GPIO* gpio, 
                         uint8_t cs_pin, uint8_t rst_pin, uint8_t dio0_pin);
                         
private:
    HAL::SPI* _spi_hal;
    HAL::GPIO* _gpio_hal;
    uint8_t _cs_pin, _rst_pin, _dio0_pin;
    
    void _loraWriteRegister(uint8_t reg, uint8_t value);
    uint8_t _loraReadRegister(uint8_t reg);
};

// src/CommunicationManager.cpp

void CommunicationManager::_loraWriteRegister(uint8_t reg, uint8_t value) {
    _spi_hal->beginTransaction(_cs_pin);
    _spi_hal->transfer(reg | 0x80);  // Write bit
    _spi_hal->transfer(value);
    _spi_hal->endTransaction(_cs_pin);
}

uint8_t CommunicationManager::_loraReadRegister(uint8_t reg) {
    _spi_hal->beginTransaction(_cs_pin);
    _spi_hal->transfer(reg & 0x7F);  // Read bit
    uint8_t value = _spi_hal->transfer(0x00);
    _spi_hal->endTransaction(_cs_pin);
    return value;
}
```

---

### Fase 4: Preparar para STM32

#### Passo 4.1: Criar implementação STM32 (exemplo)

```cpp
// hal/platform/stm32/STM32_I2C.h

#ifndef STM32_I2C_H
#define STM32_I2C_H

#include "hal/interface/I2C.h"
#include <Wire.h>  // STM32duino Wire library

namespace HAL {

class STM32_I2C : public I2C {
private:
    TwoWire* wire;
    
public:
    explicit STM32_I2C(TwoWire* wireInstance = &Wire)
        : wire(wireInstance) {}
    
    bool begin(uint8_t sda, uint8_t scl, uint32_t frequency) override {
        wire->setSDA(sda);
        wire->setSCL(scl);
        wire->setClock(frequency);
        wire->begin();
        return true;
    }
    
    // Implementações idênticas ao ESP32_I2C...
};

} // namespace HAL

#endif
```

#### Passo 4.2: Atualizar platformio.ini

```ini
[platformio]
default_envs = ttgo-lora32-v21

[common]
lib_deps = 
    wollewald/MPU9250_WE @ ^1.2.8
    adafruit/Adafruit BMP280 Library @ ^2.6.6
    sandeepmistry/LoRa @ ^0.8.0

[env:ttgo-lora32-v21]
platform = espressif32
board = ttgo-lora32-v21
framework = arduino
build_flags = 
    -DHAL_ESP32
    -DBOARD_TTGO_LORA32_V21
lib_deps = ${common.lib_deps}

[env:nucleo-l432]  # +++ NOVA PLATAFORMA
platform = ststm32
board = nucleo_l432kc
framework = arduino
build_flags = 
    -DHAL_STM32
    -DBOARD_NUCLEO_L432
lib_deps = ${common.lib_deps}
```

#### Passo 4.3: Condicional de compilação

```cpp
// src/main.cpp

#ifdef HAL_ESP32
    #include "hal/platform/esp32/ESP32_I2C.h"
    #include "hal/platform/esp32/ESP32_SPI.h"
#elif defined(HAL_STM32)
    #include "hal/platform/stm32/STM32_I2C.h"
    #include "hal/platform/stm32/STM32_SPI.h"
#endif

void setup() {
    #ifdef HAL_ESP32
        auto i2c = new HAL::ESP32_I2C(&Wire);
    #elif defined(HAL_STM32)
        auto i2c = new HAL::STM32_I2C(&Wire);
    #endif
    
    i2c->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 400000);
    
    // Código da aplicação é PORTÁVEL!
    TelemetryManager telemetry(i2c);
}
```

---

## ✅ Checklist de Migração

### Fase 1: Testes
- [ ] HAL I2C compila sem erros
- [ ] I2C scan detecta sensores (0x68, 0x76, 0x40)
- [ ] MPU9250 WHO_AM_I retorna 0x71
- [ ] Leitura de temperatura MPU funciona

### Fase 2: SensorManager
- [ ] Header modificado com `HAL::I2C* _i2c_hal`
- [ ] Construtor aceita dependency injection
- [ ] `scanI2C()` migrado para HAL
- [ ] `_initSI7021()` migrado para HAL
- [ ] `_updateSI7021()` migrado para HAL
- [ ] TelemetryManager injeta HAL no SensorManager
- [ ] Firmware compila e funciona igual ao original

### Fase 3: CommunicationManager
- [ ] LoRa SPI migrado para `HAL::SPI`
- [ ] GPIO LoRa (CS, RST, DIO0) usa `HAL::GPIO`
- [ ] Transmissão/recepção LoRa funcionando

### Fase 4: Portabilidade
- [ ] `STM32_I2C.h` implementado
- [ ] `platformio.ini` com múltiplos environments
- [ ] Compila para ESP32 sem erros
- [ ] Compila para STM32 sem erros (se hardware disponível)

---

## 🐛 Troubleshooting

### Erro: `undefined reference to vtable`
**Causa**: Método virtual não implementado  
**Solução**: Verifique se todos os métodos da interface estão implementados

### Sensores não detectados após migração
**Causa**: I2C não inicializado antes de criar managers  
**Solução**: Chame `i2c_hal->begin()` ANTES de `telemetry.begin()`

### Leituras I2C retornam 0x00 ou 0xFF
**Causa**: Endereço I2C incorreto ou sensor não conectado  
**Solução**: Execute `scanI2C()` para verificar endereços reais

---

## 📚 Próximos Passos

1. **Agora**: Teste HAL I2C isolado (Fase 1)
2. **Depois**: Migre SensorManager gradualmente (Fase 2)
3. **Por último**: Adicione suporte STM32 (Fase 4)

Boa sorte! 🚀
