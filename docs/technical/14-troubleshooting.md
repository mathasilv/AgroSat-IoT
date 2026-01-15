# Documentação Técnica AgroSat-IoT

## Parte 14: Guia de Troubleshooting

### 14.1 Visão Geral

Este guia apresenta soluções para problemas comuns encontrados durante o desenvolvimento, testes e operação do AgroSat-IoT.

### 14.2 Problemas de Inicialização

#### Sistema não inicia / Reinicia em loop

**Sintomas:**
- LED não pisca
- Nenhuma saída na Serial
- Reset contínuo

**Causas e Soluções:**

| Causa | Diagnóstico | Solução |
|-------|-------------|---------|
| Watchdog timeout | Serial mostra "rst:0x10 (RTCWDT_RTC_RESET)" | Aumentar timeout WDT |
| Stack overflow | "Guru Meditation Error: Core X panic'ed (Unhandled debug exception)" | Aumentar stack da task |
| Falha de memória | "Heap allocation failed" | Reduzir buffers |
| GPIO2 em HIGH | Não entra em modo download | Verificar SD Card |

```cpp
// Verificar razão do reset
void printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON: Serial.println("Power-on reset"); break;
        case ESP_RST_SW: Serial.println("Software reset"); break;
        case ESP_RST_PANIC: Serial.println("Exception/panic reset"); break;
        case ESP_RST_INT_WDT: Serial.println("Interrupt WDT reset"); break;
        case ESP_RST_TASK_WDT: Serial.println("Task WDT reset"); break;
        case ESP_RST_WDT: Serial.println("Other WDT reset"); break;
        default: Serial.println("Unknown reset"); break;
    }
}
```

#### Falha na inicialização de sensores

**Sintomas:**
- "Sensor X not found"
- Leituras retornam 0 ou NaN

**Diagnóstico com Scanner I2C:**

```cpp
void scanI2C() {
    Serial.println("Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Device found at 0x%02X\n", addr);
        }
    }
}
```

**Endereços esperados:**

| Sensor | Endereço | Se não encontrado |
|--------|----------|-------------------|
| MPU9250 | 0x68 ou 0x69 | Verificar AD0 |
| BMP280 | 0x76 ou 0x77 | Verificar SDO |
| SI7021 | 0x40 | Verificar conexão |
| CCS811 | 0x5A ou 0x5B | Verificar ADDR |
| DS3231 | 0x68 | Conflito com MPU! |

### 14.3 Problemas de Comunicação

#### LoRa não transmite

**Sintomas:**
- "LoRa init failed"
- Pacotes não são recebidos
- RSSI sempre -157 dBm

**Checklist:**

```
□ Verificar conexões SPI (CS, RST, DIO0)
□ Verificar antena conectada (NUNCA transmitir sem antena!)
□ Verificar frequência (915 MHz para Brasil)
□ Verificar duty cycle não excedido
□ Verificar sync word compatível
```

**Teste de LoRa:**

```cpp
bool testLoRa() {
    if (!LoRa.begin(915E6)) {
        Serial.println("LoRa init failed!");
        return false;
    }
    
    // Enviar pacote de teste
    LoRa.beginPacket();
    LoRa.print("TEST");
    int result = LoRa.endPacket();
    
    Serial.printf("LoRa TX result: %d\n", result);
    return result == 1;
}
```

#### WiFi não conecta

**Sintomas:**
- "WiFi connection timeout"
- Status WL_DISCONNECTED

**Diagnóstico:**

```cpp
void printWiFiStatus() {
    Serial.printf("WiFi Status: %d\n", WiFi.status());
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
}

// Status codes:
// 0 = WL_IDLE_STATUS
// 1 = WL_NO_SSID_AVAIL
// 3 = WL_CONNECTED
// 4 = WL_CONNECT_FAILED
// 6 = WL_DISCONNECTED
```

**Soluções comuns:**

| Problema | Solução |
|----------|---------|
| SSID não encontrado | Verificar nome exato (case-sensitive) |
| Senha incorreta | Verificar caracteres especiais |
| Muitas redes | Usar WiFi.begin(ssid, pass, channel, bssid) |
| Interferência | Mudar canal do roteador |

#### HTTP falha

**Sintomas:**
- "HTTP POST failed"
- Timeout na requisição
- Código de erro -1

**Diagnóstico:**

```cpp
void testHTTP() {
    HTTPClient http;
    http.begin("http://obsat.org.br/teste_post/envio.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(10000);
    
    int code = http.POST("equession=666&teste=1");
    Serial.printf("HTTP Response: %d\n", code);
    
    if (code > 0) {
        Serial.println(http.getString());
    } else {
        Serial.printf("Error: %s\n", http.errorToString(code).c_str());
    }
    http.end();
}
```

### 14.4 Problemas de Sensores

#### MPU9250 - Leituras erráticas

