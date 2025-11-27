# Checklist de Validação - Fase 0: Preparação e Auditoria

## ✅ Build
- [x] Código atual compila sem alterações
- [x] Sem modificações em arquivos de código (.cpp/.h)
- [x] Estrutura de diretórios preservada
- [ ] CI/CD workflow executado com sucesso (pendente primeiro push)

## ✅ Documentação Criada
- [x] docs/ARCHITECTURE_CURRENT.md - Visão geral da arquitetura atual
- [x] docs/DEPENDENCIES_MAP.md - Mapeamento de dependências entre módulos
- [x] docs/REFACTOR_PLAN.md - Plano de refatoração em 5 fases
- [x] .github/workflows/build-test.yml - CI básico com PlatformIO

## ✅ Análise Completa

### Módulos Mapeados (11 arquivos .cpp)
- [x] main.cpp (1.5 KB) - Entry point
- [x] ButtonHandler.cpp (2.7 KB)
- [x] PowerManager.cpp (4.9 KB)
- [x] SystemHealth.cpp (5.4 KB)
- [x] PayloadManager.cpp (6.6 KB)
- [x] RTCManager.cpp (6.7 KB)
- [x] DisplayManager.cpp (12.9 KB)
- [x] StorageManager.cpp (12.9 KB)
- [x] TelemetryManager.cpp (33.8 KB) ⚠️ **GRANDE**
- [x] SensorManager.cpp (40.1 KB) ⚠️ **GRANDE**
- [x] CommunicationManager.cpp (50.8 KB) ⚠️ **GRANDE**

### Headers Mapeados (11 arquivos .h)
- [x] config.h (8.4 KB) - Configurações globais
- [x] Todos os headers correspondentes aos .cpp

### Dependências Identificadas
- [x] TelemetryManager depende de: 8 outros managers
- [x] Todos os managers dependem diretamente de config.h
- [x] Acoplamento alto identificado e documentado
- [x] Bibliotecas externas mapeadas (LoRa, Wire, Adafruit, etc)

## ✅ Prioridades Identificadas

### Módulos que precisam Split (Fase 2)
1. **CommunicationManager.cpp (50.8 KB)**
   - Proposta: dividir em `lora_driver`, `lora_protocol`, `lora_queue`, `communication_manager`
   
2. **SensorManager.cpp (40.1 KB)**
   - Proposta: dividir em `mpu9250_sensor`, `bmp280_sensor`, `si7021_sensor`, `ccs811_sensor`, `sensor_interface`, `sensor_manager`
   
3. **TelemetryManager.cpp (33.8 KB)**
   - Proposta: dividir em `packet_builder`, `data_aggregator`, `telemetry_scheduler`, `telemetry_manager`

### HAL Necessário (Fase 1)
- [x] I2C Manager (usado por sensores + display + RTC)
- [x] SPI Manager (usado por LoRa + SD Card)
- [x] GPIO Manager (usado por botões + LEDs)
- [x] ADC Manager (usado por PowerManager para bateria)

### Configurações para Split (Fase 4)
- [x] config/hardware_pins.h
- [x] config/sensor_config.h
- [x] config/comm_config.h
- [x] config/timing_config.h
- [x] config/memory_config.h

## ✅ Code Smells Identificados

### Alto Acoplamento
- TelemetryManager importa 8 managers diretamente
- Comunicação direta entre módulos sem interfaces
- Dependência global de config.h

### Arquivos Grandes
- 3 arquivos > 30 KB violam Single Responsibility Principle
- CommunicationManager mistura LoRa + WiFi + HTTP + Parsing
- SensorManager mistura 4 sensores diferentes + calibração + validação

### Alocação Dinâmica
- Uso de `String` identificado (fragmentação de heap)
- Uso de `std::vector` em alguns pontos
- Necessário substituir por buffers estáticos na Fase 5

### Configuração Monolítica
- config.h com 8.4 KB mistura pins, timings, sensores, RF, etc
- Dificulta build variants e customização

## ✅ Métricas Baseline

### Tamanho de Código
| Métrica | Atual |
|---------|-------|
| Total arquivos .cpp | 11 |
| Total arquivos .h | 11 |
| Maior arquivo | 50.8 KB |
| Arquivos > 30 KB | 3 |
| Código total estimado | ~177 KB |

### Complexidade
| Aspecto | Status |
|---------|--------|
| Acoplamento | Alto |
| Coesão | Baixa (arquivos grandes) |
| Testabilidade | 0% (sem testes unitários) |
| Documentação | Mínima (apenas comentários) |

## 🎯 Critério de Sucesso da Fase 0

✅ **FASE 0 COMPLETA**

- Nenhuma modificação em código fonte
- Documentação completa criada
- Dependências mapeadas
- Plano de refatoração definido
- CI básico configurado
- Baseline de métricas estabelecido

## 📋 Próximos Passos

**Pronto para FASE 1: Estrutura Modular e HAL**

Na Fase 1 iremos:
1. Criar `src/hal/` e `include/hal/`
2. Implementar I2C, SPI, GPIO e ADC managers
3. Migrar ButtonHandler → usa GPIO HAL
4. Migrar PowerManager → usa ADC HAL
5. Migrar RTCManager → usa I2C HAL
6. Validar build incremental a cada migração

---

**Data de Conclusão da Fase 0:** 2025-11-27  
**Próxima Fase:** Fase 1 - HAL e Estrutura Modular  
**Branch:** `refactor/phase-1-hal`
