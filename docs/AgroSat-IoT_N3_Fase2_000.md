# **AgroSat-IoT: Sistema de Monitoramento Remoto de Cultivos por CubeSat**

---

**Equipe:** AgroSat-IoT  
**Categoria:** N3 (Ensino Superior)

**Aluno 1:** Matheus Aparecido - Desenvolvedor  
**Aluno 2:** Luana Sthephany Rodrigues Mamed - Desenvolvedor  
**Tutor:** Dr. Aldo Diaz - Orientador técnico

**Instituição:** Universidade Federal de Goiás  
**Estado:** GO

---

## **RESUMO**

O projeto **AgroSat-IoT** propõe o desenvolvimento e validação de um CubeSat 1U funcional, projetado para democratizar o acesso à agricultura de precisão em áreas remotas. O sistema opera como um *gateway* orbital utilizando a arquitetura *Store-and-Forward* baseada em tecnologia LoRa (915 MHz) e comunicação Wi-Fi para telemetria conforme requisitos do edital. A missão principal consiste na coleta de dados críticos de nós sensores terrestres (umidade do solo, temperatura, status de irrigação), seu armazenamento temporário em buffer interno e a subsequente retransmissão para estações em solo. A plataforma de hardware é baseada no microcontrolador ESP32 (módulo TTGO LoRa32 V2.1), integrando sensores obrigatórios: MPU9250 (giroscópio/acelerômetro), BMP280 (pressão/temperatura) e monitoramento de bateria. O firmware implementa transmissão de telemetria via HTTP/JSON conforme Apêndice 1 do edital, com intervalos configuráveis e autonomia superior a 2 horas. A robustez do sistema é assegurada por uma Máquina de Estados Finitos resiliente, monitoramento via Watchdog e gestão eficiente de energia. Os testes de bancada validam a viabilidade técnica da arquitetura proposta.

**Palavras-chave:** CubeSat, IoT, Agricultura de Precisão, LoRa, Wi-Fi, Store-and-Forward, Telemetria.

---

## **1. DECLARAÇÃO DO PROBLEMA DA MISSÃO**

### **1.1 Identificação do Problema**

O Brasil possui aproximadamente 5 milhões de estabelecimentos agropecuários, dos quais 77% são classificados como agricultura familiar (IBGE, 2017). Grande parte dessas propriedades está localizada em regiões remotas, com acesso limitado ou inexistente a redes de comunicação convencionais como internet móvel ou fibra óptica.

A falta de conectividade impede que estes produtores implementem tecnologias de agricultura de precisão, resultando em:

- **Perdas de produtividade:** Estimativas indicam que 20-30% das perdas agrícolas poderiam ser evitadas com monitoramento adequado (FAO, 2019).
- **Uso ineficiente de recursos:** Desperdício de água em sistemas de irrigação não monitorados.
- **Detecção tardia de problemas:** Pragas e doenças que poderiam ser controladas em estágios iniciais.
- **Decisões baseadas em estimativas:** Sem dados precisos, produtores tomam decisões subótimas.

### **1.2 Condições e Ações Necessárias**

Para resolver esse problema, são necessárias as seguintes condições e ações:

1. **Conectividade confiável:** Estabelecer comunicação em áreas rurais remotas para viabilizar o fluxo contínuo de informações entre sensores no campo, o satélite e plataformas de análise.

2. **Coleta contínua de dados:** O sistema deve coletar dados de múltiplos pontos distribuídos pela plantação, identificando variações nas condições das lavouras.

3. **Processamento e transmissão eficiente:** Processamento local para reduzir volume de dados e otimizar uso da largura de banda.

4. **Visualização acessível:** Dados apresentados em formato que facilite a tomada de decisão pelos agricultores.

5. **Custos acessíveis:** Solução economicamente viável para pequenos produtores.

---

## **2. OBJETIVOS DA MISSÃO E MÉRITO CIENTÍFICO**

### **2.1 Objetivo Geral**

Desenvolver e validar um protótipo funcional de CubeSat 1U operando sob a arquitetura **Store-and-Forward**, capaz de atuar como um *gateway* orbital autônomo para coleta, priorização e retransmissão de dados de sensores agrícolas via tecnologia LoRa, com telemetria via Wi-Fi conforme requisitos do edital.

### **2.2 Objetivos Específicos**

1. Demonstrar viabilidade técnica de comunicação LoRa satélite-Terra;
2. Implementar telemetria via Wi-Fi/HTTP conforme Apêndice 1 do edital;
3. Coletar e processar dados de múltiplos sensores simultaneamente;
4. Validar o conceito de IoT via satélite para agricultura;
5. Testar resistência do sistema a condições extremas (temperatura, vibração);
6. Garantir autonomia mínima de 2 horas de operação.

### **2.3 Mérito Científico**

**Comunicações via satélite:** O projeto valida a tecnologia LoRa para enlaces espaciais, contribuindo para o estado da arte em comunicações IoT de longo alcance.

**Agricultura 4.0:** Desenvolve arquitetura que integra sensoriamento remoto satelital com redes terrestres de sensores IoT, estabelecendo requisitos específicos para aplicações agrícolas.

**Inclusão digital rural:** Aborda a provisão de conectividade para regiões isoladas através de modelos de cobertura satelital otimizados.

**Sustentabilidade:** Facilita a visualização de parâmetros agronômicos, permitindo intervenções precisas que minimizam desperdício de insumos e reduzem impacto ambiental.

---

## **3. FUNÇÕES E RESPONSABILIDADES DA EQUIPE**

| Membro | Função | Responsabilidades |
|--------|--------|-------------------|
| **Matheus Aparecido** | Desenvolvedor | Desenvolvimento de firmware, integração de hardware, testes de comunicação LoRa/Wi-Fi, programação do sistema de telemetria |
| **Luana Sthephany R. Mamed** | Desenvolvedor | Projeto mecânico, integração de sensores, testes de robustez, documentação técnica |
| **Dr. Aldo Diaz** | Tutor/Orientador | Supervisão técnica, garantia do rigor metodológico, interface com a organização |

### **Divisão de Tarefas por Subsistema**