**Sintomas:**
- Valores de aceleração muito altos
- Giroscópio com drift excessivo
- Magnetômetro saturado

**Soluções:**

```cpp
// 1. Verificar escala configurada
mpu.setAccelRange(MPU9250::ACCEL_RANGE_4G);
mpu.setGyroRange(MPU9250::GYRO_RANGE_500DPS);

// 2. Aplicar filtro passa-baixa
mpu.setDLPFBandwidth(MPU9250::DLPF_BANDWIDTH_20HZ);

// 3. Calibrar magnetômetro
// Comando: CALIB_MAG (girar em 8 por 20s)

// 4. Verificar interferência magnética
// Afastar de motores, alto-falantes, ímãs
```

#### BMP280 - Altitude incorreta

**Sintomas:**
- Altitude negativa ou muito alta
- Variação excessiva

**Solução - Calibrar pressão de referência:**

```cpp
// Definir pressão ao nível do mar local
float seaLevelPressure = 1013.25; // hPa padrão

// Ou calcular a partir de altitude conhecida
float knownAltitude = 850.0; // metros
float currentPressure = bmp.readPressure() / 100.0;
seaLevelPressure = currentPressure / pow(1.0 - (knownAltitude / 44330.0), 5.255);

// Usar para calcular altitude
float altitude = 44330.0 * (1.0 - pow(currentPressure / seaLevelPressure, 0.1903));
```

#### CCS811 - eCO2 sempre 400 ppm

**Sintomas:**
- Valores fixos em 400/0
- "CCS811 not ready"

**Soluções:**

```cpp
// 1. Aguardar aquecimento (20 min primeira vez, 5 min após)
if (millis() < 300000) {
    Serial.println("CCS811 warming up...");
    return;
}

// 2. Verificar modo de operação
ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);

// 3. Fornecer dados de compensação
ccs.setEnvironmentalData(humidity, temperature);

// 4. Salvar/restaurar baseline após 24h de operação
// Comando: SAVE_BASELINE
```

#### GPS - Sem fix

**Sintomas:**
- Satélites = 0
- Lat/Lon = 0.0
- gpsFix = false

**Checklist:**

```
□ Antena GPS com visão do céu
□ Aguardar cold start (até 15 min)
□ Verificar baudrate (9600 padrão)
□ Verificar conexão TX/RX (cruzada)
□ Verificar alimentação (3.3V estável)
```

**Diagnóstico:**

```cpp
void debugGPS() {
    Serial.println("=== GPS Debug ===");
    Serial.printf("Chars processed: %lu\n", gps.charsProcessed());
    Serial.printf("Sentences OK: %lu\n", gps.sentencesWithFix());
    Serial.printf("Failed checksums: %lu\n", gps.failedChecksum());
    Serial.printf("Satellites: %d\n", gps.satellites.value());
    Serial.printf("HDOP: %.2f\n", gps.hdop.hdop());
}
```

### 14.5 Problemas de Memória

#### Heap fragmentation

**Sintomas:**
- Alocações falham mesmo com memória livre
- Sistema fica lento
- Crashes aleatórios

**Diagnóstico:**

```cpp
void printHeapInfo() {
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Min Free Heap: %u bytes\n", ESP.getMinFreeHeap());
    Serial.printf("Max Alloc Heap: %u bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("PSRAM Free: %u bytes\n", ESP.getFreePsram());
    
    // Fragmentação = 1 - (MaxAlloc / FreeHeap)
    float frag = 1.0 - ((float)ESP.getMaxAllocHeap() / ESP.getFreeHeap());
    Serial.printf("Fragmentation: %.1f%%\n", frag * 100);
}
```

**Soluções:**

1. **Usar alocação estática:**
```cpp
// Evitar
char* buffer = (char*)malloc(256);

// Preferir
static char buffer[256];
```

2. **Usar StaticJsonDocument:**
```cpp
// Evitar
DynamicJsonDocument doc(1024);

// Preferir
StaticJsonDocument<512> doc;
```

3. **Pré-alocar buffers:**
```cpp
// No início do programa
static TelemetryData telemetryBuffer;
static char jsonBuffer[512];
```

#### Stack overflow

**Sintomas:**
- "Stack canary watchpoint triggered"
- Crash em funções específicas

**Diagnóstico:**

```cpp
void printTaskStackInfo() {
    Serial.printf("Main stack high water: %u\n", 
                  uxTaskGetStackHighWaterMark(NULL));
    
    // Para tasks específicas
    TaskHandle_t handle = xTaskGetHandle("SensorsTask");
    if (handle) {
        Serial.printf("SensorsTask stack: %u\n",
                      uxTaskGetStackHighWaterMark(handle));
    }
}
```

**Solução - Aumentar stack:**

