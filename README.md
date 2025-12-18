# AgroSat-IoT: CubeSat 1U Store-and-Forward para Agricultura de Precisão

Sistema de telemetria orbital para monitoramento remoto de cultivos em áreas sem conectividade terrestre, desenvolvido pela equipe Orbitalis (UFG) para a 3ª Olimpíada Brasileira de Satélites (OBSAT MCTI).

## Visão Geral

O AgroSat-IoT é um sistema embarcado para CubeSat 1U que implementa um relay orbital operando no modelo Store-and-Forward. O satélite coleta dados de sensores agrícolas terrestres via LoRa (915 MHz) durante passagens orbitais e retransmite as informações para estações base, democratizando o acesso à agricultura de precisão em regiões rurais remotas do Brasil.

### Objetivo da Missão

Permitir o monitoramento contínuo de variáveis agrícolas críticas (umidade do solo, temperatura, qualidade do ar) em regiões sem infraestrutura de conectividade convencional, utilizando comunicação LoRa de longo alcance através de satélite em órbita baixa terrestre (LEO).

### Capacidades Principais

- **Store-and-Forward Orbital**: Recepção, armazenamento e retransmissão de dados de múltiplos nós terrestres
- **Telemetria Científica Multi-Sensor**: IMU 9-DOF, pressão barométrica, temperatura, umidade e qualidade do ar (eCO2/TVOC)
- **Gerenciamento Inteligente de Energia**: Modos operacionais adaptativos com Dynamic Frequency Scaling (DFS)
- **Robustez e Recuperação**: Watchdog Timer, monitoramento de heap, persistência de estado em NVS (Non-Volatile Storage)
- **Comunicação LoRa Otimizada**: Link budget adaptativo com SF7-12, duty cycle compliance ANATEL
- **Criptografia AES-128**: Segurança de dados nas comunicações críticas

## Arquitetura de Software

O sistema utiliza uma arquitetura modular orientada a objetos em C++11/14, seguindo o padrão de Service Managers, executando sobre FreeRTOS (ESP-IDF + Arduino Framework).

### Estrutura de Diretórios

```
AgroSat-IoT/
├── src/
│   ├── app/                   # Lógica de missão e controle de estados
│   │   ├── TelemetryManager/      → Orquestrador principal dos modos operacionais
│   │   ├── MissionController/     → Máquina de estados e sincronização
│   │   ├── GroundNodeManager/     → Buffer e priorização de nós terrestres
│   │   └── TelemetryCollector/    → Agregador de dados científicos
│   │
│   ├── comm/                  # Camadas de comunicação e protocolos
│   │   ├── CommunicationManager/  → Abstração unificada de links
│   │   ├── LoRaService/          → Driver LoRa SX1276 (915 MHz)
│   │   ├── WiFiService/          → Cliente WiFi ESP32
│   │   ├── HttpService/          → Client HTTP/HTTPS
│   │   ├── PayloadManager/       → Codificação binária/JSON
│   │   └── CryptoManager         → Criptografia AES-128
│   │
│   ├── core/                  # Serviços de sistema e baixo nível
│   │   ├── RTCManager/           → DS3231 + NTP sync
│   │   ├── PowerManager/         → ADC bateria + DFS
│   │   ├── SystemHealth/         → Watchdog + Heap monitor + NVS
│   │   └── ButtonHandler/        → Controle físico (GPIO4)
│   │
│   ├── sensors/               # Abstração de sensores I2C/SPI
│   │   ├── SensorManager/        → Mutex I2C + recuperação de falhas
│   │   ├── MPU9250Manager/       → IMU 9-DOF + calibração magnética
│   │   ├── BMP280Manager/        → Altímetro/Temperatura
│   │   ├── SI7021Manager/        → Umidade relativa/Temperatura
│   │   └── CCS811Manager/        → eCO2/TVOC (clock stretching fix)
│   │
│   └── storage/               # Persistência de dados
│       └── StorageManager/       → SD Card SPI + CSV logging
│
├── include/
│   └── config.h               # Pinout e constantes de missão
│
└── platformio.ini             # Configuração de build e dependências
```

### Fluxo de Dados Store-and-Forward

1. **Uplink**: Nós terrestres transmitem dados agrícolas via LoRa durante janela de visibilidade orbital
2. **Validação**: CubeSat valida integridade via CRC e decodifica payload
3. **Buffer**: GroundNodeManager armazena dados em buffer RAM prioritizado
4. **Persistência**: Dados críticos salvos em SD Card com rotação automática
5. **Downlink**: Durante passagem sobre estação base, dados são retransmitidos via LoRa/HTTP

## Especificações de Hardware

### Plataforma Principal