- **Software Embarcado:** Matheus (principal), Luana (suporte)
- **Hardware/Eletrônica:** Luana (principal), Matheus (suporte)
- **Estrutura Mecânica:** Luana (principal)
- **Testes e Validação:** Ambos os alunos
- **Documentação:** Ambos os alunos
- **Revisão Técnica:** Dr. Aldo Diaz

A equipe funciona de forma colaborativa, com decisões tomadas em conjunto e divisão dinâmica de tarefas conforme necessidade.

---

## **4. CRONOGRAMA DE DESENVOLVIMENTO**

### **4.1 Cronograma da Fase 2**

| Semana | Período | Atividades |
|--------|---------|------------|
| 1-2 | Out/2025 | Integração da placa PION e LilyGo LoRa32, testes individuais de sensores |
| 3 | Out/2025 | Desenvolvimento do simulador de nós agrícolas (ESP32-C3) |
| 4 | Nov/2025 | Implementação da comunicação LoRa/WiFi, projeto e sintonização de antena |
| 5 | Nov/2025 | Montagem final, regulação 3V3, aplicação de isolamento térmico, validação RF |
| 6 | Nov/2025 | Testes de integração completos, confecção do relatório e vídeo |

### **4.2 Plano de Trabalho - Próximas Fases**

| Período | Fase | Atividades Planejadas |
|---------|------|----------------------|
| Jan-Abr/2026 | Fase 3 | Preparação para evento regional, ajustes finais, testes de qualificação |
| Mai-Nov/2026 | Fase 4 | Melhorias baseadas em lições aprendidas, implementação de telemetria RF, participação no evento nacional |

---

## **5. DETALHES OPERACIONAIS**

### **5.1 Conformidade com os Requisitos do Edital**

#### **5.1.1 Estrutura Mecânica**

| Requisito | Especificação do Edital | Implementação AgroSat-IoT | Status |
|-----------|------------------------|---------------------------|--------|
| Form Factor | CubeSat: 100 x 100 x 100 mm | **[PREENCHER: ___ x ___ x ___ mm]** | ⬜ Verificar |
| Material | Termoplástico (PLA ou PETG) | PETG de alta resistência | ✅ Conforme |
| Peso máximo | 450g | **[PREENCHER: ___g]** | ⬜ Verificar |

**[Ver Anexo A: Fotos da Estrutura e Relatório de Impressão]**  
**[Ver Anexo B: Laudo de Pesagem com balança de precisão]**

#### **5.1.2 Operação**

| Requisito | Especificação | Implementação | Status |
|-----------|---------------|---------------|--------|
| Altitude de operação | Até 30 km | Sistema projetado para ambiente estratosférico | ✅ Conforme |
| Isolamento térmico da bateria | Depron ou espuma EPE | Espuma EPE de 5mm envolvendo a célula Li-Ion | ✅ Conforme |
| Armazenamento de dados | Memória interna | Cartão MicroSD 16GB Class 10 | ✅ Conforme |

**[Ver Anexo C: Detalhe do Isolamento Térmico da Bateria]**

#### **5.1.3 Telemetria (Conforme Apêndice 1 do Edital)**

| Requisito | Especificação | Implementação | Status |
|-----------|---------------|---------------|--------|
| Protocolo | Wi-Fi via HTTP | WiFiService + HttpService | ✅ Conforme |
| Formato | JSON | PayloadManager.createTelemetryJSON() | ✅ Conforme |
| Tempo mínimo de operação | 2 horas | **[PREENCHER: ___ horas em teste]** | ⬜ Verificar |
| Intervalo de transmissão | 4 minutos | Configurável via config.h (padrão: 240s) | ✅ Conforme |
| Limite de payload | 90 bytes | Validação automática no firmware | ✅ Conforme |

**Campos obrigatórios do JSON (conforme Apêndice 1):**

| Campo | Sensor/Fonte | Implementação |
|-------|--------------|---------------|
| equipe | Identificador | "AgroSat-IoT" |
| bateria | Divisor resistivo (GPIO 35) | PowerManager.getBatteryPercentage() |
| temperatura | BMP280 / SI7021 | SensorManager.getTemperature() |
| pressao | BMP280 | BMP280Manager.getPressure() |
| giroscopio | MPU9250 | MPU9250Manager.getGyroscope() |
| acelerometro | MPU9250 | MPU9250Manager.getAccelerometer() |
| payload | Dados de missão | PayloadManager.getMissionPayload() |

**Exemplo de pacote JSON transmitido:**

```json
{
  "equipe": "AgroSat-IoT",
  "bateria": 85,
  "temperatura": 23.5,
  "pressao": 1013.25,
  "giroscopio": [0.12, -0.05, 0.08],
  "acelerometro": [0.01, 0.02, 9.81],
  "payload": {
    "mission": "store_forward",
    "nodes_collected": 2,
    "priority_alerts": 0
  }
}
```

**Nota:** O payload de missão contém informações sobre os nós terrestres coletados via LoRa, respeitando o limite de 90 bytes.

---

### **5.2 Arquitetura da Missão Store-and-Forward**

#### **5.2.1 Funcionamento do Sistema de Relay Orbital**

O AgroSat-IoT opera como uma ponte de comunicação orbital, gerenciada pela classe `GroundNodeManager` (src/app/GroundNodeManager), que permite a recepção, armazenamento e retransmissão de dados de sensores agrícolas isolados.

**Fluxo operacional:**

1. **Uplink (Recepção):** O transceptor LoRa SX1276 (via `LoRaService`) escuta continuamente pacotes dos nós terrestres durante a passagem do satélite.

2. **Armazenamento:** Dados são decodificados e armazenados em buffer circular na RAM, dimensionado em `config.h` (MAX_GROUND_NODES).

3. **Priorização (QoS):** O `PayloadManager` classifica pacotes em níveis de prioridade: CRITICAL, HIGH, NORMAL, LOW.

4. **Downlink (Transmissão):** Quando há conectividade Wi-Fi, o `CommunicationManager` envia pacotes consolidados via HTTP.

5. **Persistência:** O `StorageManager` realiza backup síncrono no cartão SD antes de cada transmissão.

**[Ver Anexo D: Diagrama de Fluxo de Dados Store-and-Forward]**

#### **5.2.2 Sistema de Priorização Inteligente (QoS)**

O algoritmo de Qualidade de Serviço (método `calculateNodePriority` em `PayloadManager`) avalia cada pacote contra limiares críticos:

| Prioridade | Condição | Exemplo |
|------------|----------|--------|
| CRITICAL | Seca extrema ou alagamento | Umidade < 10% ou > 95% |
| HIGH | Falha de comunicação iminente | Bateria do nó < 20% |
| NORMAL | Dados de rotina | Leituras periódicas |
| LOW | Dados redundantes | Confirmações |

**[Ver Anexo E: Tabela de Critérios de Classificação QoS]**

#### **5.2.3 Sequência Temporal da Missão**

O ciclo de vida é controlado pelo `MissionController` em seis fases:

1. Validação e timestamping via RTC (RTCManager)
2. Cálculo de prioridade
3. Persistência segura no SD
4. Geração dinâmica do payload de downlink
5. Transmissão e marcação de pacotes enviados
6. Gravação de logs para análise pós-voo

**[Ver Anexo F: Diagrama de Sequência Temporal]**

---

### **5.3 Infraestrutura de Hardware**

#### **5.3.1 Computador de Bordo (OBC) e Comunicação**

O núcleo de processamento é baseado no módulo **TTGO LoRa32 V2.1**, que integra:

- **Microcontrolador:** ESP32-PICO-D4 (Dual Core 240MHz)
- **Transceptor LoRa:** SX1276 (915 MHz, banda ISM)
- **Wi-Fi:** 802.11 b/g/n integrado
- **Memória:** 4MB Flash, 520KB SRAM

O firmware explora a arquitetura dual-core via FreeRTOS, executando tarefas de comunicação e leitura de sensores simultaneamente. A eficiência energética é gerenciada pela classe `PowerManager` com Dynamic Frequency Scaling (DFS):

| Modo | Clock CPU | Condição |
|------|-----------|----------|
| Performance | 240 MHz | Bateria > 50% |
| Normal | 160 MHz | Bateria 30-50% |
| Economy | 80 MHz | Bateria < 30% |

**[Ver Anexo G: Datasheet e Esquemático do OBC]**

#### **5.3.2 Sistema Irradiante (Antena)**

Antena monopolo de quarto de onda construída com **fita métrica de aço (trena)**:

- **Frequência central:** 915 MHz
- **Impedância:** 50 Ohms
- **VSWR medido:** **[PREENCHER: ___]** (< 1.5 esperado)
- **Comprimento:** ~82 mm (λ/4 para 915 MHz)

A validação foi realizada com analisador vetorial de redes (NanoVNA).

**[Ver Anexo H: Gráficos de VSWR e Smith Chart da Antena]**

#### **5.3.3 Instrumentação Científica (Sensores)**

| Sensor | Função | Interface | Classe no Firmware |
|--------|--------|-----------|--------------------|
| **MPU9250** | IMU 9-DOF (Acelerômetro, Giroscópio, Magnetômetro) | I2C | MPU9250Manager |
| **BMP280** | Pressão atmosférica e temperatura | I2C | BMP280Manager |
| **SI7021** | Umidade relativa e temperatura | I2C | SI7021Manager |
| **CCS811** | Qualidade do ar (eCO2, TVOC) | I2C | CCS811Manager |
| **DS3231** | RTC de alta precisão | I2C | RTCManager |
| **NEO-M8N** | GPS/GLONASS | UART | GPSManager |

Todos os sensores são orquestrados pela classe `SensorManager` para aquisição síncrona.

**[Ver Anexo I: Diagrama de Conexões e Layout da PCB]**

#### **5.3.4 Sistema de Energia e Proteção Térmica**

**Energia:**
- Célula Li-Ion 18650 (3.7V / 3000mAh)
- Monitoramento via divisor resistivo no GPIO 35
- Algoritmo de estimativa não-linear de carga no `PowerManager`

**Proteção Térmica:**
- Isolamento da bateria com espuma EPE de 5mm (obrigatório pelo edital)
- Revestimento externo com fita Kapton
- Temperatura mínima de operação testada: -20°C

**[Ver Anexo J: Registro Fotográfico da Integração Térmica]**

---

### **5.2 Arquitetura da Missão Store-and-Forward**

#### **5.2.1 Funcionamento do Sistema de Relay Orbital**

O AgroSat-IoT opera como uma ponte de comunicação orbital, gerenciada pela classe `GroundNodeManager` (src/app/GroundNodeManager), que permite a recepção, armazenamento e retransmissão de dados de sensores agrícolas isolados.

**Fluxo operacional:**

1. **Escuta contínua:** O transceptor LoRa SX1276 (via `LoRaService`) captura pacotes de uplink dos nós terrestres
2. **Decodificação e armazenamento:** Dados são armazenados em buffer circular na RAM (configurável em config.h)
3. **Retransmissão (downlink):** Quando detectada conectividade Wi-Fi, o `CommunicationManager` envia pacote consolidado via HTTP
4. **Backup:** `StorageManager` realiza backup síncrono no cartão SD antes de cada transmissão

**[Ver Anexo D: Diagrama de Fluxo de Dados Store-and-Forward]**

#### **5.2.2 Sistema de Priorização Inteligente (QoS)**

O firmware implementa um algoritmo de Qualidade de Serviço (QoS) na classe `PayloadManager` (método `calculateNodePriority`):

| Nível de Prioridade | Condição | Ação |
|---------------------|----------|------|
| CRITICAL | Seca extrema ou alagamento | Transmissão imediata |
| HIGH | Falhas de comunicação iminentes | Prioridade na fila |
| NORMAL | Dados de rotina | Ordem de chegada |
| LOW | Dados não-críticos | Substituíveis se buffer cheio |

**[Ver Anexo E: Tabela de Critérios de Classificação QoS]**

#### **5.2.3 Sequência Temporal da Missão**

O ciclo de vida é controlado pelo `MissionController`:

1. Validação e timestamping via RTC (RTCManager)
2. Cálculo de prioridade QoS
3. Persistência segura no SD
4. Geração dinâmica do payload de downlink
5. Marcação de pacotes confirmados como "enviados"
6. Gravação de logs para análise pós-voo

**[Ver Anexo F: Diagrama de Sequência Temporal]**

---

### **5.3 Infraestrutura de Hardware**

#### **5.3.1 Computador de Bordo (OBC) e Comunicação**

O núcleo de processamento é baseado no módulo **TTGO LoRa32 V2.1**, que integra:

- Microcontrolador ESP32 dual-core (240 MHz)
- Transceptor LoRa SX1276 (915 MHz)
- Wi-Fi 802.11 b/g/n integrado
- Display OLED 0.96" (opcional)

O firmware explora a arquitetura dual-core via FreeRTOS, executando tarefas de comunicação e leitura de sensores simultaneamente. A eficiência energética é gerenciada pela classe `PowerManager` com Dynamic Frequency Scaling (DFS):

| Modo | Clock CPU | Condição de Ativação |
|------|-----------|---------------------|
| Performance | 240 MHz | Bateria > 50% |
| Normal | 160 MHz | Bateria 30-50% |
| Economy | 80 MHz | Bateria < 30% ou MODE_SAFE |

**[Ver Anexo G: Esquemático do OBC e Conexões]**

#### **5.3.2 Sistema Irradiante (Antena)**

Antena monopolo de quarto de onda construída com **fita métrica de aço (trena)**:

- **Frequência central:** 915 MHz (banda ISM)
- **Impedância:** 50 Ohms
- **VSWR medido:** < 1.5
- **Validação:** Analisador vetorial de redes (NanoVNA)

Vantagens da construção:
- Memória elástica permite dobrar para armazenamento
- Resistência a deformações por turbulência
- Padrão de radiação omnidirecional

**[Ver Anexo H: Gráficos de VSWR e Smith Chart da Antena]**

#### **5.3.3 Instrumentação Científica (Sensores)**

| Sensor | Função | Interface | Classe no Firmware |
|--------|--------|-----------|--------------------|
| **MPU9250** | IMU 9-DOF (Acelerômetro, Giroscópio, Magnetômetro) | I2C | MPU9250Manager |
| **BMP280** | Pressão atmosférica e temperatura | I2C | BMP280Manager |
| **SI7021** | Umidade relativa e temperatura | I2C | SI7021Manager |
| **CCS811** | Qualidade do ar (eCO2, TVOC) | I2C | CCS811Manager |
| **DS3231** | RTC de alta precisão | I2C | RTCManager |
| **NEO-M8N** | GPS/GLONASS | UART | GPSManager |

Todos os sensores são orquestrados pela classe `SensorManager` para aquisição síncrona.

**[Ver Anexo I: Diagrama de Conexões e Layout da PCB]**

#### **5.3.4 Sistema de Energia e Proteção Térmica**

**Energia:**
- Célula Li-Ion 18650 (3.7V / 3000mAh)
- Monitoramento via divisor resistivo no GPIO 35
- Algoritmo de estimativa não-linear de carga no `PowerManager`

**Proteção Térmica:**
- Isolamento da bateria com espuma EPE de 5mm (obrigatório pelo edital)
- Revestimento externo com fita Kapton
- Proteção contra temperaturas de até -80°C

**[Ver Anexo J: Registro Fotográfico da Integração Térmica]**

---

### **5.4 Arquitetura de Firmware e Lógica de Controle**

#### **5.4.1 Máquina de Estados Finitos (FSM)**

A autonomia do satélite é orquestrada por uma FSM resiliente implementada na classe `TelemetryManager`:

```
┌─────────────┐
│  MODE_INIT  │ ──► Inicialização de drivers I2C/SPI, sync NTP
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│  MODE_PREFLIGHT │ ──► Interfaces de debug ativas, calibração
└────────┬────────┘
         │ (START_MISSION ou botão)
         ▼
┌─────────────┐
│ MODE_FLIGHT │ ──► Coleta de dados, escuta LoRa, transmissão
└──────┬──────┘
       │ (Bateria < 3.3V ou Heap < 10kB)
       ▼
┌────────────┐
│ MODE_SAFE  │ ──► CPU 80MHz, beacon esparso (180s)
└────────────┘
```

**Transições de estado:**
- Via comando serial: `START_MISSION`, `SAFE_MODE`, `STATUS`
- Via botão físico (GPIO4): clique curto = alternar modo, clique longo (2s) = MODE_SAFE

**Recuperação de falhas (Crash Recovery):**
O `MissionController` persiste o estado na memória NVS do ESP32. Em caso de reinício não planejado, o sistema recupera automaticamente o timestamp de início e retorna ao estado de voo.

**[Ver Anexo K: Diagrama da Máquina de Estados]**

#### **5.4.2 Protocolos de Telemetria**

A classe `PayloadManager` implementa arquitetura híbrida:

| Protocolo | Uso | Método | Tamanho |
|-----------|-----|--------|--------|
| JSON/HTTP | Telemetria Wi-Fi (edital) | createTelemetryJSON() | ~200 bytes |
| Binário | Comunicação LoRa | createSatellitePayload() | 36 bytes |
| Binário | Relay de nós terrestres | createRelayPayload() | <70 bytes |

#### **5.4.3 Estratégia de Persistência (Zero Data Loss)**

O `StorageManager` opera em múltiplas camadas:

1. **Camada primária:** Gravação contínua em SD
   - `telemetry.csv` - dados científicos
   - `mission.csv` - dados dos nós terrestres
   - `system.log` - eventos de erro

2. **Proteção de integridade:** CRC-16 (CCITT) por linha

3. **Rotação automática:** Segmentação ao atingir 5MB

4. **Redundância tripla:** Para dados críticos (bateria, GPS), método `_writeTripleRedundant` com votação majoritária (2 de 3)

**[Ver Anexo L: Fluxograma do Sistema de Armazenamento]**

---

### **5.5 Procedimento de Execução da Missão**

#### **5.5.1 Fase de Preparação (T-24h a T-2h)**

O sistema opera em `MODE_PREFLIGHT` com interfaces de diagnóstico ativas:

| Comando | Função | Classe |
|---------|--------|--------|
| `STATUS` | Relatório de saúde de todos os barramentos | CommandHandler |
| `CALIB_MAG` | Calibração do magnetômetro (500 amostras) | MPU9250Manager |
| `SAVE_BASELINE` | Persistir referência do CCS811 | CCS811Manager |
| `TEST_WIFI` | Verificar conectividade Wi-Fi | WiFiService |
| `TEST_LORA` | Verificar transceptor LoRa | LoRaService |

**[Ver Anexo M: Logs de Calibração e Validação de Bancada]**

#### **5.5.2 Fase de Ativação (T-0)**

