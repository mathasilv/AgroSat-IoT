# AgroSat-IoT: CubeSat 1U Store-and-Forward (FreeRTOS Edition)

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%7C%20FreeRTOS-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-10.8.0-orange)

Sistema de telemetria orbital para monitoramento remoto de cultivos em áreas sem conectividade terrestre. Versão migrada para **FreeRTOS** para maximizar a eficiência multitarefa e o uso do dual-core do ESP32.

---

## Sumário
1. [Visão Geral](#visão-geral)
2. [Arquitetura do Sistema (FreeRTOS)](#arquitetura-do-sistema-freertos)
3. [Modos de Operação](#modos-de-operação)
4. [Especificações de Hardware](#especificações-de-hardware)
5. [Instalação e Build](#instalação-e-build)
6. [Interface de Comandos](#interface-de-comandos)

---

## Visão Geral

O AgroSat-IoT é um sistema embarcado para CubeSat 1U que implementa um relay orbital operando no modelo **Store-and-Forward**. O satélite coleta dados de sensores agrícolas terrestres via LoRa (915 MHz), processa em tempo real utilizando RTOS e retransmite para estações base.

### Capacidades Técnicas
* **Multitarefa Preemptiva:** Execução concorrente de leitura de sensores, rádio e armazenamento.
* **Dual-Core Processing:** Separação física entre lógica de sensores (Core 1) e I/O pesado (Core 0).
* **Store-and-Forward:** Bufferização segura com filas do FreeRTOS.
* **Gestão de Energia:** Dynamic Frequency Scaling (DFS) e Tasks suspensas (Blocked state).
* **Segurança de Dados:** Uso de Mutexes para proteção de barramentos (I2C/SPI) e regiões de memória crítica.

---

## Arquitetura do Sistema (FreeRTOS)

O software adota uma arquitetura híbrida, combinando camadas lógicas de aplicação com o gerenciamento de tarefas em tempo real do FreeRTOS.

### 1. Organização de Tarefas (Tasks)
O sistema distribui a carga de trabalho em tarefas dedicadas para garantir determinismo temporal na coleta de dados.

| Tarefa | Prioridade | Core | Stack | Frequência | Descrição |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **SensorsTask** | 2 (Alta) | 1 (App) | 4096 B | 10 Hz | Leitura de sensores I2C, fusão de dados e gestão de energia. |
| **HttpTask** | 1 (Baixa) | 0 (Pro) | 8192 B | Evento | Envio assíncrono de telemetria via WiFi (quando disponível). |
| **StorageTask** | 1 (Baixa) | 0 (Pro) | 4096 B | Evento | Persistência de dados no Cartão SD sem bloquear o fluxo principal. |
| **Loop (Main)** | 1 (Baixa) | 1 (App) | Auto | Loop | Gerenciamento da pilha de rádio LoRa, Watchdog e Serial. |

### 2. Sincronização e IPC (Inter-Process Communication)
Para garantir a integridade dos dados e acesso seguro aos periféricos:

* **Filas (Queues):** Desacoplam a produção de dados do consumo (armazenamento/envio).
    * `xHttpQueue`: Bufferiza pacotes JSON para envio WiFi.
    * `xStorageQueue`: Fila para gravação de logs CSV no SD.
* **Semáforos (Mutexes):**
    * `xI2CMutex`: Garante acesso exclusivo ao barramento I2C (Sensores vs Power Manager).
    * `xSerialMutex`: Previne corrupção de saída no debug serial.
    * `xDataMutex`: Protege o acesso às estruturas globais de telemetria.
* **Semáforos Binários:**
    * `xLoRaRxSemaphore`: Sinaliza interrupção de recebimento de pacote LoRa.

### 3. Camadas de Software
* **App Layer:** `MissionController`, `TelemetryManager` (Orquestração).
* **Service Layer:** `SensorManager`, `CommunicationManager` (Abstração).
* **HAL:** Drivers nativos ESP-IDF e Arduino Core.

---

## Modos de Operação

A máquina de estados controla o comportamento das Tasks e o consumo de energia.

* **PREFLIGHT (Solo):**
    * Todas as Tasks ativas.
    * CPU: 240 MHz.
    * Serial Debug: Habilitado.
    * Watchdog: 60s.

* **FLIGHT (Orbital):**
    * `HttpTask` suspensa (Rádio WiFi desligado).
    * Foco total em `SensorsTask` e LoRa.
    * CPU: Ajuste dinâmico (80-240 MHz).
    * Watchdog: 90s.

* **SAFE (Recuperação):**
    * Ativado se Bateria < 3.3V ou Heap Crítico.
    * Apenas LoRa Beacon ativo.
    * Sensores e Armazenamento suspensos.

---

## Especificações de Hardware

**Plataforma:** ESP32 (TTGO LoRa32 V2.1)

### Pinagem e Recursos (FreeRTOS Safe)

| Recurso | Hardware | Pinos / Config | Proteção |
| :--- | :--- | :--- | :--- |
| **I2C Bus** | Sensores | SDA: 21, SCL: 22 (100 kHz) | `xI2CMutex` |
| **SPI Bus** | LoRa / SD | SCK: 5/14, MISO: 19/2, MOSI: 27/15 | `xSPIMutex` |
| **Serial** | Debug / GPS | UART0 / UART1 | `xSerialMutex` |
| **Watchdog** | SW Timer | Group 0, Stage 0 | `esp_task_wdt` |

---

## Instalação e Build

### Pré-requisitos
* VS Code com PlatformIO
* Suporte a C++11/C++17

### Compilação

O projeto está configurado para o ambiente `ttgo-lora32-v21` com flags de debug do FreeRTOS ativadas.

```bash
# 1. Clonar repositório
git clone [https://github.com/mathasilv/AgroSat-IoT.git](https://github.com/mathasilv/AgroSat-IoT.git)

# 2. Compilar (Release)
pio run -e ttgo-lora32-v21

# 3. Upload e Monitor
pio run -e ttgo-lora32-v21 -t upload
pio device monitor