## Visão Geral da API

O driver SX127x fornece uma interface moderna e completa para comunicação LoRa em sistemas embarcados. Projetado para aplicações espaciais (CubeSat) com foco em confiabilidade, baixo consumo de energia e ausência de alocação dinâmica de memória.

### Características Principais
```
• API moderna C++11/14 (RAII, zero overhead)
• Thread-safe (mutex SPI)
• Suporte completo SF6-SF12, BW 7.8kHz-500kHz
• Modos: Sleep, Standby, TX, RX contínuo/single, CAD
• Compatível Arduino + ESP-IDF + PlatformIO
• Zero dependências externas (exceto HAL::SPI)
• Cache de registros opcional (otimização energia)
• Códigos de erro detalhados (enum RadioError)
```

## Estrutura de Arquivos

```
lib/Drivers/SX127x/
├── SX127x_registers.h     Mapa completo de registros (constexpr)
├── SX127x_hal_adapter.h   Abstração SPI/GPIO (header-only)
├── SX127x.h              Interface principal da classe Radio
├── SX127x.cpp            Implementação completa
└── README.md             Documentação + exemplos
```

## Uso Básico

### Inicialização

```cpp
#include <HAL/interface/SPI.h>
#include "SX127x.h"

HAL::SPI halSPI;
SX127x::Radio radio(&halSPI);

void setup() {
    halSPI.configureCS(LORA_CS);           // Configurar CS
    radio.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    if (!radio.begin(915000000)) {         // 915 MHz
        Serial.println("Falha na inicialização");
        return;
    }
    
    // Configuração padrão (SF7, BW125kHz, CR4/5, 17dBm)
    radio.setSpreadingFactor(7);
    radio.setSignalBandwidth(125000);
    radio.setCodingRate4(5);
    radio.setTxPower(17);
    radio.enableCrc();
}
```

### Transmissão

```cpp
void transmitir() {
    uint8_t payload[] = {0xAB, 0xCD, 0x01, 0x02, 0x03};
    
    if (radio.beginPacket()) {             // Iniciar pacote
        radio.write(payload, 5);           // Escrever dados
        radio.endPacket(false);            // Transmitir (blocking)
    }
}
```

### Recepção Contínua

```cpp
void loop() {
    radio.receive();                       // Modo RX contínuo
    
    int packetSize = radio.parsePacket();  // Verificar pacotes
    if (packetSize > 0) {
        uint8_t buffer[255];
        size_t bytesRead = radio.readBytes(buffer, packetSize);
        
        Serial.printf("Pacote %d bytes, RSSI=%d dBm, SNR=%.1f dB\n",
                     bytesRead, radio.packetRssi(), radio.packetSnr());
    }
    
    delay(10);
}
```

## API Completa - Classe `SX127x::Radio`

### Construtor/Destrutor

| Método | Descrição |
|--------|-----------|
| `Radio(HAL::SPI* spi)` | Construtor (não inicializa hardware) |
| `~Radio()` | Destrutor (coloca em sleep automaticamente) |

### Inicialização

| Método | Retorno | Descrição |
|--------|---------|-----------|
| `setPins(uint8_t cs, uint8_t reset, uint8_t dio0)` | `void` | Configurar pinos GPIO |
| `begin(uint32_t frequency)` | `bool` | Inicializar + verificar chip |
| `end()` | `void` | Desligar (sleep mode) |

### Configuração RF

| Método | Parâmetros | Retorno | Faixa Válida |
|--------|------------|---------|--------------|
| `setFrequency(uint32_t freq)` | Hz | `RadioError` | 137-1020 MHz |
| `setTxPower(int8_t power)` | dBm | `RadioError` | 2-20 dBm |
| `setSignalBandwidth(uint32_t bw)` | Hz | `RadioError` | 7800,10400,15600,20800,31250,41700,62500,125000,250000,500000 |
| `setSpreadingFactor(uint8_t sf)` | 6-12 | `RadioError` | SF6-SF12 |
| `setCodingRate4(uint8_t denom)` | 5-8 | `RadioError` | 4/5, 4/6, 4/7, 4/8 |
| `setPreambleLength(uint16_t len)` | símbolos | `RadioError` | ≥6 |
| `setSyncWord(uint8_t word)` | 0x00-0xFF | `RadioError` | Evitar 0x00, 0xFF |

### Configuração Pacote

| Método | Descrição |
|--------|-----------|
| `enableCrc()` / `disableCrc()` | Ativar/desativar CRC |
| `enableInvertIQ()` / `disableInvertIQ()` | Inversão IQ (LoRaWAN) |
| `implicitHeaderMode(uint8_t len)` | Header implícito (SF6 obrigatório) |
| `explicitHeaderMode()` | Header explícito (padrão) |

