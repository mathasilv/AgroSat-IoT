# AgroSat-IoT CubeSat - OBSAT Fase 2

## Equipe Orbitalis - Categoria N3

### Firmware v2.3.0 - Dual-Mode Operation

## 🚀 Características

### Sistema Dual-Mode

**MODE_PREFLIGHT** (Debug & Validation)
- ✅ Logging verboso completo no Serial
- ✅ Display OLED ativo (refresh 2s)
- ✅ Diagnósticos detalhados de heap
- ✅ Status de todos os subsistemas
- ✅ Mesma coleta de dados do FLIGHT

**MODE_FLIGHT** (Mission Critical)
- ⚡ Display OLED desligado (economia ~15mA)
- ⚡ Logging mínimo (apenas erros)
- ⚡ Heap check reduzido (60s vs 10s)
- ⚡ Maximiza autonomia de bateria
- ✅ Telemetria HTTP a cada 4min por 2h

### Hardware
- **Placa**: LoRa32 V2.1_1.6 (ESP32)
- **Sensores**: MPU6050/MPU9250 (IMU), BMP280 (Pressão/Temp), SHT20 (Umidade), CCS811 (CO2/TVOC)
- **Comunicação**: WiFi (HTTP), LoRa 433MHz
- **Armazenamento**: SD Card

### Requisitos OBSAT Fase 2
- ✅ Telemetria WiFi HTTP POST (JSON)
- ✅ Intervalo: 4 minutos (configurável em `config.h`)
- ✅ Duração: 2 horas
- ✅ Dados: Bateria, Temp, Pressão, IMU (6 eixos), Payload LoRa (máx 90 bytes)
- ✅ Backup local em SD Card

## 🛠️ Uso

### Comandos Serial (115200 baud)

```
START / S / 1  - Iniciar missão (PREFLIGHT → FLIGHT)
STOP  / P / 0  - Parar missão
STATUS / ?     - Mostrar status do sistema
RESTART / R    - Reiniciar ESP32
HELP  / H      - Lista de comandos
```

### Configuração WiFi

Edite `include/config.h`:

```cpp
#define WIFI_SSID           "SEU_SSID"
#define WIFI_PASSWORD       "SUA_SENHA"
#define HTTP_SERVER         "192.168.1.XXX"
#define HTTP_PORT           5000
#define HTTP_ENDPOINT       "/api/telemetria"
```

## 📊 Comparação de Consumo

| Modo | Display | Serial Log | Heap Check | Consumo Extra | Autonomia (2000mAh) |
|------|---------|------------|------------|---------------|---------------------|
| **PREFLIGHT** | Ativo (2s) | Verboso | 10s | ~18mA | ~90h |
| **FLIGHT** | OFF | Mínimo | 60s | ~0.5mA | ~110h (+22%) |

## 📝 Changelog

### v2.3.0 (2025-11-07)
- Implementado sistema dual-mode (PREFLIGHT verbose / FLIGHT lean)
- Adicionadas macros `LOG_PREFLIGHT()` / `LOG_FLIGHT()` / `LOG_ERROR()`
- Display automaticamente desligado em modo FLIGHT
- Otimização de heap check por modo de operação
- Redução de ~97% no overhead de logging em FLIGHT

### v2.2.0 (2025-11-04)
- I2C centralizado
- Sem race conditions
- Suporte a sensores opcionais (SHT20, CCS811, MPU9250)

## 💻 Estrutura do Projeto

```
AgroSat-IoT/
├── include/
│   ├── config.h              # Configurações globais + macros dual-mode
│   ├── TelemetryManager.h
│   ├── SensorManager.h
│   ├── PowerManager.h
│   ├── CommunicationManager.h
│   ├── StorageManager.h
│   ├── PayloadManager.h
│   └── SystemHealth.h
├── src/
│   ├── main.cpp
│   ├── TelemetryManager.cpp  # Lógica dual-mode
│   └── ... (outros managers)
├── platformio.ini
└── README.md
```

## 🔧 Desenvolvimento

### Compilar

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

### Monitor Serial

```bash
pio device monitor --baud 115200
```

## 💡 Dicas de Uso

1. **Teste em PREFLIGHT primeiro**: Valide todos os sensores e comunicação antes de iniciar missão
2. **Monitore heap mínimo**: Se < 10KB, risco de crash
3. **Verifique bateria**: Nunca inicie missão com < 70%
4. **SD Card**: Formatado FAT32, máx 32GB
5. **WiFi**: Conectar em PREFLIGHT evita timeout em FLIGHT

## 📝 Licença

Projeto desenvolvido para OBSAT MCTI - Equipe Orbitalis

---

**Contato**: [GitHub Issues](https://github.com/mathasilv/AgroSat-IoT/issues)