- **MCU**: ESP32 (Xtensa LX6 Dual-Core @ 240 MHz / 80 MHz Safe Mode)
- **Memória**: 520 KB SRAM, 16 MB Flash (TTGO LoRa32 V2.1)
- **Transceptor RF**: Semtech SX1276 LoRa (915 MHz, SF7-12, BW 125 kHz)
- **Armazenamento**: MicroSD Card (SPI)

### Sensores Científicos

| Sensor | Função | Interface | Endereço I2C |
|--------|--------|-----------|--------------|
| MPU9250 | IMU 9-DOF (Aceleração + Giroscópio + Magnetômetro) | I2C | 0x69 |
| BMP280 | Pressão barométrica + Temperatura | I2C | 0x76 |
| SI7021 | Umidade relativa + Temperatura | I2C | 0x40 |
| CCS811 | Qualidade do ar (eCO2 + TVOC) | I2C | 0x5A |
| DS3231 | Real-Time Clock (RTC) | I2C | 0x68 |
| NEO-M8N | GPS/GNSS (posicionamento orbital) | UART | - |

### Pinout Crítico TTGO LoRa32 V2.1

**LoRa SX1276 (SPI)**:
- SCK: GPIO 5, MISO: GPIO 19, MOSI: GPIO 27
- CS: GPIO 18, RST: GPIO 23, DIO0: GPIO 26

**SD Card (SPI)**:
- CS: GPIO 13, MOSI: GPIO 15, MISO: GPIO 2, SCLK: GPIO 14

**I2C Sensors**:
- SDA: GPIO 21, SCL: GPIO 22
- Frequência: 100 kHz (Standard Mode)
- Timeout: 3000 ms (clock stretching CCS811)

**Power Management**:
- Battery ADC: GPIO 35 (ADC1_CH7)

**Controle**:
- Button: GPIO 4
- LED: GPIO 25

## Modos Operacionais

O sistema implementa uma máquina de estados finitos com persistência em NVS para recuperação pós-reset.

### Configuração dos Modos

| Modo | Serial Logs | LoRa TX | HTTP TX | CPU Freq | Intervalo Telemetria | Watchdog |
|------|-------------|---------|---------|----------|---------------------|----------|
| **PREFLIGHT** | Habilitado | Habilitado | Habilitado | 240 MHz | 20s | 60s |
| **FLIGHT** | Desabilitado | Habilitado | Habilitado | 240 MHz | 60s | 90s |
| **SAFE** | Habilitado | Beacon 180s | Desabilitado | 80 MHz | 120s | 180s |

### Transições de Estado

```
PREFLIGHT → FLIGHT  : Comando "START_MISSION" ou timer automático
FLIGHT → SAFE       : Bateria < 3.3V ou Heap < 20KB (automático)
SAFE → PREFLIGHT    : Reset manual + bateria restaurada (> 3.5V)
```

### Modo SAFE

Ativado automaticamente em condições críticas:
- Tensão da bateria abaixo de 3.3V
- Memória heap disponível menor que 20 KB
- Erros fatais de sistema

Comportamento em modo SAFE:
- CPU reduzida para 80 MHz (economia de energia)
- Beacon LoRa a cada 180 segundos
- Comunicação HTTP desabilitada
- Logs verbosos habilitados para diagnóstico
- Intervalo de telemetria aumentado para 120s

## Instalação e Compilação

### Pré-requisitos

- Visual Studio Code com extensão PlatformIO IDE
- Python 3.7 ou superior
- Driver USB-Serial CP2104 (TTGO LoRa32)

### Dependências

As seguintes bibliotecas são instaladas automaticamente via `platformio.ini`:

- `sandeepmistry/LoRa` @ ^0.8.0 - Driver LoRa SX1276
- `adafruit/RTClib` @ ^2.1.4 - RTC DS3231
- `bblanchon/ArduinoJson` @ ^6.21.3 - Serialização JSON
- `mikalhart/TinyGPSPlus` @ ^1.0.0 - Parser GPS NMEA

### Build e Upload

```bash
# Clone o repositório
git clone https://github.com/mathasilv/AgroSat-IoT.git
cd AgroSat-IoT

# Compile o projeto
pio run -e ttgo-lora32-v21

# Upload para o ESP32
pio run -e ttgo-lora32-v21 -t upload

# Monitor serial (115200 baud)
pio device monitor
```

### Configuração Inicial

1. Conecte o hardware (sensores I2C, GPS UART, SD Card)
2. Ajuste credenciais WiFi em `include/config.h` se necessário
3. Execute calibração do magnetômetro antes do primeiro voo (comando `CALIB_MAG`)
4. Para CCS811, execute baseline em ar puro por 20 minutos (`SAVE_BASELINE`)

## Comandos do Console Serial

Interface de comandos disponível nos modos PREFLIGHT e SAFE (115200 baud):

