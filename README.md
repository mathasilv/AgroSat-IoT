# AgroSat-IoT

Sistema embarcado para CubeSat 1U desenvolvido para monitoramento remoto de cultivos agrícolas em regiões sem cobertura de internet ou telefonia celular.

---

## Sumário

1. [Sobre o Projeto](#sobre-o-projeto)
2. [Problema e Solução](#problema-e-solução)
3. [Como Funciona](#como-funciona)
4. [Funcionalidades Principais](#funcionalidades-principais)
5. [Sensores e Medições](#sensores-e-medições)
6. [Modos de Operação](#modos-de-operação)
7. [Comunicação](#comunicação)
8. [Segurança e Confiabilidade](#segurança-e-confiabilidade)
9. [Hardware Utilizado](#hardware-utilizado)
10. [Instalação](#instalação)
11. [Comandos Disponíveis](#comandos-disponíveis)
12. [Equipe](#equipe)

---

## Sobre o Projeto

O AgroSat-IoT e um sistema de telemetria orbital projetado para operar em um CubeSat no formato 1U (cubo de 10x10x10 cm). Seu objetivo principal e servir como um ponto de retransmissao (relay) entre sensores instalados em propriedades rurais e estacoes de controle em terra.

O projeto foi desenvolvido para participar de competicoes de CubeSats educacionais e demonstrar a viabilidade de solucoes de baixo custo para agricultura de precisao em areas remotas.

---

## Problema e Solucao

### O Problema

Grandes areas agricolas no Brasil, especialmente em regioes do Centro-Oeste e Norte, nao possuem cobertura de internet ou sinal de celular. Isso impede que produtores rurais utilizem tecnologias modernas de monitoramento de suas lavouras, como sensores de umidade do solo, temperatura e sistemas de irrigacao inteligente.

### A Solucao

O AgroSat-IoT resolve esse problema utilizando comunicacao via radio LoRa, que possui longo alcance e baixo consumo de energia. O sistema funciona da seguinte forma:

1. Sensores instalados nas propriedades rurais (nos terrestres) coletam dados sobre as condicoes do solo e ambiente
2. Esses dados sao transmitidos via radio para o satelite quando ele passa sobre a regiao
3. O satelite armazena os dados recebidos (modelo Store-and-Forward)
4. Quando o satelite passa sobre uma estacao base com internet, os dados sao retransmitidos
5. Os dados ficam disponiveis para consulta pelo produtor rural

---

## Como Funciona

O sistema opera em um ciclo continuo de coleta, armazenamento e retransmissao de dados:

### Coleta de Dados Proprios

O satelite possui sensores embarcados que monitoram constantemente:
- Sua propria posicao via GPS
- Condicoes internas (temperatura, pressao, umidade)
- Orientacao espacial (acelerometro, giroscopio, magnetometro)
- Qualidade do ar interno
- Nivel de bateria e saude do sistema

### Recepcao de Dados Terrestres

Quando o satelite passa sobre uma area monitorada, ele recebe automaticamente os dados transmitidos pelos sensores terrestres. Cada pacote recebido contem:
- Identificacao do sensor de origem
- Umidade do solo
- Temperatura ambiente
- Status do sistema de irrigacao

### Armazenamento Seguro

Todos os dados sao armazenados em cartao SD com redundancia tripla e verificacao de integridade (CRC). Isso garante que nenhuma informacao seja perdida mesmo em caso de falhas parciais.

### Retransmissao

Os dados coletados sao retransmitidos de duas formas:
- Via radio LoRa para estacoes terrestres
- Via WiFi/HTTP quando disponivel (em testes de solo)

---

## Funcionalidades Principais

### Relay Store-and-Forward

O satelite atua como um intermediario entre sensores remotos e estacoes base. Ele armazena os dados recebidos e os retransmite quando possivel, permitindo comunicacao assincrona mesmo quando nao ha linha de visada direta.

### Gerenciamento de Multiplos Nos

O sistema pode gerenciar simultaneamente ate 3 nos terrestres ativos, priorizando automaticamente os dados mais criticos para retransmissao. A priorizacao considera:
- Criticidade dos dados (alertas de irrigacao tem prioridade)
- Tempo desde a ultima transmissao bem-sucedida
- Qualidade do sinal de cada no

### Controle Automatico de Energia

O sistema monitora constantemente o nivel da bateria e ajusta automaticamente seu comportamento:
- Em condicoes normais: todas as funcoes ativas
- Bateria baixa: reduz frequencia de transmissoes
- Bateria critica: entra em modo de sobrevivencia, mantendo apenas funcoes essenciais

### Monitoramento de Saude

O sistema realiza autodiagnostico continuo, verificando:
- Funcionamento de todos os sensores
- Espaco disponivel no armazenamento
- Memoria disponivel
- Temperatura interna
- Erros de comunicacao

### Sincronizacao de Tempo

O relogio interno e sincronizado via NTP quando ha conexao WiFi disponivel, garantindo que todos os dados tenham registro temporal preciso. Um relogio de tempo real (RTC) com bateria propria mantem a hora mesmo durante reinicializacoes.

---

## Sensores e Medicoes

### Sensores de Navegacao e Orientacao

**GPS (NEO-M8N)**
- Latitude e longitude
- Altitude
- Numero de satelites visiveis
- Status de posicionamento

**Unidade de Medicao Inercial - IMU (MPU9250)**
- Acelerometro 3 eixos: mede aceleracao linear
- Giroscopio 3 eixos: mede velocidade de rotacao
- Magnetometro 3 eixos: funciona como bussola digital

### Sensores Ambientais

**Pressao e Temperatura (BMP280)**
- Pressao atmosferica
- Temperatura
- Altitude barometrica calculada

**Umidade e Temperatura (SI7021)**
- Umidade relativa do ar
- Temperatura (redundancia)

**Qualidade do Ar (CCS811)**
- Concentracao de CO2 equivalente (eCO2)
- Compostos Organicos Volateis Totais (TVOC)

### Monitoramento de Energia

- Tensao da bateria
- Percentual de carga estimado
- Deteccao de niveis criticos

---

## Modos de Operacao

O sistema possui diferentes modos de operacao que se adaptam as condicoes da missao:

### Modo Pre-Voo (PREFLIGHT)

Utilizado durante testes em solo antes do lancamento:
- Todas as funcionalidades ativas
- Comunicacao WiFi habilitada para testes
- Logs detalhados para depuracao
- Display ativo (quando disponivel)

### Modo Voo (FLIGHT)

Modo principal de operacao orbital:
- Foco em coleta e retransmissao de dados
- WiFi desabilitado para economia de energia
- Comunicacao exclusivamente via LoRa
- Intervalos otimizados para duracao da bateria

### Modo Seguro (SAFE)

Ativado automaticamente em situacoes criticas:
- Bateria abaixo do nivel critico
- Temperatura fora dos limites seguros
- Falhas graves de sistema

Neste modo:
- Apenas funcoes essenciais permanecem ativas
- Transmite beacon periodico para localizacao
- Sensores secundarios desligados
- Maximo foco em preservar energia

---

## Comunicacao

### Radio LoRa

Principal meio de comunicacao do satelite:
- Frequencia: 915 MHz (banda ISM Brasil)
- Alcance: dezenas de quilometros em linha de visada
- Baixo consumo de energia
- Resistente a interferencias

O sistema implementa controle de ciclo de trabalho (duty cycle) para cumprir regulamentacoes e evitar sobrecarga do canal de radio.

### WiFi e HTTP

Utilizado principalmente em testes de solo:
- Conexao com redes WiFi convencionais
- Envio de dados para servidor via HTTPS
- Sincronizacao de relogio via NTP

### Protocolo de Dados

Os dados sao transmitidos em formato binario compacto para maximizar a eficiencia do canal de radio. O sistema suporta:
- Pacotes de telemetria do satelite
- Pacotes de dados dos nos terrestres
- Pacotes de relay (dados combinados)

---

## Seguranca e Confiabilidade

### Criptografia

Os dados transmitidos via LoRa sao protegidos com criptografia AES-128, impedindo que terceiros interceptem ou adulterem as informacoes.

### Integridade de Dados

- Verificacao CRC em todas as transmissoes
- Armazenamento com redundancia tripla no cartao SD
- Deteccao e recuperacao automatica de erros

### Watchdog

Um temporizador de seguranca (watchdog) reinicia automaticamente o sistema caso ele trave, garantindo operacao continua mesmo em caso de falhas de software.

### Protecao de Recursos

O sistema utiliza mecanismos de sincronizacao (mutexes) para garantir que multiplas tarefas nao corrompam dados ao acessar recursos compartilhados simultaneamente.

---

## Hardware Utilizado

### Placa Principal

**TTGO LoRa32 V2.1 (LilyGo T3 V1.6.1)**
- Microcontrolador ESP32 dual-core
- Radio LoRa SX1276 integrado
- Display OLED (opcional)
- Conector para bateria Li-ion

### Sensores

| Componente | Funcao |
|------------|--------|
| NEO-M8N | GPS |
| MPU9250 | Acelerometro, Giroscopio, Magnetometro |
| BMP280 | Pressao e Temperatura |
| SI7021 | Umidade e Temperatura |
| CCS811 | Qualidade do Ar (CO2, TVOC) |
| DS3231 | Relogio de Tempo Real |

### Armazenamento

- Cartao microSD para registro de dados

### Alimentacao

- Bateria Li-ion 18650
- Monitoramento de tensao integrado

---

## Instalacao

### Requisitos

- Visual Studio Code
- Extensao PlatformIO
- Cabo USB para programacao

### Compilacao e Upload

```bash
# Clonar o repositorio
git clone https://github.com/mathasilv/AgroSat-IoT.git

# Entrar no diretorio
cd AgroSat-IoT

# Compilar o projeto
pio run

# Enviar para a placa
pio run -t upload

# Abrir monitor serial
pio device monitor
```

---

## Comandos Disponiveis

O sistema aceita comandos via porta serial (115200 baud):

| Comando | Descricao |
|---------|-----------|
| STATUS | Exibe estado detalhado do sistema |
| START_MISSION | Inicia modo de voo (FLIGHT) |
| STOP_MISSION | Retorna ao modo pre-voo (PREFLIGHT) |
| SAFE_MODE | Forca entrada no modo seguro |
| HELP | Lista comandos disponiveis |

---

## Equipe

**Equipe Orbitalis - Universidade Federal de Goias (UFG)**

---

## Licenca

Este projeto esta licenciado sob a licenca MIT.