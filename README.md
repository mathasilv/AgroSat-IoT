# 🛰️ AgroSat-IoT - Sistema de Monitoramento Agrícola por Satélite

## Visão Geral do Projeto

O **AgroSat-IoT** é um sistema embarcado de aquisição de dados, com capacidade de retransmissão de dados via satélite de órbita baixa (LEO) no modelo **Store-and-Forward**. Seu principal objetivo é fornecer uma solução de **Agricultura de Precisão** para monitoramento de nós terrestres (sensores de solo) em áreas rurais sem conectividade convencional.

Desenvolvido em torno do microcontrolador **ESP32**, o projeto foca na robustez, baixo consumo de energia e comunicação de longo alcance via **LoRa** para coletar dados dos nós terrestres e retransmiti-los para uma estação em terra durante as passagens do CubeSat.

## Missão e Funcionalidades Chave

A missão principal do AgroSat-IoT é atuar como um **nó de coleta e retransmissão orbital**, garantindo que os dados críticos de campo cheguem à base de controle.

* **🛰️ Telemetria Científica**: Coleta contínua de dados ambientais do próprio CubeSat (temperatura, pressão, IMU 9-DOF, altitude, gases e saúde do sistema).
* **📡 Função Store-and-Forward (LoRa Relay)**: Recebe e armazena pacotes de dados de diversos **Nós Terrestres** (Ground Nodes) via LoRa e os retransmite à estação em terra via uplink HTTP ou formato binário de satélite.
* **⚙️ Gerenciamento de Missão**: Implementação de modos de operação (`PREFLIGHT`, `FLIGHT`, `SAFE`) com persistência de estado (NVS) e recuperação de falhas.
* **🛡️ Saúde e Robustez do Sistema**: Monitoramento contínuo de recursos (Heap, Watchdog) com estratégias automáticas de reinicialização e modos de baixo consumo de energia.
* **💾 Armazenamento de Dados**: Gerenciamento robusto de cartão SD (SD Card) para log de telemetria, erros e dados da missão, com função de rotação de arquivos por tamanho.

## Arquitetura de Software

A arquitetura modular e orientada a objetos é implementada em C++ no framework Arduino/ESP-IDF, seguindo o padrão de **Gerenciadores de Serviço** para abstrair o hardware e o comportamento da missão.

| Diretório | Responsabilidade | Componentes Chave |
| :--- | :--- | :--- |
| `src/core` | Funções de baixo nível, tempo e sistema. | `RTCManager`, `PowerManager`, `SystemHealth`, `ButtonHandler` |
| `src/sensors` | Interface centralizada para todos os sensores. | `SensorManager`, `MPU9250Manager`, `BMP280Manager`, `SI7021Manager`, `CCS811Manager` |
| `src/comm` | Todos os links de comunicação e formatação de dados. | `CommunicationManager`, `LoRaService`, `WiFiService`, `HttpService`, `PayloadManager` |
| `src/app` | Lógica de negócio, estado da missão e coleta de telemetria. | `TelemetryManager`, `MissionController`, `GroundNodeManager`, `TelemetryCollector` |
| `src/storage` | Leitura/Escrita de dados no SD Card e log de sistema. | `StorageManager` |


## Configuração e Especificações Técnicas

### 1. Hardware e Pinos

O projeto é otimizado para placas com ESP32 e módulo LoRa (ex: **TTGO LoRa32-V2.1**). As configurações de pinos estão definidas em `include/config.h`.

| Periférico | Função | Pinos (Padrão) |
| :--- | :--- | :--- |
| **LoRa** | SPI | SCK(5), MISO(19), MOSI(27), CS(18), RST(23), DIO0(26) |
| **SD Card** | SPI | CS(13), MOSI(15), MISO(2), SCLK(14) |
| **I2C Sensores** | SDA, SCL | SDA(21), SCL(22) |
| **Bateria** | Leitura Analógica | PIN(35) |
| **Botão de Controle** | Entrada | PIN(4) |

