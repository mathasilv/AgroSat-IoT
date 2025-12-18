# AgroSat-IoT: CubeSat 1U Store-and-Forward

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%7C%20FreeRTOS-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-9.0.0-orange)

Sistema de telemetria orbital para monitoramento remoto de cultivos em áreas sem conectividade terrestre. Desenvolvido pela equipe Orbitalis (UFG) para a 3ª Olimpíada Brasileira de Satélites (OBSAT MCTI).

---

## Sumário
1. [Visão Geral](#visão-geral)
2. [Arquitetura do Sistema](#arquitetura-do-sistema)
3. [Modos de Operação](#modos-de-operação)
4. [Especificações de Hardware](#especificações-de-hardware)
5. [Instalação e Build](#instalação-e-build)
6. [Interface de Comandos](#interface-de-comandos)
7. [Protocolos de Comunicação](#protocolos-de-comunicação)

---

## Visão Geral

O AgroSat-IoT é um sistema embarcado para CubeSat 1U que implementa um relay orbital operando no modelo **Store-and-Forward**. O satélite coleta dados de sensores agrícolas terrestres via LoRa (915 MHz) durante passagens orbitais, armazena-os em memória não-volátil e retransmite as informações para estações base quando solicitado.

### Capacidades Técnicas
* **Store-and-Forward Orbital:** Recepção, bufferização e retransmissão de dados de múltiplos nós.
* **Telemetria Multi-Sensor:** IMU 9-DOF, Barômetro, Higrômetro e Qualidade do Ar (eCO2/TVOC).
* **Gestão de Energia:** Dynamic Frequency Scaling (DFS) com ajuste de clock (80/240 MHz).
* **Robustez:** Watchdog Timer adaptativo, monitoramento de Heap e persistência NVS.
* **Segurança:** Criptografia AES-128 nos links de dados críticos.

---

## Arquitetura do Sistema

O software segue uma arquitetura em camadas (Layered Architecture) sobre FreeRTOS, garantindo desacoplamento entre hardware e regras de negócio.

### 1. Camada de Aplicação (App Layer)
Responsável pela lógica de missão e orquestração dos dados.
* **MissionController:** Gerencia a máquina de estados global e transições de voo.
* **TelemetryManager:** Agrega dados de subsistemas e formata pacotes JSON/Binários.
* **GroundNodeManager:** Gerencia a fila de prioridade (QoS) dos nós terrestres.

### 2. Gerenciadores de Serviço (Service Managers)
Abstraem funcionalidades complexas em interfaces simples para a aplicação.
* **SensorManager:** Controla o barramento I2C, leituras e recuperação de falhas (bit-banging).
* **CommunicationManager:** Unifica as interfaces LoRa, WiFi e Serial.
* **SystemHealth:** Monitora bateria, memória livre e alimenta o Watchdog.

### 3. Camada de Abstração de Hardware (HAL)
Drivers de baixo nível isolados.
* Drivers para SX1276 (LoRa), MPU9250, BME280, CCS811.
* Acesso direto a registros SPI/I2C.

---

## Modos de Operação

O satélite opera com base em uma máquina de estados finita para garantir a segurança da missão.

* **PREFLIGHT (Solo):**
    * Modo padrão na inicialização.
    * Todos os periféricos ativos.
    * Logs seriais habilitados para debug.
    * CPU em performance máxima (240 MHz).
    * Timeout do Watchdog: 60s.

* **FLIGHT (Orbital):**
    * Ativado via comando `START_MISSION`.
    * Operação silenciosa (Serial desativada).
    * Ciclos de rádio otimizados.
    * CPU ajustável (DFS).
    * Timeout do Watchdog: 90s.

* **SAFE (Recuperação):**
    * Ativado automaticamente se Bateria < 3.3V ou Heap < 20KB.
    * Sensores e WiFi desligados.
    * Apenas beacon LoRa ativo a cada 180s.
    * CPU em modo econômico (80 MHz).
    * Requer intervenção manual ou recarga de bateria para sair.

> [!WARNING]
> A transição para o modo SAFE é uma medida de proteção crítica. O sistema prioriza a manutenção do link de comunicação em detrimento da coleta de dados.

---

## Especificações de Hardware

**Plataforma:** ESP32 (TTGO LoRa32 V2.1)

### Pinagem Crítica

| Periférico | Interface | Pinos (ESP32) |
| :--- | :--- | :--- |
| **LoRa SX1276** | SPI | SCK: 5, MISO: 19, MOSI: 27, CS: 18, RST: 23, DIO0: 26 |
| **SD Card** | SPI | SCK: 14, MISO: 2, MOSI: 15, CS: 13 |
| **Sensores** | I2C | SDA: 21, SCL: 22 (100 kHz) |
| **Bateria** | ADC | GPIO 35 (Divisor 1/2) |
| **GPS UART** | Serial | TX: 12, RX: 34 |

---

## Instalação e Build

### Pré-requisitos
* VS Code com PlatformIO
* Driver CP2104

### Compilação e Upload

```bash
# 1. Clonar repositório
git clone [https://github.com/mathasilv/AgroSat-IoT.git](https://github.com/mathasilv/AgroSat-IoT.git)

# 2. Compilar
pio run -e ttgo-lora32-v21

# 3. Upload e Monitor
pio run -e ttgo-lora32-v21 -t upload
pio device monitor

```

---

## Interface de Comandos

Disponível via Serial (115200 baud) nos modos PREFLIGHT e SAFE.

| Comando | Descrição |
| --- | --- |
| `STATUS` | Exibe telemetria instantânea e status online/offline dos sensores. |
| `CALIB_MAG` | Inicia rotina de calibração do magnetômetro (requer rotação física). |
| `SAVE_BASELINE` | Salva a linha de base do sensor de qualidade do ar (CCS811). |
| `START_MISSION` | Transição manual para modo FLIGHT. |
| `STOP_MISSION` | Retorna ao modo PREFLIGHT. |
| `LINK_BUDGET` | Exibe análise teórica do link de rádio LoRa. |

---

## Protocolos de Comunicação

### LoRa (Uplink/Downlink)

* **Frequência:** 915 MHz
* **Largura de Banda:** 125 kHz
* **Spreading Factor:** SF7 a SF12 (Adaptativo)
* **Coding Rate:** 4/5
* **Potência:** 20 dBm

**Estrutura do Pacote Binário:**
`[Header: 2B] [Team ID: 2B] [Timestamp: 4B] [Payload: N bytes] [CRC: 2B]`

### Armazenamento (SD Card)

Os dados são persistidos em formato CSV com rotação de arquivos para evitar corrupção de sistema de arquivos em grandes volumes de dados.

**Exemplo de Log de Telemetria:**

```csv
timestamp,mission_time,battery_v,temp_mpu,pressure,lat,lon,satellites,system_status
1703450000,1200,3.85,25.4,980.5,-16.68,-49.26,8,0x00

```

---

**Desenvolvido por Equipe Orbitalis - UFG**