### Transmissão

| Método | Parâmetros | Retorno | Descrição |
|--------|------------|---------|-----------|
| `beginPacket(bool implicit=false)` | - | `RadioError` | Iniciar construção pacote |
| `write(uint8_t* data, size_t len)` | buffer | `size_t` | Escrever dados no FIFO |
| `write(uint8_t byte)` | byte | `size_t` | Escrever 1 byte |
| `print(const String&)` | string | `size_t` | Compatibilidade Arduino |
| `endPacket(bool async=false)` | async | `bool` | Transmitir (blocking/async) |

### Recepção

| Método | Retorno | Descrição |
|--------|---------|-----------|
| `receive()` | `RadioError` | Entrar modo RX contínuo |
| `parsePacket()` | `int` | Bytes disponíveis (0=nada) |
| `available()` | `int` | Bytes restantes |
| `read()` | `int` | Ler 1 byte (-1=erro) |
| `readBytes(uint8_t*, size_t)` | `size_t` | Ler múltiplos bytes |
| `peek()` | `int` | Preview próximo byte |

### Estatísticas Pacote

| Método | Retorno | Descrição |
|--------|---------|-----------|
| `packetRssi()` | `int` | RSSI último pacote (dBm) |
| `packetSnr()` | `float` | SNR último pacote (dB) |
| `rssi()` | `int` | RSSI canal atual (dBm) |
| `packetFrequencyError()` | `int32_t` | Erro frequência (Hz) |

### Controle de Modo

| Método | Retorno | Descrição |
|--------|---------|-----------|
| `sleep()` | `RadioError` | Modo sleep (mínimo consumo) |
| `standby()` | `RadioError` | Modo standby |
| `getMode()` | `RadioMode` | Modo atual |
| `isTransmitting()` | `bool` | TX em progresso |

### Diagnóstico

| Método | Retorno | Descrição |
|--------|---------|-----------|
| `getChipVersion()` | `uint8_t` | Versão chip (0x12=OK) |
| `getLastError()` | `RadioError` | Último erro |
| `dumpRegisters()` | `void` | Dump todos registros (debug) |

## Códigos de Erro (`RadioError`)

```
OK                      = 0
CHIP_NOT_FOUND          = 1
INVALID_FREQUENCY       = 2
INVALID_BANDWIDTH       = 3
INVALID_SPREADING_FACTOR= 4
INVALID_CODING_RATE     = 5
INVALID_TX_POWER        = 6
INVALID_PREAMBLE_LENGTH = 7
INVALID_SYNC_WORD       = 8
PAYLOAD_TOO_LARGE       = 9
TX_TIMEOUT             = 10
RX_TIMEOUT             = 11
CRC_ERROR              = 12
NOT_INITIALIZED        = 13
ALREADY_IN_TX          = 14
ALREADY_IN_RX          = 15
INVALID_MODE           = 16
```

## Otimizações CubeSat

### Consumo de Energia
```
Sleep:      ~0.2 μA (XTAL desligado)
Standby:   ~1.8 mA
RX (SF12): ~10.8 mA
TX (17dBm): ~120 mA
TX (20dBm): ~130 mA
```

### Timing Crítico
```
Reset completo:     20 ms
Mudança modo:       5 ms
TX SF7 (32 bytes):  48 ms
TX SF12 (32 bytes): 1.7 s
RX symbol SF12:    16 ms
CAD:                1-20 ms
```

## Configuração PlatformIO

```ini
[env:ttgo-lora32-v21]
platform = espressif32
board = ttgo-lora32-v21
framework = arduino
lib_deps = 
    ./lib/Drivers/SX127x
build_flags = 
    -DSX127X_DEBUG_REGISTERS
    -DSX127X_DEBUG_SPI
```

## Limitações Conhecidas

1. **Sem interrupções** - Usa polling DIO0 (mais simples, menos consumo IRQ)
2. **SPI blocking** - Transações SPI bloqueiam (thread-safe via mutex)
3. **SF6 limitado** - Requer header implícito (configurado automaticamente)
4. **Sem FSK** - Apenas LoRa mode (suficiente para CubeSat)

## Roadmap Futuro

```
v1.1.0: Suporte interrupções DIO0 (attachInterrupt)
v1.2.0: Cache registros ativo (economia SPI 30%)
v1.3.0: Modo sleep profundo (TCXO)
v2.0.0: Suporte SX126x (nova geração)
```

## Licença e Contribuições

```
Licença: MIT
Contribuições: Pull requests bem-vindos
Testado: TTGO LoRa32 V2.1, ESP32-S3
Datasheet: SX1276/77/78/79 Rev 7 (2020)
```

***

**Documentação técnica completa para integração em produção CubeSat.**