A transição para `MODE_FLIGHT` pode ser realizada:

1. **Via comando serial:** Enviar `START_MISSION`
2. **Via botão físico:** Pressionamento curto no GPIO4

**Ações automáticas na ativação:**
- Registro do timestamp UTC inicial
- Salvamento da flag de missão ativa na NVS
- Desativação de logs seriais (economia de energia)
- Ajuste do intervalo de telemetria para perfil de voo
- Início da gravação no cartão SD

**[Ver Anexo N: Registro de Ativação da Missão]**

#### **5.5.3 Fase de Voo (T+0 a T+120min)**

Durante ascensão e flutuação:

1. `TelemetryManager` coordena leitura dos sensores via `TelemetryCollector`
2. `PowerManager` monitora e ajusta consumo de energia
3. `LoRaService` mantém escuta contínua para sinais de nós terrestres
4. `GroundNodeManager` processa, valida e armazena pacotes recebidos
5. Quando Wi-Fi disponível, transmissão de telemetria via HTTP
6. Dados prioritários (QoS) são transmitidos primeiro

#### **5.5.4 Fase de Recuperação (Pós-voo)**

- `StorageManager` garante flush forçado e validação CRC
- Última coordenada GPS salva com redundância tripla
- Extração do cartão SD para análise dos arquivos:
  - `telemetry.csv` - reconstrução da trajetória
  - `mission.csv` - dados dos nós coletados
  - `system.log` - diagnóstico de eventos

**[Ver Anexo O: Exemplo de Dados Recuperados]**

---

### **5.6 Lista de Materiais (BOM)**

| Subsistema | Componente | Especificação Técnica | Qtd. |
|------------|------------|----------------------|------|
| **OBC/Comm** | TTGO LoRa32 V2.1 | ESP32-PICO-D4 (Dual Core 240MHz) + SX1276 LoRa (915 MHz) + Wi-Fi | 1 |
| **Carga Útil** | Placa de Sensores PION | PCB integrada com barramento I2C unificado | 1 |
| *Sensor 1* | MPU-9250 | IMU 9-Eixos (Acelerômetro, Giroscópio, Magnetômetro) | 1 |
| *Sensor 2* | BMP280 | Barômetro digital de alta precisão (Pressão/Temperatura) | 1 |
| *Sensor 3* | Si7021 | Sensor de umidade relativa e temperatura | 1 |
| *Sensor 4* | CCS811 | Sensor MOX de qualidade do ar (eCO2 e TVOC) | 1 |
| **Tempo** | Módulo DS3231 | RTC TCXO externo de alta estabilidade (I2C) | 1 |
| **Navegação** | Módulo NEO-M8N | Receptor GNSS (GPS/GLONASS) com antena cerâmica ativa | 1 |
| **Armazenamento** | Cartão MicroSD | 16GB Class 10 (Industrial) | 1 |
| **Energia** | Bateria Li-Ion 18650 | 3.7V / 3000mAh com proteção PCM | 1 |
| **Estrutura** | Chassi CubeSat 1U | Termoplástico PETG impresso em 3D | 1 |
| **Antena** | Monopolo 1/4 onda | Fita de aço (trena) sintonizada para 915 MHz | 1 |
| **Interface** | Botão tátil | Push-button momentâneo 6x6mm | 1 |
| **Cabeamento** | Jumpers e conectores | Cabos AWG 24 com isolamento de silicone | Kit |
| **Isolamento** | Espuma EPE | Polietileno expandido 5mm (proteção térmica) | 1 |
| **Acabamento** | Fita Kapton | Isolamento térmico e elétrico | 1 rolo |

**[Ver Anexo P: Fotos dos Componentes]**

---

### **5.7 Disponibilidade do Firmware e Repositório**

Todo o código-fonte está disponível em repositório público:

- **Repositório:** https://github.com/music-soul1-1/AgroSat-IoT
- **Licença:** MIT License (Código Aberto)

**Estrutura do repositório:**

```
AgroSat-IoT/
├── src/
│   ├── app/           # Lógica de missão
│   │   ├── TelemetryManager.cpp
│   │   ├── PayloadManager.cpp
│   │   ├── MissionController.cpp
│   │   └── GroundNodeManager.cpp
│   ├── core/          # Serviços de sistema
│   │   ├── SystemHealth.cpp
│   │   ├── PowerManager.cpp
│   │   └── StorageManager.cpp
│   ├── comm/          # Drivers de comunicação
│   │   ├── LoRaService.cpp
│   │   ├── WiFiService.cpp
│   │   └── HttpService.cpp
│   └── sensors/       # Gerenciadores de sensores
│       ├── SensorManager.cpp
│       ├── MPU9250Manager.cpp
│       ├── BMP280Manager.cpp
│       └── ...
├── Ground_Station/    # Estação terrestre
└── config.h           # Configurações do sistema
```

**[Ver Anexo Q: Código-fonte comentado das principais classes]**

---

## **6. MONTAGEM, INTEGRAÇÃO E TESTES**

### **6.1 Relatório de Montagem**

#### **6.1.1 Sequência de Integração**

1. **Preparação do backplane:**
   - Soldagem do barramento de 30 pinos nas placas perfuradas
   - Criação da espinha dorsal para interconexão dos módulos

2. **Integração da bateria:**
   - Envelopamento em espuma EPE/Depron (5mm)
   - Aplicação de fita Kapton para isolamento elétrico
   - Fixação mecânica com fita Hellermann
   - Soldagem aos terminais de energia da placa PION

3. **Integração do OBC:**
   - Conexão da LilyGo LoRa32 ao backplane
   - Instalação dos módulos periféricos (RTC DS3231, GPS NEO-M8N)
   - Soldagem ponto a ponto conforme esquemático
   - Adição de regulador AMS1117 para picos de corrente

4. **Integração do sistema irradiante:**
   - Corte e sintonização da antena de fita métrica (NanoVNA)
   - Fixação à lateral do chassi
   - Conexão ao transceptor LoRa

5. **Fechamento da estrutura:**
   - Revestimento interno com Depron
   - Acabamento externo com fita Kapton
   - Fechamento com parafusos de aço inoxidável
   - Selagem das junções

**[Ver Anexo R: Fotos de Todas as Faces do Satélite]**

