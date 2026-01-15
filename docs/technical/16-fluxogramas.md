# Documentação Técnica AgroSat-IoT

## Parte 16: Fluxogramas e Diagramas

### 16.1 Visão Geral

Esta seção apresenta diagramas visuais do sistema AgroSat-IoT para facilitar a compreensão da arquitetura e fluxos de dados.

### 16.2 Arquitetura Geral do Sistema

```mermaid
graph TB
    subgraph "Hardware"
        ESP32[ESP32-PICO-D4]
        LORA_HW[SX1276 LoRa]
        SD_HW[SD Card]
        GPS_HW[NEO-6M GPS]
        IMU[MPU9250]
        BARO[BMP280]
        HUM[SI7021]
        AIR[CCS811]
        RTC_HW[DS3231]
        BAT[Battery ADC]
        BTN[Button]
        LED[LED]
    end
    
    subgraph "Drivers"
        LORA_DRV[LoRaService]
        SD_DRV[SD Library]
        GPS_DRV[GPSManager]
        SENS_DRV[SensorManager]
        RTC_DRV[RTCManager]
        PWR_DRV[PowerManager]
    end
    
    subgraph "Serviços"
        COMM[CommunicationManager]
        STOR[StorageManager]
        HEALTH[SystemHealth]
    end
    
    subgraph "Aplicação"
        TM[TelemetryManager]
        MC[MissionController]
        GNM[GroundNodeManager]
        TC[TelemetryCollector]
        CMD[CommandHandler]
    end
    
    ESP32 --> LORA_HW
    ESP32 --> SD_HW
    ESP32 --> GPS_HW
    ESP32 --> IMU
    ESP32 --> BARO
    ESP32 --> HUM
    ESP32 --> AIR
    ESP32 --> RTC_HW
    ESP32 --> BAT
    ESP32 --> BTN
    ESP32 --> LED
    
    LORA_HW --> LORA_DRV
    SD_HW --> SD_DRV
    GPS_HW --> GPS_DRV
    IMU --> SENS_DRV
    BARO --> SENS_DRV
    HUM --> SENS_DRV
    AIR --> SENS_DRV
    RTC_HW --> RTC_DRV
    BAT --> PWR_DRV
    
    LORA_DRV --> COMM
    SD_DRV --> STOR
    GPS_DRV --> TC
    SENS_DRV --> TC
    RTC_DRV --> MC
    PWR_DRV --> TM
    
    COMM --> TM
    STOR --> TM
    HEALTH --> TM
    TC --> TM
    MC --> TM
    GNM --> TM
    CMD --> TM
```

### 16.3 Fluxo de Inicialização (Boot)

```mermaid
flowchart TD
    START([Power On]) --> SERIAL[Inicializa Serial 115200]
    SERIAL --> GLOBALS[Inicializa Recursos Globais]
    GLOBALS --> CHECK_RESET{Verifica Reset Reason}
    
    CHECK_RESET -->|WDT Reset| LOG_WDT[Log: Watchdog Reset]
    CHECK_RESET -->|Normal| CONTINUE
    LOG_WDT --> CONTINUE
    
    CONTINUE --> INIT_I2C[Inicializa I2C Bus]
    INIT_I2C --> INIT_SPI[Inicializa SPI Bus]
    
    INIT_SPI --> INIT_SENSORS[Inicializa Sensores]
    INIT_SENSORS --> SENS_OK{Sensores OK?}
    SENS_OK -->|Não| SENS_WARN[Warning: Sensores offline]
    SENS_OK -->|Sim| INIT_GPS
    SENS_WARN --> INIT_GPS
    
    INIT_GPS[Inicializa GPS] --> INIT_LORA[Inicializa LoRa]
    INIT_LORA --> LORA_OK{LoRa OK?}
    LORA_OK -->|Não| LORA_ERR[ERRO CRÍTICO]
    LORA_OK -->|Sim| INIT_SD
    LORA_ERR --> SAFE_MODE
    
    INIT_SD[Inicializa SD Card] --> SD_OK{SD OK?}
    SD_OK -->|Não| SD_WARN[Warning: SD offline]
    SD_OK -->|Sim| INIT_WIFI
    SD_WARN --> INIT_WIFI
    
    INIT_WIFI[Inicializa WiFi] --> INIT_TASKS[Cria Tasks FreeRTOS]
    INIT_TASKS --> INIT_WDT[Configura Watchdog]
    INIT_WDT --> LOAD_STATE[Carrega Estado da NVS]
    
    LOAD_STATE --> MISSION_ACTIVE{Missão Ativa?}
    MISSION_ACTIVE -->|Sim| RESUME[Resume Missão]
    MISSION_ACTIVE -->|Não| PREFLIGHT
    
    RESUME --> FLIGHT_MODE[Modo FLIGHT]
    PREFLIGHT[Modo PREFLIGHT] --> READY
    FLIGHT_MODE --> READY
    
    SAFE_MODE[Modo SAFE] --> READY
    READY([Sistema Pronto]) --> MAIN_LOOP[Entra no Loop Principal]
```

