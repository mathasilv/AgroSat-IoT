# Mapa de Dependências - Módulo a Módulo

## Tabela de Dependências
| Módulo Origem | Dependências Diretas | Acoplamento | Tamanho |
|---------------|---------------------|-------------|---------|
| main.cpp     | TelemetryManager   | Alto       | 1.5KB  |
| TelemetryManager | SensorManager, CommunicationManager, DisplayManager, StorageManager | Muito Alto | 33KB |
| CommunicationManager | SPI (Wire.h?), config.h | Médio-Alto | 50KB |
| SensorManager | I2C (Wire.h), múltiplos drivers | Alto     | 40KB   |
| DisplayManager | SPI/ST7735       | Médio      | 12KB   |

## Gráfico de Dependências (Texto)
```
                 config.h
                     ↑
main.cpp ──→ TelemetryManager ──┬──→ SensorManager ──→ I2C/Wire.h
                                ├──→ CommunicationManager ──→ SX127x/SPI
                                ├──→ DisplayManager ──→ ST7735
                                └──→ StorageManager ──→ SD/FatFs?
```

## Análise de Ciclos
- Sem ciclos detectados explicitamente
- Fluxo unidirecional main → Telemetry → Others