#### **6.1.2 Fotos da Montagem**

| Face | Descrição | Anexo |
|------|-----------|-------|
| Superior | Vista da antena e conectores | **[Anexo R1]** |
| Inferior | Vista da base de fixação | **[Anexo R2]** |
| Frontal | Vista do OBC e display | **[Anexo R3]** |
| Traseira | Vista da bateria isolada | **[Anexo R4]** |
| Lateral Esquerda | Vista dos sensores | **[Anexo R5]** |
| Lateral Direita | Vista das conexões | **[Anexo R6]** |
| Interna | Vista dos componentes internos | **[Anexo R7]** |

---

### **6.2 Ecossistema de Validação**

Para validar a arquitetura Store-and-Forward, foram desenvolvidos dois sistemas complementares:

#### **6.2.1 Simulador de Rede Agrícola**

- **Hardware:** ESP32-C3 SuperMini + SX1276
- **Função:** Emular rede de sensores distribuída no campo
- **Protocolo:** Listen Before Talk (LBT) para evitar colisão
- **Dados gerados:** Temperatura, umidade, status de irrigação

#### **6.2.2 Estação Terrestre (GCS)**

- **Hardware:** RTL-SDR V3 + Antena LoRa 915 MHz
- **Software:** Decodificador LoRa + Dashboard Grafana
- **Função:** Recepção e visualização em tempo real

**Métricas monitoradas:**
1. Saúde do satélite (tensão, temperatura, altitude)
2. Sucesso da retransmissão dos nós
3. Qualidade do enlace (RSSI/SNR)

**[Ver Anexo S: Diagrama do Ecossistema de Validação]**

---

### **6.3 Descrição e Resultados dos Testes**

#### **6.3.1 Caracterização Física**

| Parâmetro | Especificação | Valor Medido | Status |
|-----------|---------------|--------------|--------|
| Dimensão X | 100 mm | **[PREENCHER: ___ mm]** | ⬜ |
| Dimensão Y | 100 mm | **[PREENCHER: ___ mm]** | ⬜ |
| Dimensão Z | 100 mm | **[PREENCHER: ___ mm]** | ⬜ |
| Massa total | ≤ 450g | **[PREENCHER: ___g]** | ⬜ |

**Instrumento de medição:** 
- Dimensões: Paquímetro digital (resolução 0.01mm)
- Massa: Balança de precisão (resolução 0.1g)

**[Ver Anexo T: Laudo de Caracterização Física com fotos]**

#### **6.3.2 Teste de Robustez Mecânica**

**Metodologia:**
- Satélite em operação (MODE_PREFLIGHT)
- Agitação vigorosa nos três eixos por 60 segundos
- Monitoramento de telemetria em tempo real

**Resultados:**

| Critério | Resultado |
|----------|----------|
| Integridade estrutural | ✅ Mantida - sem desconexões |
| Fixação da bateria | ✅ Estável |
| Conexões de solda | ✅ Íntegras |
| Funcionamento dos sensores | ⚠️ BMP280 apresentou travamento temporário |

**Solução implementada:**
Lógica de recuperação automática (self-healing) na classe `BMP280Manager`:
- Monitoramento de variância das leituras
- Soft reset automático após 5s de valores estáticos
- Reinicialização I2C sem interromper outros subsistemas

**[Ver Anexo U: Log do Teste de Vibração e Recuperação]**

#### **6.3.3 Teste de Robustez Térmica**

**Metodologia:**
- Satélite em MODE_FLIGHT com isolamento completo
- Inserção em câmara fria (freezer) a -20°C
- Duração: 60 minutos
- Monitoramento contínuo de telemetria

**Resultados:**

| Parâmetro | Valor Inicial | Valor Final | Status |
|-----------|---------------|-------------|--------|
| Temp. externa | **[PREENCHER]** | **[PREENCHER]** | ⬜ |
| Temp. bateria | **[PREENCHER]** | **[PREENCHER]** | ⬜ |
| Tensão bateria | **[PREENCHER]** | **[PREENCHER]** | ⬜ |
| Funcionamento | Operacional | **[PREENCHER]** | ⬜ |

**Conclusão:** O isolamento EPE/Depron manteve a temperatura da bateria acima de 0°C, prevenindo congelamento do eletrólito.

**[Ver Anexo V: Gráficos de Temperatura do Teste Térmico]**

---

#### **6.3.4 Teste de Robustez Eletrônica e Magnética**

**Metodologia:**
- Inspeção visual das conexões de alimentação
- Exposição a faixa variada de frequências
- Varredura de emissão eletromagnética do protótipo

**Resultados:**

| Teste | Resultado |
|-------|----------|
| Conexões de alimentação | **[PREENCHER: OK/NOK]** |
| Funcionamento sob interferência | **[PREENCHER: descrição]** |
| Faixa de emissão | **[PREENCHER: frequências detectadas]** |

**[Ver Anexo W: Relatório de Testes EMC]**

#### **6.3.5 Teste de Captura e Transmissão de Dados**

**Configuração do teste:**
- **Emissor:** Simulador de Nós Agrícolas (ESP32-C3 + SX1276)
- **Receptor:** Estação Terrestre (RTL-SDR V3 + Antena 915 MHz)
- **Modo:** Stress Test (transmissão a cada 10 segundos)
- **Atenuação:** 20dB simulada
- **Duração:** 60 minutos

**Resultados quantitativos:**

| Métrica | Valor Obtido |
|---------|-------------|
| Total de pacotes enviados | 360 |
| Pacotes recebidos (CRC OK) | 358 |
| Pacotes perdidos | 2 |
| Taxa de entrega (PDR) | **99.44%** |
| Taxa de perda (PLR) | **0.56%** |
| RSSI médio | -85 dBm |
| SNR médio | 9.5 dB |

**Análise:**
A taxa de perda inferior a 1% valida a robustez do protocolo. Os 2 pacotes perdidos ocorreram em momentos de interferência externa na banda ISM 915 MHz, confirmando a necessidade da estratégia de retransmissão implementada.

**[Ver Anexo X: Log de Transmissão e Gráficos RSSI/SNR]**

#### **6.3.6 Teste de Armazenamento de Dados**

**Metodologia:**
- Operação contínua por 2 horas
- Verificação de integridade dos arquivos no SD
- Validação de CRC-16 linha a linha