### 16.4 Loop Principal

```mermaid
flowchart TD
    LOOP_START([Loop Start]) --> FEED_WDT[Feed Watchdog]
    FEED_WDT --> CHECK_BTN[Verifica Botão]
    
    CHECK_BTN --> BTN_EVT{Evento?}
    BTN_EVT -->|Short Press| TOGGLE_MISSION[Toggle Missão]
    BTN_EVT -->|Long Press| FORCE_SAFE[Força SAFE Mode]
    BTN_EVT -->|None| CHECK_SERIAL
    TOGGLE_MISSION --> CHECK_SERIAL
    FORCE_SAFE --> CHECK_SERIAL
    
    CHECK_SERIAL[Verifica Serial] --> SERIAL_DATA{Dados?}
    SERIAL_DATA -->|Sim| PROCESS_CMD[Processa Comando]
    SERIAL_DATA -->|Não| CHECK_LORA
    PROCESS_CMD --> CHECK_LORA
    
    CHECK_LORA[Verifica LoRa RX] --> LORA_DATA{Pacote?}
    LORA_DATA -->|Sim| PROCESS_LORA[Processa Ground Node]
    LORA_DATA -->|Não| CHECK_TIMERS
    PROCESS_LORA --> CHECK_TIMERS
    
    CHECK_TIMERS[Verifica Timers] --> TELEM_TIME{Hora Telemetria?}
    TELEM_TIME -->|Sim| COLLECT_DATA[Coleta Dados]
    TELEM_TIME -->|Não| STORAGE_TIME
    
    COLLECT_DATA --> SEND_LORA[Envia LoRa]
    SEND_LORA --> SEND_HTTP[Envia HTTP Async]
    SEND_HTTP --> STORAGE_TIME
    
    STORAGE_TIME{Hora Storage?} -->|Sim| SAVE_SD[Salva SD Async]
    STORAGE_TIME -->|Não| HEALTH_TIME
    SAVE_SD --> HEALTH_TIME
    
    HEALTH_TIME{Hora Health?} -->|Sim| CHECK_HEALTH[Verifica Saúde]
    HEALTH_TIME -->|Não| UPDATE_GPS
    CHECK_HEALTH --> HEALTH_OK{OK?}
    HEALTH_OK -->|Não| HANDLE_ERROR[Trata Erro]
    HEALTH_OK -->|Sim| UPDATE_GPS
    HANDLE_ERROR --> UPDATE_GPS
    
    UPDATE_GPS[Atualiza GPS] --> PRUNE_NODES[Remove Nós Expirados]
    PRUNE_NODES --> LOOP_END([Loop End])
    LOOP_END --> LOOP_START
```

### 16.5 Fluxo de Coleta de Telemetria

