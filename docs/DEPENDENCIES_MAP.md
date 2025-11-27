# Mapa de Dependências do Projeto AgroSat-IoT

## Includes e Acoplamentos Identificados

- **main.cpp** inclui: config.h, TelemetryManager.h
- **TelemetryManager.h** inclui: config.h, SensorManager.h, CommunicationManager.h, StorageManager.h, PowerManager.h, SystemHealth.h, DisplayManager.h, RTCManager.h, ButtonHandler.h
- **CommunicationManager.h** inclui: config.h
- **SensorManager.h** inclui: config.h
- **DisplayManager.h** inclui: config.h
- **StorageManager.h** inclui: config.h
- **PayloadManager.h** inclui: config.h
- **PowerManager.h** inclui: config.h
- **RTCManager.h** inclui: config.h
- **SystemHealth.h** inclui: config.h
- **ButtonHandler.h** inclui: config.h

## Diagrama Resumido de Includes

```
                 main.cpp
                     |
           TelemetryManager.h
          /   |   |   |   |   \
CommMgr.h ...         ...   SensorManager.h
```
A maioria dos modules dependem de config.h e de telas específicas, indicando acoplamento alto (problema comum em embedded).

## Observações
- Vários includes cruzados, pois cada manager importa config.h diretamente para adquirir defines, enums e structs globais.
- Dependências de bibliotecas externas concentradas nos headers dos managers (
ex: Wire.h, LoRa.h, HTTPClient.h, Adafruit libs).

## Sugestão Inicial
Refatorar para interfaces desacopladas e centralização das configurações em módulos dedicados nas fases seguintes.