| Comando | Função | Observação |
|---------|--------|------------|
| `STATUS` | Exibe estado de todos os sensores e leituras atuais | Inclui status online/offline |
| `CALIB_MAG` | Inicia calibração do magnetômetro | Rotacionar CubeSat 360° durante processo |
| `CLEAR_MAG` | Apaga offsets de calibração do magnetômetro (NVS) | Requer recalibração posterior |
| `SAVE_BASELINE` | Salva baseline CCS811 em ar puro | Executar após 20 min em ambiente controlado |
| `START_MISSION` | Transição PREFLIGHT → FLIGHT | Inicia missão orbital |
| `STOP_MISSION` | Transição FLIGHT → PREFLIGHT | Retorna ao modo de pré-voo |
| `SAFE_MODE` | Força entrada no modo SAFE | Útil para diagnóstico |
| `LINK_BUDGET` | Exibe cálculo de link budget LoRa | Mostra margem de link e distância |
| `HELP` | Lista comandos disponíveis | - |

## Protocolos de Comunicação

### LoRa (Satélite ↔ Terra)

**Configuração de Rádio**:
- Frequência: 915 MHz (ISM Band Brasil)
- Spreading Factor: SF7 (alta taxa, curta distância) / SF12 (longo alcance, até 2302 km LEO)
- Bandwidth: 125 kHz
- Coding Rate: 4/5
- Potência de Transmissão: 20 dBm (100 mW)
- Duty Cycle: 10% (compliance ANATEL)

**Estrutura de Pacote Binário** (Store-and-Forward):

```
[Header: 2B] [Team ID: 2B] [Timestamp: 4B] [Payload: N bytes] [CRC: 2B]
```

**Link Budget**:
- Distância máxima LEO: 2302 km (órbita 600 km)
- Margem mínima viável: 3 dB
- Spreading Factor adaptativo baseado em distância GPS

### HTTP/JSON (Backup/Testes)

Endpoint OBSAT API (WiFi disponível):

```
POST https://obsat.org.br/teste_post/envio.php
Content-Type: application/json

{
  "team_id": 666,
  "timestamp": "2025-12-18T03:00:00Z",
  "cubesat": {
    "battery_voltage": 3.85,
    "temperature": 22.5,
    "position": {"lat": -16.6869, "lon": -49.2648, "alt": 600.0}
  },
  "ground_nodes": [
    {
      "node_id": "GN001",
      "soil_moisture": 45.2,
      "air_temp": 28.7,
      "rssi": -87,
      "received_at": "2025-12-18T02:58:12Z"
    }
  ]
}
```

## Formato de Telemetria

### Dados do CubeSat

Coletados a cada 20s (PREFLIGHT), 60s (FLIGHT) ou 120s (SAFE):

**Sistema de Potência**:
- Tensão da bateria (V)
- Percentual de carga (%)

**Atitude e Orientação**:
- Roll, Pitch, Yaw (graus) via MPU9250
- Campo magnético (X, Y, Z em μT)
- Aceleração (X, Y, Z em m/s²)
- Velocidade angular (X, Y, Z em °/s)

**Posição**:
- Latitude, Longitude (decimal degrees)
- Altitude GPS (m)
- Número de satélites GPS
- Status de fix GPS

**Ambiente Interno**:
- Temperatura (°C) - 3 fontes: MPU9250, BMP280, SI7021
- Pressão atmosférica (hPa)
- Umidade relativa (%)
- Qualidade do ar: eCO2 (ppm), TVOC (ppb)
- Altitude barométrica (m)

**Sistema**:
- Uptime (ms)
- Contagem de resets
- Heap mínimo disponível (bytes)
- Temperatura CPU (°C)
- Status de erro (bitmask)

### Dados de Nós Terrestres (Store-and-Forward)

- ID do nó terrestre
- Umidade do solo (%)
- Temperatura ambiente (°C)
- Umidade relativa (%)
- Status de irrigação
- RSSI/SNR do link LoRa
- Timestamp de geração/coleta/retransmissão
- Prioridade (Critical/High/Normal/Low)
- Flag de retransmissão

## Armazenamento em SD Card

### Estrutura de Arquivos CSV

O sistema implementa rotação automática de arquivos ao atingir 5 MB:

```
/telemetry/
  ├── telemetry_001.csv      # Dados científicos do CubeSat
  ├── telemetry_002.csv
  ├── ground_nodes_001.csv   # Dados Store-and-Forward
  ├── ground_nodes_002.csv
  ├── system_log_001.csv     # Logs de sistema e eventos
  └── errors.log             # Registro de erros críticos
```

### Formato CSV

**telemetry.csv**:
```csv
timestamp,mission_time,battery_v,battery_pct,temp_mpu,temp_bmp,temp_si,pressure,lat,lon,alt_gps,satellites,gyro_x,gyro_y,gyro_z,accel_x,accel_y,accel_z,mag_x,mag_y,mag_z,alt_baro,humidity,co2,tvoc,system_status,uptime,heap_min
```