```mermaid
sequenceDiagram
    participant TM as TelemetryManager
    participant TC as TelemetryCollector
    participant SM as SensorManager
    participant GPS as GPSManager
    participant PM as PowerManager
    participant DATA as TelemetryData
    
    TM->>TC: collectTelemetry()
    
    TC->>PM: readVoltage()
    PM-->>TC: voltage
    TC->>PM: readPercentage()
    PM-->>TC: percentage
    
    TC->>SM: readAll(data)
    
    Note over SM: Adquire I2C Mutex
    
    SM->>SM: readIMU()
    SM->>SM: readBarometer()
    SM->>SM: readHumidity()
    SM->>SM: readAirQuality()
    
    Note over SM: Libera I2C Mutex
    
    SM-->>TC: sensor data
    
    TC->>GPS: read()
    GPS-->>TC: position data
    
    TC->>TC: fusionTemperature()
    TC->>TC: calculateAltitude()
    TC->>TC: validateData()
    
    TC->>DATA: populate struct
    TC-->>TM: TelemetryData
```

### 16.6 Fluxo de Transmissão LoRa

```mermaid
sequenceDiagram
    participant TM as TelemetryManager
    participant CM as CommunicationManager
    participant DC as DutyCycleTracker
    participant PM as PayloadManager
    participant LORA as LoRaService
    
    TM->>CM: sendLoRa(data)
    
    CM->>PM: buildPayload(data)
    PM-->>CM: payload string
    
    CM->>DC: canTransmit(payloadSize)
    
    alt Duty Cycle OK
        DC-->>CM: true
        CM->>LORA: send(payload)
        
        Note over LORA: beginPacket()
        Note over LORA: write(payload)
        Note over LORA: endPacket()
        
        LORA-->>CM: success
        CM->>DC: recordTransmission(ToA)
        CM-->>TM: true
    else Duty Cycle Exceeded
        DC-->>CM: false
        CM-->>TM: false (duty cycle)
    end
```

### 16.7 Fluxo de Recepção de Ground Node

```mermaid
sequenceDiagram
    participant ISR as LoRa ISR
    participant LORA as LoRaService
    participant CM as CommunicationManager
    participant GNM as GroundNodeManager
    participant TM as TelemetryManager
    
    Note over ISR: DIO0 Interrupt
    ISR->>ISR: xSemaphoreGiveFromISR()
    
    Note over LORA: Task aguarda semáforo
    LORA->>LORA: parsePacket()
    LORA->>LORA: readBytes()
    LORA->>LORA: getRSSI(), getSNR()
    
    LORA->>CM: notifyReceive(data, rssi, snr)
    
    CM->>CM: parseGroundNodePacket()
    CM->>CM: validateChecksum()
    
    alt Pacote Válido
        CM->>GNM: addOrUpdateNode(missionData)
        GNM->>GNM: calculatePriority()
        GNM->>GNM: updateStatistics()
        GNM-->>CM: success
        CM->>TM: notifyNewNode()
    else Pacote Inválido
        CM->>CM: logError()
    end
```

### 16.8 Fluxo de Armazenamento Assíncrono

```mermaid
sequenceDiagram
    participant TM as TelemetryManager
    participant QUEUE as xStorageQueue
    participant TASK as StorageTask
    participant SM as StorageManager
    participant SD as SD Card
    
    TM->>TM: prepareStorageData()
    TM->>QUEUE: xQueueSend(signal)
    
    Note over TM: Continua execução
    
    Note over TASK: Task bloqueada em xQueueReceive
    QUEUE-->>TASK: signal received
    
    TASK->>SM: getCurrentData()
    SM-->>TASK: data copy
    
    TASK->>SM: saveTelemetry(data)
    
    SM->>SD: open("/telemetry.csv", APPEND)
    SM->>SD: write(csvLine)
    SM->>SD: flush()
    SM->>SD: close()
    
    SD-->>SM: success
    SM-->>TASK: success
    
    Note over TASK: Volta a aguardar na fila
```

### 16.9 Máquina de Estados - Modos de Operação