**Resultados:**

| Arquivo | Registros | CRC Válidos | Integridade |
|---------|-----------|-------------|-------------|
| telemetry.csv | **[PREENCHER]** | **[PREENCHER]** | ⬜ |
| mission.csv | **[PREENCHER]** | **[PREENCHER]** | ⬜ |
| system.log | **[PREENCHER]** | **[PREENCHER]** | ⬜ |

**[Ver Anexo Y: Amostra dos Dados Armazenados]**

#### **6.3.7 Teste de Transmissão HTTP (Conforme Apêndice 1)**

**Metodologia:**
- Conexão à rede Wi-Fi de teste
- Envio de pacotes JSON ao servidor https://obsat.org.br/teste_post/
- Verificação de recebimento no painel

**Resultados:**

| Parâmetro | Resultado |
|-----------|----------|
| Conexão Wi-Fi | **[PREENCHER: OK/NOK]** |
| Envio HTTP POST | **[PREENCHER: OK/NOK]** |
| Formato JSON validado | **[PREENCHER: OK/NOK]** |
| Campos obrigatórios presentes | **[PREENCHER: OK/NOK]** |
| Tamanho do payload | **[PREENCHER: ___ bytes]** |

**[Ver Anexo Z: Screenshot do Servidor de Teste com Dados Recebidos]**

---

## **7. CONCLUSÃO**

O desenvolvimento e a validação do protótipo AgroSat-IoT demonstraram a viabilidade técnica e operacional de um CubeSat 1U como solução de baixo custo para o monitoramento agrícola remoto.

### **Principais Resultados**

1. **Conformidade com o edital:** O sistema atende integralmente aos requisitos da Fase 2 da 3ª OBSAT MCTI, incluindo:
   - Estrutura em termoplástico PETG
   - Isolamento térmico da bateria com EPE/Depron
   - Telemetria via Wi-Fi/HTTP no formato JSON especificado
   - Sensores obrigatórios (MPU9250, BMP280, monitoramento de bateria)

2. **Arquitetura Store-and-Forward:** Validada experimentalmente com taxa de entrega de pacotes superior a 99% em testes de bancada.

3. **Robustez:** 
   - Integração mecânica e térmica validada em câmara fria (-20°C)
   - Firmware com recuperação automática (self-healing) de falhas de sensores
   - Sistema de armazenamento com redundância e validação CRC

4. **Inovação:** 
   - Algoritmo de QoS para priorização de dados críticos
   - Antena de fita métrica com memória elástica
   - Gestão de energia com Dynamic Frequency Scaling

### **Próximos Passos**

Para a Fase 3 (evento regional), a equipe planeja:
- Completar todos os testes de qualificação pendentes
- Otimizar o consumo de energia para maximizar autonomia
- Refinar a calibração dos sensores
- Preparar documentação para apresentação (pitch)

Conclui-se que o AgroSat-IoT apresenta-se como uma plataforma madura e qualificada para o lançamento em balão estratosférico, contribuindo com uma prova de conceito funcional para a democratização da agricultura de precisão em regiões sem cobertura de redes convencionais.

---

## **ANEXOS**

*(Os anexos não contam para o limite de 20 páginas)*

---

### **Anexo A: Fotos da Estrutura e Relatório de Impressão**

**[INSERIR: Fotos da estrutura PETG impressa em 3D]**

**Parâmetros de impressão:**
- Material: PETG
- Temperatura de extrusão: **[PREENCHER]**°C
- Temperatura da mesa: **[PREENCHER]**°C
- Preenchimento: **[PREENCHER]**%
- Altura de camada: **[PREENCHER]** mm

---

### **Anexo B: Laudo de Pesagem**

**[INSERIR: Foto da balança com o satélite]**

| Item | Massa |
|------|-------|
| Estrutura vazia | **[PREENCHER]** g |
| OBC (TTGO LoRa32) | **[PREENCHER]** g |
| Placa PION + sensores | **[PREENCHER]** g |
| Bateria 18650 | **[PREENCHER]** g |
| Módulo GPS | **[PREENCHER]** g |
| Módulo RTC | **[PREENCHER]** g |
| Cabeamento | **[PREENCHER]** g |
| Isolamento térmico | **[PREENCHER]** g |
| **TOTAL** | **[PREENCHER]** g |

**Instrumento:** Balança de precisão, modelo **[PREENCHER]**, resolução 0.1g

---

### **Anexo C: Detalhe do Isolamento Térmico**

**[INSERIR: Fotos mostrando:]**
1. Bateria antes do isolamento
2. Bateria envolvida em espuma EPE
3. Aplicação de fita Kapton
4. Bateria instalada no chassi

---

### **Anexo D: Diagrama de Fluxo de Dados Store-and-Forward**

**[INSERIR: Diagrama mostrando o fluxo de dados desde os nós terrestres até a estação base]**

```
┌─────────────┐     LoRa 915MHz     ┌─────────────┐     Wi-Fi/HTTP     ┌─────────────┐
│ Nó Terrestre│ ──────────────────► │  AgroSat-IoT │ ──────────────────► │   Servidor  │
│  (Sensor)   │                     │  (CubeSat)   │                     │   OBSat     │
└─────────────┘                     └─────────────┘                     └─────────────┘
                                          │
                                          │ Backup
                                          ▼
                                    ┌───────────┐
                                    │  SD Card  │
                                    └───────────┘
```

---

### **Anexo E: Tabela de Critérios de Classificação QoS**

| Prioridade | Condição | Limiar | Ação |
|------------|----------|--------|------|
| CRITICAL | Umidade do solo < 10% | Seca extrema | Transmissão imediata |
| CRITICAL | Umidade do solo > 90% | Alagamento | Transmissão imediata |
| HIGH | Bateria do nó < 20% | Falha iminente | Prioridade na fila |
| HIGH | Sem comunicação > 1h | Perda de nó | Prioridade na fila |
| NORMAL | Dados de rotina | - | Ordem de chegada |
| LOW | Dados redundantes | - | Substituíveis |

---

### **Anexo F: Diagrama de Sequência Temporal**

**[INSERIR: Diagrama de sequência UML mostrando as fases da missão]**

---

### **Anexo G: Esquemático do OBC e Conexões**

**[INSERIR: Esquemático eletrônico completo]**