**ground_nodes.csv**:
```csv
timestamp,node_id,seq_num,soil_moisture,temp,humidity,irrigation,rssi,snr,collection_time,retransmission_time,priority,forwarded
```

## Robustez e Recuperação de Falhas

### Watchdog Timer (WDT)

- Timeout adaptativo por modo: 60s (PREFLIGHT), 90s (FLIGHT), 180s (SAFE)
- Reset automático em caso de trava de tarefas FreeRTOS
- Contador de resets persistente em NVS

### Monitoramento de Heap

```cpp
if (ESP.getFreeHeap() < 20 * 1024) {  // < 20 KB
    enterSafeMode();
    logError("Critical heap exhaustion");
}
```

### Persistência de Estado (NVS)

Dados salvos em memória não-volátil (sobrevivem a resets):
- Modo operacional atual
- Offsets de calibração de sensores (magnetômetro)
- Baseline CCS811 (qualidade do ar)
- Contador de missões e resets
- Estatísticas de uptime

### Recuperação de Falhas I2C

O `SensorManager` implementa mutex I2C e retry automático:
- Timeout de 3000 ms para clock stretching (CCS811)
- Buffer I2C de 512 bytes para estabilidade
- Recuperação automática de falhas de comunicação

## Link Budget e Análise de Comunicação

O sistema calcula dinamicamente o link budget LoRa:

```cpp
struct LinkBudget {
    float maxDistance;      // Distância máxima teórica (2302 km)
    float currentDistance;  // Distância calculada via GPS (km)
    float linkMargin;       // Margem de link (dB)
    float pathLoss;         // Perda de propagação (dB)
    bool isViable;          // true se margin > 3 dB
    int8_t recommendedSF;   // SF recomendado baseado em distância
};
```

**Parâmetros de Link**:
- Potência TX: +20 dBm (100 mW)
- Sensibilidade RX: -148 dBm (SF12, BW 125 kHz)
- Path Loss (Friis): ~190 dB @ 2302 km, 915 MHz
- Margem de segurança mínima: 3 dB

**Comando**: `LINK_BUDGET` exibe análise completa no console serial.

## Segurança e Criptografia

### AES-128

O `CryptoManager` implementa criptografia AES-128 para dados críticos:
- Chave simétrica de 128 bits (configurável em `config.h`)
- Modo CBC (Cipher Block Chaining)
- IV (Initialization Vector) aleatório por mensagem
- Habilitação via flag `AES_ENABLED`

## Documentação Técnica

### Arquivos de Configuração

- [`platformio.ini`](https://github.com/mathasilv/AgroSat-IoT/blob/main/platformio.ini) - Build config + dependências
- [`include/config.h`](https://github.com/mathasilv/AgroSat-IoT/blob/main/include/config.h) - Pinout + constantes de missão
- [`src/main.cpp`](https://github.com/mathasilv/AgroSat-IoT/blob/main/src/main.cpp) - Programa principal e loop

### Diretrizes de Desenvolvimento

**Boas Práticas**:
- Utilizar C++11/14 para compatibilidade ESP32
- Evitar alocação dinâmica (`malloc`/`new`) sempre que possível
- Documentar código crítico e hardware-specific
- Testar em hardware real (TTGO LoRa32) antes de commits
- Manter consistência com padrão de Service Managers

**Estrutura de Commits**:
- `Add:` nova funcionalidade
- `Fix:` correção de bug
- `Refactor:` refatoração sem mudança funcional
- `Docs:` atualização de documentação

## Contribuindo

Contribuições são bem-vindas. Siga o fluxo padrão de contribuição:

1. Faça um fork do repositório
2. Crie uma feature branch: `git checkout -b feature/nova-funcionalidade`
3. Commit suas mudanças: `git commit -m 'Add: Nova funcionalidade X'`
4. Push para a branch: `git push origin feature/nova-funcionalidade`
5. Abra um Pull Request

Certifique-se de testar em hardware real antes de submeter PRs.

## Equipe Orbitalis

**Categoria N3 - 3ª Olimpíada Brasileira de Satélites (OBSAT MCTI)**

- **Instituição**: Universidade Federal de Goiás (UFG)
- **Desenvolvedores**:
  - Matheus Aparecido Souza Silva
  - Luana Sthephany Rodrigues Mamed
- **Tutor**: Prof. Aldo Diaz
- **Organização**: Ministério da Ciência, Tecnologia e Inovação (MCTI)

## Agradecimentos

- OBSAT MCTI pelo suporte à competição
- PlatformIO pela plataforma de desenvolvimento robusta
- Espressif Systems pela arquitetura ESP32
- LoRa Alliance pelos padrões de comunicação
- Comunidade open-source de sistemas embarcados aeroespaciais