```mermaid
stateDiagram-v2
    [*] --> INIT: Power On
    
    INIT --> PREFLIGHT: Init OK
    INIT --> SAFE: Init Failed
    
    PREFLIGHT --> FLIGHT: START_MISSION\n(Serial/Botão)
    PREFLIGHT --> SAFE: Erro Crítico
    
    FLIGHT --> PREFLIGHT: STOP_MISSION\n(Serial/Botão)
    FLIGHT --> SAFE: Bateria Crítica\n(<3.3V)
    FLIGHT --> SAFE: Erro Crítico
    FLIGHT --> SAFE: Botão Longo\n(>2s)
    
    SAFE --> PREFLIGHT: Recuperação\n(Manual)
    SAFE --> SAFE: Beacon\n(3 min)
    
    note right of PREFLIGHT
        - Logs habilitados
        - Telemetria 20s
        - Storage 1s
    end note
    
    note right of FLIGHT
        - Logs desabilitados
        - Telemetria 60s
        - Storage 10s
    end note
    
    note right of SAFE
        - Mínimo consumo
        - Telemetria 120s
        - Beacon 180s
    end note
```

### 16.10 Diagrama de Tasks FreeRTOS

```mermaid
graph TB
    subgraph "Core 0"
        MAIN[Loop Principal<br/>Prioridade: 1]
        HTTP[HttpTask<br/>Prioridade: 1<br/>Stack: 8KB]
        STOR[StorageTask<br/>Prioridade: 1<br/>Stack: 8KB]
    end
    
    subgraph "Core 1"
        SENS[SensorsTask<br/>Prioridade: 2<br/>Stack: 4KB]
    end
    
    subgraph "Recursos Compartilhados"
        SERIAL_M[xSerialMutex]
        I2C_M[xI2CMutex]
        DATA_M[xDataMutex]
        LORA_S[xLoRaRxSemaphore]
        HTTP_Q[xHttpQueue<br/>5 itens]
        STOR_Q[xStorageQueue<br/>10 itens]
    end
    
    MAIN -.->|Envia| HTTP_Q
    MAIN -.->|Envia| STOR_Q
    HTTP -.->|Recebe| HTTP_Q
    STOR -.->|Recebe| STOR_Q
    
    MAIN -.->|Usa| SERIAL_M
    MAIN -.->|Usa| DATA_M
    SENS -.->|Usa| I2C_M
    SENS -.->|Usa| DATA_M
    
    MAIN -.->|Aguarda| LORA_S
```

### 16.11 Fluxo de Dados Completo

```mermaid
flowchart LR
    subgraph "Entrada"
        SENS_IN[Sensores I2C]
        GPS_IN[GPS UART]
        LORA_IN[LoRa RX]
        BTN_IN[Botão]
        SERIAL_IN[Serial RX]
    end
    
    subgraph "Processamento"
        TC[TelemetryCollector]
        GNM[GroundNodeManager]
        CMD[CommandHandler]
        TM[TelemetryManager]
    end
    
    subgraph "Armazenamento"
        RAM[RAM Buffers]
        SD[SD Card]
        NVS[NVS Flash]
    end
    
    subgraph "Saída"
        LORA_OUT[LoRa TX]
        HTTP_OUT[HTTP POST]
        SERIAL_OUT[Serial TX]
        LED_OUT[LED]
    end
    
    SENS_IN --> TC
    GPS_IN --> TC
    TC --> TM
    
    LORA_IN --> GNM
    GNM --> TM
    
    BTN_IN --> TM
    SERIAL_IN --> CMD
    CMD --> TM
    
    TM --> RAM
    TM --> SD
    TM --> NVS
    
    TM --> LORA_OUT
    TM --> HTTP_OUT
    TM --> SERIAL_OUT
    TM --> LED_OUT
```

### 16.12 Diagrama de Conexões Físicas