```cpp
// Antes
xTaskCreatePinnedToCore(sensorsTask, "Sensors", 4096, ...);

// Depois
xTaskCreatePinnedToCore(sensorsTask, "Sensors", 8192, ...);
```

### 14.6 Problemas de SD Card

#### SD não inicializa

**Sintomas:**
- "SD Card Mount Failed"
- "No SD card attached"

**Checklist:**

```
□ Cartão formatado como FAT32
□ Cartão < 32GB (FAT32 limit)
□ Conexões SPI corretas
□ Velocidade SPI adequada
□ GPIO2 não conflitante
```

**Teste:**

```cpp
bool testSD() {
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Mount Failed");
        return false;
    }
    
    Serial.printf("SD Type: %d\n", SD.cardType());
    Serial.printf("SD Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    
    // Teste de escrita
    File f = SD.open("/test.txt", FILE_WRITE);
    if (f) {
        f.println("Test");
        f.close();
        SD.remove("/test.txt");
        Serial.println("SD Write OK");
        return true;
    }
    return false;
}
```

#### Corrupção de arquivos

**Sintomas:**
- Arquivos truncados
- Dados corrompidos
- Arquivo não fecha

**Soluções:**

```cpp
// 1. Sempre fechar arquivos
File f = SD.open("/data.csv", FILE_APPEND);
if (f) {
    f.println(data);
    f.flush();  // Força escrita
    f.close();  // SEMPRE fechar!
}

// 2. Usar mutex para acesso concorrente
if (xSemaphoreTake(xSDMutex, pdMS_TO_TICKS(1000))) {
    // Operações no SD
    xSemaphoreGive(xSDMutex);
}

// 3. Verificar espaço antes de escrever
if (SD.totalBytes() - SD.usedBytes() < 1024) {
    Serial.println("SD Card full!");
}
```

### 14.7 Problemas de FreeRTOS

#### Deadlock

**Sintomas:**
- Sistema trava
- Watchdog dispara
- Tasks não executam

**Diagnóstico:**

```cpp
// Adicionar timeout em todos os mutex takes
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Operação
    xSemaphoreGive(xMutex);
} else {
    Serial.println("Mutex timeout - possible deadlock!");
    s_mutexTimeouts++;
}
```

**Prevenção:**

1. **Ordem consistente de aquisição:**
```cpp
// Sempre adquirir na mesma ordem
xSemaphoreTake(xI2CMutex, ...);
xSemaphoreTake(xDataMutex, ...);
// ... operações ...
xSemaphoreGive(xDataMutex);
xSemaphoreGive(xI2CMutex);
```

2. **Evitar mutex em ISR:**
```cpp
// Em ISR, usar FromISR
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

#### Fila cheia

**Sintomas:**
- Dados perdidos
- "Queue full" warnings

**Solução:**

```cpp
// Verificar antes de enviar
if (uxQueueSpacesAvailable(xHttpQueue) > 0) {
    xQueueSend(xHttpQueue, &msg, 0);
} else {
    Serial.println("HTTP Queue full, dropping packet");
    s_droppedPackets++;
}

// Ou usar timeout
if (xQueueSend(xHttpQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("Queue send timeout");
}
```

### 14.8 Tabela de Códigos de Erro

| Código | Módulo | Descrição | Ação |
|--------|--------|-----------|------|
| E001 | I2C | Sensor não responde | Verificar conexões |
| E002 | LoRa | Init falhou | Verificar SPI |
| E003 | SD | Mount falhou | Verificar cartão |
| E004 | WiFi | Conexão timeout | Verificar credenciais |
| E005 | HTTP | Request falhou | Verificar servidor |
| E006 | GPS | Sem fix | Verificar antena |
| E007 | Power | Bateria crítica | Carregar bateria |
| E008 | Memory | Heap baixo | Reduzir buffers |
| E009 | WDT | Watchdog reset | Verificar loops |
| E010 | Mutex | Timeout | Verificar deadlock |

### 14.9 Comandos de Diagnóstico

| Comando | Função |
|---------|--------|
| `STATUS` | Status de todos os sensores |
| `MUTEX_STATS` | Estatísticas de mutex |
| `DUTY_CYCLE` | Uso do duty cycle LoRa |
| `HELP` | Lista de comandos |

### 14.10 Logs Úteis para Debug

```cpp
// Habilitar logs detalhados temporariamente
#define DEBUG_VERBOSE 1

#if DEBUG_VERBOSE
    #define VERBOSE_LOG(...) DEBUG_PRINTF(__VA_ARGS__)
#else
    #define VERBOSE_LOG(...)
#endif

// Uso
VERBOSE_LOG("[SENSOR] MPU9250 raw: ax=%d ay=%d az=%d\n", ax, ay, az);
```

---

*Anterior: [13 - Configurações e Constantes](13-configuracoes-constantes.md)*

*Próxima parte: [15 - Referência de API](15-referencia-api.md)*