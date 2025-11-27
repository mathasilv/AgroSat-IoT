# Arquitetura Atual - AgroSat-IoT (Pré-Refatoração)

## Visão Geral
Sistema monolítico com forte acoplamento entre módulos. main.cpp orquestra TelemetryManager, que depende de múltiplos managers.

## Diagrama de Dependências
```
main.cpp → TelemetryManager → [SensorManager, CommunicationManager, DisplayManager, StorageManager, etc.]
```
## Mapeamento de Includes (main.cpp)
- Arduino.h (system)
- esp_task_wdt.h (ESP32)
- config.h (local)
- TelemetryManager.h (local)

## Acoplamentos Críticos Identificados
1. **TelemetryManager** (33KB): Centraliza sensores, comunicação LoRa, display → Code smell: God Object
2. **CommunicationManager** (50KB): SX127x driver + protocolo + queue → Violação SRP
3. **SensorManager** (40KB): MPU9250, BMP280, SI7021, CCS811 → Mistura interface + implementação
4. Ciclos potenciais: Storage ↔ Telemetry ↔ Sensors

## Code Smells Principais
- Arquivos >30KB sem decomposição
- Alocações dinâmicas excessivas (Strings, vectors)
- Hardware hardcoded (pinos em config.h mas lógica espalhada)
- Sem abstrações HAL (I2C/SPI direto)
- Falta de interfaces/contratos