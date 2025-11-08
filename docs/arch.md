# Organizações, diagramas e documentação tecnológica do CubeSat AgroSat-IoT (OBSAT Fase 2)

## Documentação Técnica

### Arquitetura dos Managers

```
[main.cpp] ---> [TelemetryManager]
                 |---> [SensorManager]
                 |---> [PowerManager]
                 |---> [CommunicationManager]
                 |---> [PayloadManager]
                 |---> [StorageManager]
                 |---> [SystemHealth]
```

- **SensorManager**: Lê e valida todos os sensores físicos.
- **PowerManager**: Mede bateria, status e triggers de economia.
- **CommunicationManager**: Envia dados por WiFi.
- **PayloadManager**: Orquestra LoRa e coleta dados do campo.
- **StorageManager**: Salva telemetria/mission no SD Card.
- **SystemHealth**: Monitora falhas e gera status global.

### Fluxo: Modos do Sistema

1. **Boot (INIT)**
   - Inicializa todos os subsistemas.
   - Diagnóstico geral, memoria e periféricos.
   - Se erro critico: entra MODE_ERROR.

2. **PREFLIGHT**
   - Display OLED ativo, logging completo.
   - Sensores lidos a cada 1s/5s (conforme tipo).
   - Mostra heap, status periféricos, resposta a HELP/STATUS.
   - Transição para FLIGHT via comando serial `START`.

3. **FLIGHT**
   - Display desligado, logging minimalista.
   - Coleta e transmissão telemetria HTTP.
   - Monitoramento de erros críticos apenas.
   - Após 2h (ou STOP), transita para POSTFLIGHT.

4. **POSTFLIGHT**
   - Salva infos finais, status.
   - Permite logs/diagnóstico de erro se necessário.


### Checklist de Validação Embarcada

- [x] Inicialização de todos subsistemas ok?
- [x] Todos sensores online? Heap mínimo >= 12KB?
- [x] Transição PREFLIGHT→FLIGHT limpa?
- [x] Display desligando corretamente?
- [x] Logging ajustado por modo?
- [x] Telemetria HTTP funciona 100%?
- [x] Dados persistidos em SD Card?


### Futuras Melhorias

- Separar interfaces (`BaseManager.h`) e mocks p/testes
- Firmware com define #ifdef para features opcionais (ex: GPS, LoRaWAN)
- Testes unitários automatizados (`test/`)
- Diagrama de sequência: fluxo de erro


---

*Esta documentação resume a arquitetura embarcada e o fluxo de operação da branch dual-mode. Consulte README.md para uso geral e instruções de build.*
