# Arquitetura Atual do Projeto AgroSat-IoT

## Visão Geral
Este documento descreve como o código está organizado atualmente, detalhando os principais módulos, as dependências diretas e fluxos críticos do sistema embarcado AgroSat-IoT baseado em ESP32 + LoRa SX127x.

## Módulos Fonte

- **src/main.cpp**: Ponto de entrada. Inicializa Serial, Watchdog e TelemetryManager.
- **TelemetryManager**: Orquestrador central, chama métodos de SensorManager, CommunicationManager, StorageManager, etc.
- **CommunicationManager**: Comunicação LoRa, WiFi e HTTP. Possui lógica de fila, transmissão e recepção de pacotes, além de adaptação dinâmica do Spreading Factor (SF).
- **SensorManager**: Inicializa e lê sensores (MPU9250, BMP280, SI7021, CCS811). Implementa múltiplas validações e redundâncias.
- **DisplayManager**: Atualiza display OLED via I2C.
- **StorageManager**: Lida com armazenamento de telemetria em SD Card.
- **PayloadManager**: Composição e parsing dos pacotes enviados/recebidos.
- **PowerManager**: Monitora status de alimentação (bateria) e energia do sistema.
- **RTCManager**: Gerencia RTC (DS3231), sincronismo NTP e horário local.
- **SystemHealth**: Status globais e watchdog do sistema.
- **ButtonHandler**: Interpreta comandos do botão físico (long press, toggle, etc).

## Headers
- Todos os módulos principais têm headers na pasta include/.
- "config.h" centraliza definições, enums e structs de configuração.

## Fluxos Críticos
1. **Startup**: setup() chama begin() dos managers principais.
2. **Loop**: loop() do TelemetryManager orquestra coletas, envio LoRa, logs e update de display.
3. **Button**: Interrupção/manipulação por ButtonHandler afeta operação (modo SAFE, FLIGHT, etc).

## Diagrama Simplificado
```
[main.cpp]
    |
    ↓
[TelemetryManager]
  |   |   |   |   |   |   |   |   |
  ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
[Sensor] [Comm] [Storage] ... outros Managers
```

## Observações
- Acoplamento alto entre managers via propriedades diretas.
- Módulos grandes: CommunicationManager.cpp (~50KB), SensorManager.cpp (~40KB), TelemetryManager.cpp (~33KB).
- Importa todas as dependências diretamente via headers.
