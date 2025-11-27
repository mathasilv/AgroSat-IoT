# Hardware Abstraction Layer (HAL) - AgroSat-IoT

## 📋 Visão Geral

Esta camada de abstração de hardware (HAL) permite que o firmware AgroSat-IoT seja portável entre diferentes plataformas (ESP32, STM32, etc.) sem modificar a lógica da aplicação.

## 🏗️ Arquitetura

```
hal/
├── interface/          # Interfaces abstratas (virtual methods)
│   ├── I2C.h          # Comunicação I2C (sensores)
│   ├── SPI.h          # Comunicação SPI (LoRa)
│   ├── GPIO.h         # Controle de pinos
│   └── UART.h         # Comunicação serial
│
├── platform/           # Implementações concretas
│   └── esp32/         # ESP32 implementation
│       ├── ESP32_I2C.h
│       ├── ESP32_SPI.h
│       ├── ESP32_GPIO.h
│       └── ESP32_UART.h
│
└── board/              # Configurações de hardware
    └── ttgo_lora32_v21.h  # Pin mappings TTGO LoRa32 V2.1
```

## 🚀 Como Usar

### 1. Exemplo Básico - I2C com Sensores

```cpp
#include "hal/platform/esp32/ESP32_I2C.h"
#include "hal/board/ttgo_lora32_v21.h"

// Criar instância do HAL
HAL::ESP32_I2C* i2c = new HAL::ESP32_I2C(&Wire);

void setup() {
    // Inicializar I2C com pinos da board
    i2c->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_FREQUENCY);
    
    // Wake up MPU9250
    i2c->writeRegister(0x68, 0x6B, 0x00);
    
    // Ler WHO_AM_I register
    uint8_t whoami = i2c->readRegister(0x68, 0x75);
    Serial.printf("MPU9250 ID: 0x%02X\n", whoami);
}
```

### 2. Dependency Injection em Managers

**Antes (código acoplado ao ESP32):**
```cpp
class SensorManager {
private:
    MPU9250_WE mpu;  // Biblioteca específica
    
public:
    void init() {
        Wire.begin(21, 22);  // Hardcoded ESP32 pins!
        mpu.init();
    }
};
```

**Depois (código portável com HAL):**
```cpp
class SensorManager {
private:
    HAL::I2C* i2c_hal;  // Interface abstrata
    
public:
    // Dependency injection via construtor
    SensorManager(HAL::I2C* hal) : i2c_hal(hal) {}
    
    void initMPU9250() {
        // Usa interface genérica - PORTÁVEL!
        i2c_hal->writeRegister(0x68, 0x6B, 0x00);
    }
};
```

**No main.cpp:**
```cpp
#include "hal/platform/esp32/ESP32_I2C.h"
#include "SensorManager.h"

void setup() {
    // Criar HAL concreto
    HAL::ESP32_I2C* i2c = new HAL::ESP32_I2C(&Wire);
    i2c->begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 400000);
    
    // Injetar HAL no manager
    SensorManager* sensors = new SensorManager(i2c);
    sensors->initMPU9250();
}
```

### 3. Exemplo LoRa com SPI

```cpp
#include "hal/platform/esp32/ESP32_SPI.h"
#include "hal/platform/esp32/ESP32_GPIO.h"
#include "hal/board/ttgo_lora32_v21.h"

HAL::ESP32_SPI* spi = new HAL::ESP32_SPI(&SPI);
HAL::ESP32_GPIO* gpio = new HAL::ESP32_GPIO();

void setup() {
    // Setup CS pin
    gpio->pinMode(BOARD_LORA_CS, HAL::PinMode::OUTPUT);
    gpio->digitalWrite(BOARD_LORA_CS, HAL::PinState::HIGH);
    
    // Inicializar SPI
    spi->begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI, BOARD_SPI_FREQUENCY);
    
    // Ler LoRa version register
    spi->beginTransaction(BOARD_LORA_CS);
    uint8_t version = spi->transfer(0x42);  // Read register 0x42
    spi->endTransaction(BOARD_LORA_CS);
    
    Serial.printf("LoRa Version: 0x%02X\n", version);
}
```

## 🔄 Migrando Managers Existentes

### Passo 1: Modificar Header
```cpp
// include/SensorManager.h
#include "hal/interface/I2C.h"  // +++ Adicionar

class SensorManager {
private:
    HAL::I2C* i2c_hal;  // +++ Adicionar
    
public:
    SensorManager(HAL::I2C* hal);  // +++ Modificar construtor
    // ...
};
```

### Passo 2: Modificar Implementação
```cpp
// src/SensorManager.cpp
SensorManager::SensorManager(HAL::I2C* hal) : i2c_hal(hal) {}

void SensorManager::initMPU9250() {
    // Trocar Wire.write() por i2c_hal->writeRegister()
    i2c_hal->writeRegister(0x68, 0x6B, 0x00);
}
```

### Passo 3: Atualizar main.cpp
```cpp
#include "hal/platform/esp32/ESP32_I2C.h"

void setup() {
    // Criar HAL
    auto i2c = new HAL::ESP32_I2C(&Wire);
    i2c->begin(21, 22, 400000);
    
    // Injetar no manager
    SensorManager sensors(i2c);
}
```

## 🎯 Benefícios

✅ **Portabilidade**: Mesmo código roda em ESP32, STM32, etc.  
✅ **Testabilidade**: Pode criar Mock_I2C para unit tests  
✅ **Manutenção**: Mudanças de hardware não afetam lógica de aplicação  
✅ **Reutilização**: Código limpo e desacoplado  

## 📦 Próximos Passos

1. Migrar `SensorManager` para usar `HAL::I2C`
2. Migrar `CommunicationManager` para usar `HAL::SPI` (LoRa)
3. Criar implementação `STM32_I2C`, `STM32_SPI`
4. Adicionar testes unitários com Mock HAL

## 📚 Referências

- [Panglos - ESP32/STM32 HAL](https://github.com/DaveBerkeley/panglos)
- [Designing HAL in C++](https://blog.mbedded.ninja/programming/languages/c-plus-plus/designing-a-hal-in-cpp/)
- [Bare Naked Embedded - C++ with STM32](https://barenakedembedded.com/how-to-use-cpp-with-stm32-hal/)
