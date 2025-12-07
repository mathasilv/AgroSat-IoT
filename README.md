# 🛰️ AgroSat-IoT: CubeSat 1U Store-and-Forward para Agricultura de Precisão

<div align="center">

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Built![ESP32](https://img.shields.io/badge/ESP32-Powere-green![License: MIT](https://img.shields.io/badge/License-MIT-yellow
**Sistema de Telemetria Orbital para Monitoramento Remoto de Cultivos em Áreas sem Conectividade**

[Arquitetura](#-arquitetura-de-software) -  [Hardware](#-especificações-de-hardware) -  [Instalação](#-instalação-e-compilação) -  [Documentação](#-documentação-técnica) -  [Equipe](#-equipe-orbitalis)

</div>

***

## 📡 Visão Geral da Missão

O **AgroSat-IoT** é um sistema embarcado para CubeSat 1U desenvolvido pela equipe **Orbitalis** (UFG) para a **3ª Olimpíada Brasileira de Satélites (OBSAT MCTI)**. A missão implementa um relay orbital operando no modelo **Store-and-Forward**, coletando dados de sensores agrícolas terrestres via LoRa e retransmitindo para estações base durante passagens orbitais.

### 🎯 Objetivo da Missão

Democratizar acesso à agricultura de precisão em regiões rurais remotas do Brasil, permitindo monitoramento contínuo de variáveis críticas (umidade do solo, temperatura, qualidade do ar) independente de infraestrutura terrestre convencional.

### ✨ Capacidades Principais

- **🌍 Store-and-Forward Orbital**: Recepção, armazenamento e retransmissão de dados de múltiplos nós terrestres
- **📊 Telemetria Científica**: Coleta de dados ambientais (IMU 9-DOF, pressão barométrica, temperatura, umidade, gases)
- **🔋 Gerenciamento de Energia**: Modos operacionais otimizados (PREFLIGHT/FLIGHT/SAFE) com controle dinâmico de frequência
- **🛡️ Robustez e Recuperação**: Watchdog Timer, monitoramento de heap, persistência de estado em NVS
- **💾 Armazenamento Robusto**: Logs estruturados em SD Card com rotação automática por tamanho

***

## 🏗️ Arquitetura de Software

Arquitetura modular orientada a objetos em **C++11/14** seguindo padrão de **Gerenciadores de Serviço**, executando sobre **FreeRTOS** (ESP-IDF + Arduino Framework).

```
AgroSat-IoT/
├── src/
│   ├── app/              # Lógica de missão e controle de estados
│   │   ├── TelemetryManager       → Orquestrador de modos operacionais
│   │   ├── MissionController      → Estado da missão e sincronização
│   │   ├── GroundNodeManager      → Buffer e priorização de nós terrestres
│   │   └── TelemetryCollector     → Agregador de dados científicos
│   │
│   ├── comm/             # Camadas de comunicação e protocolos
│   │   ├── CommunicationManager   → Abstração unificada de links
│   │   ├── LoRaService           → Driver LoRa SX1276 (915 MHz)
│   │   ├── WiFiService           → Cliente WiFi ESP32
│   │   ├── HttpService           → Client HTTP (OBSAT API)
│   │   └── PayloadManager        → Codificação binária/JSON
│   │
│   ├── core/             # Serviços de sistema e baixo nível
│   │   ├── RTCManager            → DS3231 + NTP sync
│   │   ├── PowerManager          → ADC bateria + DFS (Dynamic Freq Scaling)
│   │   ├── SystemHealth          → Watchdog + Heap monitor + NVS state
│   │   └── ButtonHandler         → Controle físico (GPIO4)
│   │
│   ├── sensors/          # Abstração de sensores I2C/SPI
│   │   ├── SensorManager         → Mutex I2C + recuperação de falhas
│   │   ├── MPU9250Manager        → IMU 9-DOF + calibração magnética
│   │   ├── BMP280Manager         → Altímetro/Temperatura
│   │   ├── SI7021Manager         → Umidade relativa/Temperatura
│   │   └── CCS811Manager         → eCO2/TVOC (Clock stretching fix)
│   │
│   └── storage/          # Persistência de dados
│       └── StorageManager        → SD Card SPI + CSV logging
│
├── include/
│   └── config.h          # Pinout e constantes de missão
│
└── platformio.ini        # Build config + dependências
```

### 🔄 Fluxo de Dados Store-and-Forward

```mermaid
graph LR
    A[Nó Terrestre] -->|LoRa Uplink| B[CubeSat]
    B -->|Validação CRC| C[PayloadManager]
    C -->|Decodificação| D[GroundNodeManager]
    D -->|Buffer RAM + SD| E[Priorização]
    E -->|Janela Orbital| F[Downlink para GS]
    F -->|HTTP/LoRa| G[Estação Base]
```

***

## ⚙️ Especificações de Hardware

### Plataforma Principal
- **MCU**: ESP32 (Xtensa LX6 Dual-Core @ 240 MHz)
- **Board**: TTGO LoRa32 V2.1 (16MB Flash)
- **Transceptor**: SX1276 LoRa (915 MHz, SF7-12, BW 125 kHz)

### Sensores Científicos

| Sensor | Função | Interface | Endereço I2C |
|--------|--------|-----------|--------------|
| **MPU9250** | IMU 9-DOF (Accel + Gyro + Mag) | I2C | `0x68` |
| **BMP280** | Pressão barométrica + Temperatura | I2C | `0x76` |
| **SI7021** | Umidade relativa + Temperatura | I2C | `0x40` |
| **CCS811** | eCO2 + TVOC (Qualidade do ar) | I2C | `0x5A` |
| **DS3231** | RTC (Real-Time Clock) | I2C | `0x68` |
| **NEO-M8N** | GPS/GNSS | UART | - |

### Pinout Crítico (TTGO LoRa32 V2.1)

```cpp
// LoRa SX1276 (SPI)
#define LORA_SCK      5
#define LORA_MISO     19
#define LORA_MOSI     27
#define LORA_CS       18
#define LORA_RST      23
#define LORA_DIO0     26

// SD Card (SPI)
#define SD_CS         13
#define SD_MOSI       15
#define SD_MISO       2
#define SD_SCLK       14

// I2C Sensors
#define I2C_SDA       21
#define I2C_SCL       22
#define I2C_FREQ      50000  // 50 kHz (Clock stretching CCS811)

// Power Monitor
#define BATTERY_PIN   35     // ADC1_CH7
```

### Correção I2C para CCS811

O sensor de qualidade do ar **CCS811** requer **clock stretching** prolongado. Configuração aplicada:

```cpp
Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
Wire.setTimeout(3000);  // 3s timeout
Wire.setBufferSize(512);
```

***

## 🎮 Modos Operacionais

Sistema de estados finitos com persistência em NVS (Non-Volatile Storage) para recuperação pós-reset.

| Modo | Condição de Ativação | Log Serial | LoRa TX | HTTP TX | CPU Freq | Intervalo SD |
|------|---------------------|------------|---------|---------|----------|--------------|
| **PREFLIGHT** | Boot padrão ou comando manual | ✅ | ✅ | ✅ | 240 MHz | 10s |
| **FLIGHT** | Comando remoto ou timer | ❌ | ✅ | ✅ | 240 MHz | 10s |
| **SAFE** | Bateria <3.3V ou Heap <20KB | ✅ | Beacon (300s) | ❌ | 80 MHz | 300s |

### Transições de Estado

```cpp
PREFLIGHT → FLIGHT  // Via comando serial "START_MISSION"
FLIGHT → SAFE       // Auto: Bateria crítica ou erro fatal
SAFE → PREFLIGHT    // Reset manual + bateria restaurada
```

***

## 📦 Instalação e Compilação

### Pré-requisitos

- **Visual Studio Code** + **PlatformIO IDE Extension**
- **Python 3.7+** (para build tools)
- **Driver USB-Serial**: CP2104 (TTGO LoRa32)

### Dependências (Auto-instaladas via `platformio.ini`)

```ini
sandeepmistry/LoRa @ ^0.8.0
adafruit/Adafruit BusIO @ ^1.16.2
adafruit/RTClib @ ^2.1.4
bblanchon/ArduinoJson @ ^7.2.1
mikalhart/TinyGPSPlus @ ^1.1.0
adafruit/Adafruit MPU6050 @ ^2.2.6
adafruit/Adafruit BMP280 Library @ ^2.6.8
adafruit/Adafruit Si7021 Library @ ^1.6.1
adafruit/Adafruit CCS811 Library @ ^1.1.3
```

### Build e Upload

1. **Clone o repositório**:
   ```bash
   git clone https://github.com/mathasilv/AgroSat-IoT.git
   cd AgroSat-IoT
   git checkout feature/refactor-structure
   ```

2. **Compile o projeto**:
   ```bash
   pio run -e ttgo-lora32-v21
   ```

3. **Upload para o ESP32**:
   ```bash
   pio run -e ttgo-lora32-v21 -t upload
   ```

4. **Monitor Serial** (115200 baud):
   ```bash
   pio device monitor
   ```

***

## 🔧 Comandos do Console Serial

Interface de comandos disponível nos modos PREFLIGHT e SAFE:

| Comando | Função | Exemplo |
|---------|--------|---------|
| `STATUS` | Exibe estado de todos os sensores (online/offline + leituras) | `STATUS` |
| `CALIB_MAG` | Inicia calibração do magnetômetro (rotacionar CubeSat 360°) | `CALIB_MAG` |
| `CLEAR_MAG` | Apaga offsets de calibração do magnetômetro (NVS) | `CLEAR_MAG` |
| `SAVE_BASELINE` | Salva baseline CCS811 (executar em ar puro 20min) | `SAVE_BASELINE` |
| `START_MISSION` | Transição PREFLIGHT → FLIGHT | `START_MISSION` |
| `SAFE_MODE` | Força entrada no modo SAFE | `SAFE_MODE` |
| `HELP` | Lista comandos disponíveis | `HELP` |

***

## 📡 Protocolos de Comunicação

### LoRa (Satélite ↔ Terra)

**Configuração de Rádio**:
```cpp
Frequência: 915 MHz
Spreading Factor: SF7 (alta taxa) / SF12 (longo alcance)
Bandwidth: 125 kHz
Coding Rate: 4/5
Potência: 20 dBm (máx)
```

**Estrutura de Pacote Binário** (Store-and-Forward):

```
[Header: 2B] [Team ID: 2B] [Timestamp: 4B] [Payload: NB] [CRC: 2B]
```

### HTTP/JSON (Backup/Testes)

Endpoint OBSAT API (quando WiFi disponível):

```json
POST /api/telemetry
{
  "team_id": 1234,
  "timestamp": "2025-12-07T03:00:00Z",
  "cubesat": {
    "battery_voltage": 3.85,
    "temperature": 22.5,
    "position": {"lat": -16.6869, "lon": -49.2648}
  },
  "ground_nodes": [
    {
      "node_id": "GN001",
      "soil_moisture": 45.2,
      "air_temp": 28.7,
      "rssi": -87,
      "received_at": "2025-12-07T02:58:12Z"
    }
  ]
}
```

***

## 📊 Formato de Telemetria

### Dados Científicos Coletados

**CubeSat (Taxa: 10s PREFLIGHT/FLIGHT, 300s SAFE)**:
- Tensão da bateria (V)
- Atitude (roll/pitch/yaw) do IMU
- Posição GPS (lat/lon/altitude)
- Temperatura interna (°C)
- Pressão atmosférica (hPa)
- Umidade relativa (%)
- Qualidade do ar (eCO2 ppm, TVOC ppb)

**Nós Terrestres (Store-and-Forward)**:
- ID do nó
- Umidade do solo (%)
- Temperatura ambiente (°C)
- RSSI/SNR do link LoRa
- Timestamp de geração/recepção/retransmissão

### Armazenamento em SD Card

Estrutura de arquivos CSV com rotação automática (10MB):

```
/telemetry/
  ├── telemetry_001.csv      # Dados do CubeSat
  ├── telemetry_002.csv
  ├── ground_nodes_001.csv   # Dados Store-and-Forward
  └── system_log_001.csv     # Logs de sistema
```

***

## 🛡️ Robustez e Recuperação de Falhas

### Watchdog Timer (WDT)

- **Timeout**: 8 segundos
- **Reset Automático**: Trava de tarefas FreeRTOS
- **Persistência**: Contador de resets em NVS

### Monitoramento de Heap

```cpp
if (ESP.getFreeHeap() < 20 * 1024) {  // < 20 KB
    enterSafeMode();
}
```

### Persistência de Estado (NVS)

Dados salvos em memória não-volátil:
- Modo operacional atual
- Offsets de calibração de sensores
- Contador de missões
- Baseline CCS811
- Estatísticas de uptime

***

## 📚 Documentação Técnica

### Arquivos de Configuração

- [`platformio.ini`](https://github.com/mathasilv/AgroSat-IoT/blob/feature/refactor-structure/platformio.ini) - Build config + libs
- [`include/config.h`](https://github.com/mathasilv/AgroSat-IoT/blob/feature/refactor-structure/include/config.h) - Pinout + constantes

### Estrutura Detalhada

```
src/
├── app/
│   ├── TelemetryManager.cpp       # Orquestrador principal
│   ├── MissionController.cpp      # FSM (Finite State Machine)
│   ├── GroundNodeManager.cpp      # Buffer circular + priorização
│   └── TelemetryCollector.cpp     # Agregador de dados
├── comm/
│   ├── CommunicationManager.cpp   # Multiplexador de links
│   ├── LoRaService.cpp            # Driver SX1276
│   └── PayloadManager.cpp         # Serialização binária
├── core/
│   ├── RTCManager.cpp             # DS3231 + NTP
│   ├── PowerManager.cpp           # ADC + histerese
│   └── SystemHealth.cpp           # WDT + Heap + NVS
└── sensors/
    ├── SensorManager.cpp          # Mutex I2C
    └── [drivers específicos]
```

***

## 🤝 Contribuindo

Contribuições são bem-vindas! Siga o fluxo padrão de contribuição do GitHub:

1. **Fork** este repositório
2. Crie uma **feature branch**: `git checkout -b feature/nova-funcionalidade`
3. **Commit** suas mudanças: `git commit -m 'Add: Nova funcionalidade X'`
4. **Push** para a branch: `git push origin feature/nova-funcionalidade`
5. Abra um **Pull Request**

### Diretrizes de Código

- Use **C++11/14** para compatibilidade ESP32
- Evite alocação dinâmica (`malloc/new`) quando possível
- Documente código complexo ou hardware-specific
- Teste em hardware real (TTGO LoRa32) antes do PR
- Mantenha consistência com padrão de Gerenciadores existente

***

## 👨‍🚀 Equipe Orbitalis

**Categoria N3 - 3ª Olimpíada Brasileira de Satélites (OBSAT MCTI)**

- **Instituição**: Universidade Federal de Goiás (UFG)
- **Desenvolvedores**:
  - Matheus Aparecido Souza Silva - Firmware Lead
  - Luana Sthephany Rodrigues Mamed - Hardware & Integration
- **Tutor**: Prof. Aldo Diaz
- **Organização**: Ministério da Ciência, Tecnologia e Inovação (MCTI)

***

## 📄 Licença

Este projeto está licenciado sob a **MIT License** - veja o arquivo [LICENSE](LICENSE) para detalhes.

***

## 🙏 Agradecimentos

- **OBSAT MCTI** pelo suporte à competição
- **PlatformIO** pela plataforma de desenvolvimento robusta
- **Espressif Systems** pela arquitetura ESP32
- **LoRa Alliance** pelos padrões de comunicação
- Comunidade open-source de sistemas embarcados aeroespaciais

***

<div align="center">

**🛰️ Desenvolvido com ❤️ para democratizar acesso à agricultura de precisão no Brasil**

[![GitHub](https://img.shields.io/badge/GitHub-mathasilv%
[![OBSAT](https://img.shields.io/badge/Website-OBSAT-re</div>