### 2. Barramento I2C Robusto

A configuração do I2C é crítica para o funcionamento do sensor de qualidade do ar **CCS811**, que utiliza **Clock Stretching**. O sistema aplica uma correção de inicialização para evitar falhas I2C (`ERRO 263`):
* **Frequência**: $50 \text{kHz}$ (Para robustez)
* **Timeout**: $3000 \text{ms}$

### 3. Modos de Operação

O sistema possui modos operacionais distintos, gerenciados pelo `TelemetryManager`, com configurações específicas de consumo e comunicação:

| Modo | Descrição | Log Serial | LoRa TX | HTTP TX | Intervalo SD |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `MODE_PREFLIGHT` | Debug total / Standby na base. | **Sim** | **Sim** | **Sim** | $10 \text{s}$ |
| `MODE_FLIGHT` | Missão ativa (Otimizado). | Não | **Sim** | **Sim** | $10 \text{s}$ |
| `MODE_SAFE` | Bateria crítica / Erro. | **Sim** | **Sim** (Beacon Lento) | Não | $300 \text{s}$ |

## Configuração do Ambiente de Desenvolvimento

### Pré-requisitos
* **PlatformIO IDE**: Recomendado o uso do Visual Studio Code com a extensão PlatformIO.

### Build e Upload
1.  **Clone o Repositório**:
    ```bash
    git clone [https://github.com/mathasilv/AgroSat-IoT.git](https://github.com/mathasilv/AgroSat-IoT.git)
    cd AgroSat-IoT
    ```

2.  **Instale Dependências (PlatformIO)**: As dependências principais (LoRa, RTClib, ArduinoJson) são listadas em `platformio.ini`.
    ```bash
    pio lib install
    ```

3.  **Compile e Faça Upload**:
    ```bash
    pio run -e ttgo-lora32-v21 -t upload
    ```
    *(O ambiente `ttgo-lora32-v21` é o board de destino padrão, definido em `platformio.ini`)*.

## Formato de Telemetria

Os dados são transmitidos em dois formatos:

### 1. HTTP/JSON (Formato OBSAT)
Utilizado para envio de dados quando a conexão WiFi está disponível (após a missão ou durante testes). É um JSON rigoroso, compatível com a plataforma OBSAT, que inclui todos os dados do CubeSat mais um array detalhado dos **Nós Terrestres** coletados.

### 2. LoRa (Payload Binário Compacto)
Utilizado para comunicação de satélite (Store-and-Forward) e retransmissão de Ground Nodes. O formato binário garante o uso eficiente da largura de banda LoRa. Os pacotes são codificados em hexadecimal para transmissão.

## Comandos de Console (Serial Monitor)

Durante o desenvolvimento ou no modo `PREFLIGHT`/`SAFE`, comandos podem ser enviados via Serial Monitor:

| Comando | Descrição | Módulo Principal |
| :--- | :--- | :--- |
| `STATUS` | Exibe o status detalhado de todos os sensores (online/offline, leituras). | `SensorManager` |
| `CALIB_MAG` | Inicia a rotina de calibração do Magnetômetro (MPU9250). | `MPU9250Manager` |
| `CLEAR_MAG` | Apaga os offsets de calibração do Magnetômetro salvos na NVS. | `MPU9250Manager` |
| `SAVE_BASELINE` | Salva o valor de Baseline do CCS811 na NVS (usar em ar puro). | `CCS811Manager` |
| `HELP` | Lista os comandos disponíveis. | `CommandHandler` |

---

## 👨‍💻 Contribuindo

Se você deseja contribuir, siga as diretrizes padrão do GitHub (Fork, Feature Branch, Pull Request).

* **Boas Práticas**: Priorize o uso das classes Gerenciadoras existentes e mantenha a lógica de "negócio" em `src/app`.

## Licença

Este projeto está licenciado sob a Licença MIT.

*Agradecimentos especiais a OBSAT e ao workflow PlatformIO por apoiar o desenvolvimento de sistemas espaciais embarcados.*