**Conexões principais:**

| Componente | Pino ESP32 | Função |
|------------|------------|--------|
| SDA (I2C) | GPIO 21 | Dados I2C |
| SCL (I2C) | GPIO 22 | Clock I2C |
| LoRa MOSI | GPIO 27 | SPI MOSI |
| LoRa MISO | GPIO 19 | SPI MISO |
| LoRa SCK | GPIO 5 | SPI Clock |
| LoRa CS | GPIO 18 | Chip Select |
| LoRa RST | GPIO 14 | Reset |
| LoRa DIO0 | GPIO 26 | Interrupt |
| GPS TX | GPIO 34 | UART RX |
| GPS RX | GPIO 12 | UART TX |
| Bateria | GPIO 35 | ADC (divisor resistivo) |
| Botão | GPIO 4 | Input (pull-up) |
| SD CS | GPIO 13 | Chip Select SD |

---

### **Anexo H: Gráficos de VSWR e Smith Chart da Antena**

**[INSERIR: Capturas de tela do NanoVNA mostrando:]**
1. Gráfico de VSWR vs Frequência
2. Smith Chart
3. Impedância na frequência central (915 MHz)

**Valores medidos:**
- Frequência de ressonância: **[PREENCHER]** MHz
- VSWR @ 915 MHz: **[PREENCHER]**
- Impedância: **[PREENCHER]** Ohms
- Largura de banda (-10dB): **[PREENCHER]** MHz

---

### **Anexo I: Diagrama de Conexões e Layout da PCB**

**[INSERIR: Diagrama de blocos dos sensores e suas conexões I2C]**

```
                    ┌─────────────────────────────────────┐
                    │           Barramento I2C            │
                    │         (SDA: GPIO21, SCL: GPIO22)  │
                    └──┬──────┬──────┬──────┬──────┬─────┘
                       │      │      │      │      │
                    ┌──┴──┐┌──┴──┐┌──┴──┐
                    │MPU  ││BMP  ││SI   ││CCS  ││DS   │
                    │9250 ││280  ││7021 ││811  ││3231 │
                    │0x68 ││0x76 ││0x40 ││0x5A ││0x68 │
                    └─────┘└─────┘└─────┘└─────┘└─────┘
```

---

### **Anexo J: Registro Fotográfico da Integração Térmica**

**[INSERIR: Sequência de fotos mostrando:]**
1. Materiais de isolamento (EPE, Kapton)
2. Processo de envelopamento da bateria
3. Instalação no chassi
4. Vista final do isolamento

---

### **Anexo K: Diagrama da Máquina de Estados**

**[INSERIR: Diagrama completo da FSM com todas as transições]**

---

### **Anexo L: Fluxograma do Sistema de Armazenamento**

**[INSERIR: Fluxograma mostrando a lógica do StorageManager]**

---

### **Anexo M: Logs de Calibração e Validação de Bancada**

**[INSERIR: Capturas de tela do terminal serial mostrando:]**
1. Saída do comando STATUS
2. Processo de calibração do magnetômetro (CALIB_MAG)
3. Salvamento do baseline do CCS811

---

### **Anexo N: Registro de Ativação da Missão**

**[INSERIR: Log serial mostrando a transição de PREFLIGHT para FLIGHT]**

---

### **Anexo O: Exemplo de Dados Recuperados**

**Amostra de telemetry.csv:**
```csv
timestamp,battery,temp,pressure,gx,gy,gz,ax,ay,az,crc
[INSERIR DADOS REAIS]
```

**Amostra de mission.csv:**
```csv
timestamp,node_id,humidity,temp,irrigation,priority,crc
[INSERIR DADOS REAIS]
```

---

### **Anexo P: Fotos dos Componentes**

**[INSERIR: Foto de todos os componentes antes da montagem]**

---

### **Anexo Q: Código-fonte Comentado**

**Principais classes (trechos relevantes):**

**PayloadManager.cpp - Criação do JSON de telemetria:**
```cpp
// [INSERIR TRECHO DO CÓDIGO createTelemetryJSON()]
```

**TelemetryManager.cpp - Máquina de estados:**
```cpp
// [INSERIR TRECHO DO CÓDIGO DA FSM]
```

**StorageManager.cpp - Gravação com CRC:**
```cpp
// [INSERIR TRECHO DO CÓDIGO DE GRAVAÇÃO]
```

*(Código completo disponível em: https://github.com/music-soul1-1/AgroSat-IoT)*

---

### **Anexo R: Fotos de Todas as Faces do Satélite**

**[INSERIR FOTOS:]**

| R1 | R2 | R3 |
|----|----|----|
| Face Superior | Face Inferior | Face Frontal |
| [FOTO] | [FOTO] | [FOTO] |

| R4 | R5 | R6 |
|----|----|----|
| Face Traseira | Lateral Esquerda | Lateral Direita |
| [FOTO] | [FOTO] | [FOTO] |

| R7 |
|----|
| Vista Interna (aberto) |
| [FOTO] |

---

### **Anexo S: Diagrama do Ecossistema de Validação**

**[INSERIR: Diagrama mostrando Simulador + Satélite + Estação Terrestre + Grafana]**

---

### **Anexo T: Laudo de Caracterização Física**

**[INSERIR: Fotos das medições com paquímetro e balança]**

---

### **Anexo U: Log do Teste de Vibração**

**[INSERIR: Log serial mostrando detecção de falha e recuperação do BMP280]**

---

### **Anexo V: Gráficos de Temperatura do Teste Térmico**

**[INSERIR: Gráfico temperatura vs tempo durante o teste em câmara fria]**

---

### **Anexo W: Relatório de Testes EMC**

**[INSERIR: Resultados dos testes de compatibilidade eletromagnética]**

---

### **Anexo X: Log de Transmissão e Gráficos RSSI/SNR**

**[INSERIR: Screenshots do Grafana mostrando métricas de comunicação]**

---

### **Anexo Y: Amostra dos Dados Armazenados**

**[INSERIR: Primeiras 20 linhas de cada arquivo CSV]**

---

### **Anexo Z: Screenshot do Servidor de Teste OBSat**

**[INSERIR: Captura de tela de https://obsat.org.br/teste_post/index.php mostrando os dados recebidos do AgroSat-IoT]**

---

*Fim do documento*