```
┌─────────────────────────────────────────────────────────────────┐
│                    LilyGo T3 V1.6.1 (ESP32)                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐   │
│  │ MPU9250 │     │ BMP280  │     │ SI7021  │     │ CCS811  │   │
│  │  0x69   │     │  0x76   │     │  0x40   │     │  0x5A   │   │
│  └────┬────┘     └────┬────┘   │
│       │               │               │               │         │
│       └───────────────┴───────────────┴───────────────┘         │
│                           │                                     │
│                      I2C Bus                                    │
│                   SDA=21, SCL=22                                │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐                              ┌─────────┐           │
│  │ NEO-6M  │                              │ DS3231  │           │
│  │  GPS    │                              │  RTC    │           │
│  └────┬────┘                              └────┬────┘           │
│       │                                        │                │
│   UART2                                    I2C 0x68             │
│   RX=34, TX=12                                                  │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐                              ┌─────────┐           │
│  │ SX1276  │                              │ SD Card │           │
│  │  LoRa   │                              │         │           │
│  └────┬────┘                              └────┬────┘           │
│       │                                        │                │
│   VSPI                                     HSPI                 │
│   CS=18, RST=23, DIO0=26                   CS=13                │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐                   │
│  │ Battery │     │  Button │     │   LED   │                   │
│  │  ADC    │     │         │     │         │                   │
│  └────┬────┘     └────┬────┘     └────┬────┘                   │
│       │               │               │                         │
│    GPIO35          GPIO4           GPIO25                       │
│                                                                 │
└─────────────────────────────────┘
```

### 16.13 Formato de Pacote LoRa

```
┌────────────────────────────────────────────────────────────────┐
│                    Pacote LoRa (max 255 bytes)                 │
├────────┬────────┬────────┬─────────────────────┬───────────────┤
│ Header │ TeamID │ SeqNum │      Payload        │   Checksum    │
│ 1 byte │ 2 bytes│ 2 bytes│    N bytes          │    2 bytes    │
├────────┼────────┼────────┼─────────────────────┼───────────────┤
│  0xAA  │  0x029A│  0x0001│  CSV ou Binary      │    CRC16      │
└────────┴────────┴────────┴─────────────────────┴───────────────┘

Header: 0xAA = Telemetria, 0xBB = Ground Node, 0xCC = Beacon
TeamID: 666 (0x029A)
SeqNum: Número sequencial (0-65535)
Payload: Dados em formato CSV ou binário
Checksum: CRC16-CCITT
```

### 16.14 Timeline de Operação Típica

```
Tempo (s)    0    10    20    30    40    50    60    70    80
            │     │     │     │     │     │     │     │     │
PREFLIGHT   ├─────┼─────┼─────┼─────┤
            │  T  │  T  │  T  │     │
            │  S  │  S  │  S  │     │
            │     │     │     │     │
FLIGHT      │     │     │     │     ├─────┼─────┼─────┼─────┤
            │     │     │     │     │     │  T  │     │     │
            │     │     │     │     │  S  │  S  │  S  │
            │     │     │     │     │     │     │     │     │

T = Transmissão LoRa
S = Storage SD

PREFLIGHT: T cada 20s, S cada 1s
FLIGHT: T cada 60s, S cada 10s
```

### 16.15 Diagrama de Prioridade QoS

```mermaid
flowchart TD
    START([Pacote Recebido]) --> CHECK_SOIL{Solo < 20%?}
    
    CHECK_SOIL -->|Sim| CRITICAL[CRITICAL<br/>Prioridade 0]
    CHECK_SOIL -->|Não| CHECK_TEMP{Temp Extrema?}
    
    CHECK_TEMP -->|Sim| CRITICAL
    CHECK_TEMP -->|Não| CHECK_RSSI{RSSI < -110?}
    
    CHECK_RSSI -->|Sim| HIGH[HIGH_PRIORITY<br/>Prioridade 1]
    CHECK_RSSI -->|Não| CHECK_IRRIG{Irrigação Ativa?}
    
    CHECK_IRRIG -->|Sim| HIGH
    CHECK_IRRIG -->|Não| CHECK_AGE{Dados > 5min?}
    
    CHECK_AGE -->|Sim| LOW[LOW_PRIORITY<br/>Prioridade 3]
    CHECK_AGE -->|Não| NORMAL[NORMAL<br/>Prioridade 2]
    
    CRITICAL --> QUEUE[Adiciona à Fila]
    HIGH --> QUEUE
    NORMAL --> QUEUE
    LOW --> QUEUE
    
    QUEUE --> SORT[Ordena por Prioridade]
```

---

*Anterior: [15 - Referência de API](15-referencia-api.md)*

*Próxima parte: [17 - Guia de Desenvolvimento](17-guia-desenvolvimento.md)*