# Plano Detalhado de Refatoração - AgroSat-IoT

## Fases (5 Fases Atômicas)

### Fase 0 ✅ COMPLETA
- Documentação de auditoria
- platformio.ini com warnings habilitados

### Fase 1: HAL Layer (I2C/SPI/GPIO/ADC)
**Entregáveis**: 8 arquivos HAL + migração incremental
**Critério**: Compila, funcionalidade mantida

### Fase 2: Decomposição Módulos Grandes
2.1 CommunicationManager → 4 arquivos (Driver/Protocol/Queue/Manager)
2.2 SensorManager → 7 arquivos (Interface + 4 Sensores + Manager)
2.3 TelemetryManager → 4 arquivos (Builder/Aggregator/Scheduler/Manager)

### Fase 3: Padrões de Design
- EventBus (Observer)
- StateMachine
- SensorFactory
- CommandProcessor

### Fase 4: Configuração Centralizada
- hardwarepins.h, sensorconfig.h, commconfig.h, etc.
- platformio.ini otimizado

### Fase 5: Testes e Otimização
- test/ unit tests
- MemoryProfiler, WatchdogManager

## Checklist de Validação Global
```
## Build Metrics
- [ ] Compila sem erros/warnings (-Wall -Wextra)
- [ ] Flash RAM deltas <5%
- [ ] Boot em <5s

## Funcionalidade
- [ ] Sensores inicializam
- [ ] LoRa TX/RX funciona
- [ ] Display atualiza
- [ ] Estados FSM OK

## Qualidade
- [ ] 100% arquivos completos
- [ ] Sem leaks (heap estável 1h)
- [ ] Convenções seguidas
```

## Roadmap Temporal
| Fase | Duração Estimada | Arquivos |
|------|------------------|----------|
| 0    | Completa        | 4 docs  |
| 1    | 1h              | 8 HAL   |
| 2.1  | 2h              | 8 Comm  |
| 2.2  | 2h              | 11 Sens |
| 2.3  | 1.5h            | 8 Tel   |
```