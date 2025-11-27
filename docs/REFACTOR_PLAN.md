# Plano de Refatoração Incremental AgroSat-IoT

## Fases Planejadas

### Fase 0: Auditoria e Documentação
- Mapear dependências, includes e diagramas atuais
- Documentar no diretório /docs

### Fase 1: HAL e Estrutura Modular
- Criar abstrações para hardware em src/hal/
- Migrar managers para uso de HAL

### Fase 2: Decomposição de Módulos Grandes
- Comunicação split em comm/
- Sensores split em sensors/
- Telemetria split em telemetry/

### Fase 3: Padrões de Design e Desacoplamento
- Observer/EventBus, State Machine, Factory

### Fase 4: Configurações e Constantes
- Refatorar config.h em config/
- Build variants no platformio.ini

### Fase 5: Otimização e Testes
- Unit tests, watchdog, memory profiling, LittleFS

## Critérios de Validação
- Compila sem warnings (-Wall)
- Delta RAM/Flash sob controle
- Testes incrementais a cada branch
- Checklist obrigatório por fase

(Plano detalhado no README após cada merge de fase)
