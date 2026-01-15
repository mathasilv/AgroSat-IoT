# Documentação Técnica AgroSat-IoT

## Parte 17: Guia de Desenvolvimento

### 17.1 Visão Geral

Este guia fornece instruções para desenvolvedores que desejam contribuir, modificar ou estender o sistema AgroSat-IoT.

### 17.2 Ambiente de Desenvolvimento

#### Requisitos

| Software | Versão | Descrição |
|----------|--------|-----------|
| PlatformIO | >= 6.0 | Build system |
| VS Code | >= 1.80 | IDE recomendada |
| Git | >= 2.30 | Controle de versão |
| Python | >= 3.8 | Scripts auxiliares |

#### Instalação do Ambiente

```bash
# 1. Instalar VS Code
# https://code.visualstudio.com/

# 2. Instalar extensão PlatformIO
# Extensions -> Search "PlatformIO IDE" -> Install

# 3. Clonar repositório
git clone https://github.com/agrosat/agrosat-iot.git
cd agrosat-iot

# 4. Abrir no VS Code
code .

# 5. PlatformIO irá baixar dependências automaticamente
```

#### Estrutura do platformio.ini

```ini
[env:ttgo-lora32-v21]
platform = espressif32
board = ttgo-lora32-v21
framework = arduino

; Configurações de build
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM=0

; Bibliotecas
lib_deps = 
    adafruit/RTClib@^2.1.4
    sandeepmistry/LoRa@^0.8.0
    bblanchon/ArduinoJson@^6.21.3
    mikalhart/TinyGPSPlus@^1.0.0

; Monitor serial
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

; Upload
upload_speed = 921600
```

### 17.3 Convenções de Código

#### Nomenclatura

```cpp
// Classes: PascalCase
class TelemetryManager { };
class GroundNodeManager { };

// Métodos: camelCase
void startMission();
bool handleCommand(const String& cmd);

// Variáveis locais: camelCase
int packetCount = 0;
float temperature = 25.0f;

// Variáveis membro: _prefixo
class Example {
private:
    int _counter;
    float _lastValue;
};

// Constantes: UPPER_SNAKE_CASE
#define MAX_BUFFER_SIZE 256
const int DEFAULT_TIMEOUT = 1000;

// Enums: PascalCase com valores UPPER_SNAKE_CASE
enum class OperationMode : uint8_t {
    MODE_INIT = 0,
    MODE_PREFLIGHT = 1,
    MODE_FLIGHT = 2
};
```

#### Formatação

```cpp
// Indentação: 4 espaços (não tabs)
void example() {
    if (condition) {
        doSomething();
    }
}

// Chaves: estilo Allman para funções, K&R para blocos
void function()
{
    if (x) {
        // ...
    } else {
        // ...
    }
}

// Limite de linha: 100 caracteres
// Quebrar linhas longas de forma legível
bool result = veryLongFunctionName(
    parameter1,
    parameter2,
    parameter3
);
```

#### Documentação

```cpp
/**
 * @brief Descrição breve da função
 * 
 * @details Descrição detalhada se necessário.
 *          Pode ocupar múltiplas linhas.
 * 
 * @param param1 Descrição do parâmetro
 * @param param2 Descrição do parâmetro
 * 
 * @return Descrição do retorno
 * 
 * @note Notas importantes
 * @warning Avisos de uso
 * 
 * @example
 * ```cpp
 * int result = myFunction(10, "test");
 * ```
 */
int myFunction(int param1, const char* param2);
```

### 17.4 Arquitetura de Módulos

#### Estrutura de um Módulo

```
src/
└── categoria/
    └── NomeModulo/
        ├── NomeModulo.h      # Interface pública
        └── NomeModulo.cpp    # Implementação
```

#### Template de Header (.h)

```cpp
/**
 * @file NomeModulo.h
 * @brief Descrição do módulo
 * @version 1.0.0
 */

#ifndef NOME_MODULO_H
#define NOME_MODULO_H

#include <Arduino.h>

/**
 * @class NomeModulo
 * @brief Descrição da classe
 */
class NomeModulo {
public:
    /**
     * @brief Construtor
     */
    NomeModulo();
    
    /**
     * @brief Inicializa o módulo
     * @return true se sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza o módulo (chamar em loop)
     */
    void update();

private:
    bool _initialized;
    // Membros privados
};

#endif // NOME_MODULO_H
```

#### Template de Implementação (.cpp)

```cpp
/**
 * @file NomeModulo.cpp
 * @brief Implementação do NomeModulo
 */

#include "NomeModulo.h"
#include "config.h"

NomeModulo::NomeModulo() 
    : _initialized(false)
{
}

bool NomeModulo::begin() {
    if (_initialized) {
        return true;  // Já inicializado
    }
    
    // Inicialização...
    
    _initialized = true;
    DEBUG_PRINTLN("[NomeModulo] Inicializado");
    return true;
}

void NomeModulo::update() {
    if (!_initialized) return;
    
    // Lógica de atualização...
}
```

### 17.5 Adicionando um Novo Sensor

#### Passo 1: Criar o Driver

```cpp
// src/sensors/NovoSensor/NovoSensor.h
#ifndef NOVO_SENSOR_H
#define NOVO_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class NovoSensor {
public:
    static const uint8_t I2C_ADDRESS = 0x50;
    
    bool begin(TwoWire& wire = Wire);
    bool read(float& value1, float& value2);
    bool isOnline();
    
private:
    TwoWire* _wire;
    bool _online;
};

#endif
```

```cpp
// src/sensors/NovoSensor/NovoSensor.cpp
#include "NovoSensor.h"
#include "config/debug.h"

bool NovoSensor::begin(TwoWire& wire) {
    _wire = &wire;
    
    _wire->beginTransmission(I2C_ADDRESS);
    _online = (_wire->endTransmission() == 0);
    
    if (_online) {
        DEBUG_PRINTLN("[NovoSensor] Inicializado");
    } else {
        DEBUG_PRINTLN("[NovoSensor] Não encontrado!");
    }
    
    return _online;
}

bool NovoSensor::read(float& value1, float& value2) {
    if (!_online) return false;
    
    // Implementar leitura...
    
    return true;
}

bool NovoSensor::isOnline() {
    return _online;
}
```

#### Passo 2: Integrar ao SensorManager

```cpp
// Em SensorManager.h, adicionar:
#include "sensors/NovoSensor/NovoSensor.h"

class SensorManager {
    // ...
private:
    NovoSensor _novoSensor;
};

// Em SensorManager.cpp, no begin():
if (!_novoSensor.begin(Wire)) {
    DEBUG_PRINTLN("[SM] NovoSensor falhou");
}

// Em readAll():
float v1, v2;
if (_novoSensor.read(v1, v2)) {
    data.novoValor1 = v1;
    data.novoValor2 = v2;
}
```

#### Passo 3: Atualizar TelemetryTypes.h

```cpp
struct TelemetryData {
    // ... campos existentes ...
    
    // Novo sensor
    float novoValor1;
    float novoValor2;
};
```

### 17.6 Adicionando um Novo Comando

#### Passo 1: Implementar no CommandHandler

```cpp
// Em CommandHandler.cpp
bool CommandHandler::handle(String cmd) {
    // ... comandos existentes ...
    
    if (cmd == "NOVO_COMANDO") {
        _handleNovoComando();
        return true;
    }
    
    if (cmd.startsWith("SET_PARAM ")) {
        String param = cmd.substring(10);
        _handleSetParam(param);
        return true;
    }
    
    return false;
}

void CommandHandler::_handleNovoComando() {
    DEBUG_PRINTLN("[CMD] Executando novo comando...");
    // Implementação...
    DEBUG_PRINTLN("[CMD] Concluído");
}

void CommandHandler::_handleSetParam(const String& param) {
    int value = param.toInt();
    DEBUG_PRINTF("[CMD] Parâmetro definido: %d\n", value);
    // Implementação...
}
```

#### Passo 2: Atualizar Menu de Ajuda

```cpp
void CommandHandler::_printHelp() {
    DEBUG_PRINTLN("--- COMANDOS DISPONIVEIS ---");
    // ... comandos existentes ...
    DEBUG_PRINTLN("  NOVO_COMANDO    : Descrição do comando");
    DEBUG_PRINTLN("  SET_PARAM <val> : Define parâmetro");
    DEBUG_PRINTLN("----------------------------");
}
```

### 17.7 Criando uma Nova Task FreeRTOS

```cpp
// Definir handle global
TaskHandle_t xMinhaTaskHandle = NULL;

// Função da task
void minhaTask(void* parameter) {
    // Inicialização única
    DEBUG_PRINTLN("[MinhaTask] Iniciada");
    
    for (;;) {
        // Aguardar evento ou timeout
        if (xQueueReceive(xMinhaQueue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Processar mensagem
            processarMensagem(msg);
        }
        
        // Ou executar periodicamente
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Feed watchdog se necessário
        esp_task_wdt_reset();
    }
}

// Criar task no setup()
void setup() {
    // ...
    
    xTaskCreatePinnedToCore(
        minhaTask,           // Função
        "MinhaTask",         // Nome
        4096,                // Stack (bytes)
        NULL,                // Parâmetro
        1,                   // Prioridade
        &xMinhaTaskHandle,   // Handle
        0                    // Core (0 ou 1)
    );
}
```

### 17.8 Testes

#### Teste Unitário de Módulo

```cpp
// test/test_sensor.cpp
#include <unity.h>
#include "sensors/NovoSensor/NovoSensor.h"

void test_sensor_init() {
    NovoSensor sensor;
    // Mock ou hardware real
    TEST_ASSERT_TRUE(sensor.begin());
}

void test_sensor_read() {
    NovoSensor sensor;
    sensor.begin();
    
    float v1, v2;
    TEST_ASSERT_TRUE(sensor.read(v1, v2));
    TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0, v1);  // Esperado ~25
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_init);
    RUN_TEST(test_sensor_read);
    UNITY_END();
}

void loop() {}
```

#### Executar Testes

```bash
# Via PlatformIO
pio test -e native

# Ou no hardware
pio test -e ttgo-lora32-v21
```

### 17.9 Debug e Profiling

#### Habilitar Logs Detalhados

```cpp
// Em platformio.ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  ; Verbose
    -DDEBUG_VERBOSE=1
```

#### Medir Tempo de Execução

```cpp
void measureFunction() {
    unsigned long start = micros();
    
    // Código a medir
    doSomething();
    
    unsigned long elapsed = micros() - start;
    DEBUG_PRINTF("Tempo: %lu us\n", elapsed);
}
```

#### Monitor de Stack

```cpp
void printStackUsage() {
    DEBUG_PRINTF("Stack livre (main): %u\n", 
                 uxTaskGetStackHighWaterMark(NULL));
    
    if (xSensorsTaskHandle) {
        DEBUG_PRINTF("Stack livre (sensors): %u\n",
                     uxTaskGetStackHighWaterMark(xSensorsTaskHandle));
    }
}
```

#### Analisador de Heap

```cpp
void printHeapAnalysis() {
    DEBUG_PRINTLN("=== HEAP ANALYSIS ===");
    DEBUG_PRINTF("Total: %u\n", ESP.getHeapSize());
    DEBUG_PRINTF("Free: %u\n", ESP.getFreeHeap());
    DEBUG_PRINTF("Min Free: %u\n", ESP.getMinFreeHeap());
    DEBUG_PRINTF("Max Alloc: %u\n", ESP.getMaxAllocHeap());
    
    float frag = 1.0 - ((float)ESP.getMaxAllocHeap() / ESP.getFreeHeap());
    DEBUG_PRINTF("Fragmentation: %.1f%%\n", frag * 100);
    DEBUG_PRINTLN("====================");
}
```

### 17.10 Boas Práticas

#### Gerenciamento de Memória

```cpp
// ✓ BOM: Alocação estática
static char buffer[256];
static TelemetryData dataBuffer;

// ✗ EVITAR: Alocação dinâmica frequente
char* buffer = new char[256];  // Evitar
delete[] buffer;

// ✓ BOM: StaticJsonDocument
StaticJsonDocument<512> doc;

// ✗ EVITAR: DynamicJsonDocument em loops
DynamicJsonDocument doc(1024);  // Evitar em loops
```

#### Thread Safety

```cpp
// ✓ BOM: Sempre usar mutex com timeout
if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Operação protegida
    sharedData = newValue;
    xSemaphoreGive(xDataMutex);
} else {
    DEBUG_PRINTLN("Mutex timeout!");
}

// ✗ EVITAR: Mutex sem timeout (pode travar)
xSemaphoreTake(xDataMutex, portMAX_DELAY);  // Perigoso!
```

#### Validação de Dados

```cpp
// ✓ BOM: Sempre validar entrada
bool processData(float value) {
    if (isnan(value) || isinf(value)) {
        return false;
    }
    if (value < MIN_VALID || value > MAX_VALID) {
        return false;
    }
    // Processar...
    return true;
}

// ✓ BOM: Verificar ponteiros
void processBuffer(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }
    // Processar...
}
```

#### Economia de Energia

```cpp
// ✓ BOM: Desabilitar periféricos não usados
void enterLowPowerMode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();
    
    // Reduzir frequência CPU
    setCpuFrequencyMhz(80);
}

// ✓ BOM: Usar light sleep quando possível
void waitWithLowPower(uint32_t ms) {
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_light_sleep_start();
}
```

### 17.11 Checklist de Pull Request

Antes de submeter um PR, verifique:

```markdown
## Checklist

### Código
- [ ] Segue convenções de nomenclatura
- [ ] Documentação Doxygen em funções públicas
- [ ] Sem warnings de compilação
- [ ] Sem memory leaks (verificar heap)

### Testes
- [ ] Testado em hardware real
- [ ] Testado em todos os modos (PREFLIGHT, FLIGHT, SAFE)
- [ ] Verificado uso de memória
- [ ] Verificado comportamento com sensores offline

### Documentação
- [ ] README atualizado se necessário
- [ ] Documentação técnica atualizada
- [ ] Changelog atualizado

### Segurança
- [ ] Sem credenciais hardcoded
- [ ] Validação de entrada implementada
- [ ] Timeouts em todas operações bloqueantes
```

### 17.12 Recursos Adicionais

#### Links Úteis

- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [LoRa Alliance](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)
- [PlatformIO Docs](https://docs.platformio.org/)

#### Ferramentas Recomendadas

| Ferramenta | Uso |
|------------|-----|
| Logic Analyzer | Debug de I2C/SPI |
| SDR (RTL-SDR) | Análise de RF LoRa |
| Serial Plotter | Visualização de dados |
| Wireshark | Análise de pacotes WiFi |

---

*Anterior: [16 - Fluxogramas e Diagramas](16-fluxogramas.md)*

---

## Índice da Documentação

1. [Visão Geral e Arquitetura](01-visao-geral.md)
2. [Hardware e Pinagem](02-hardware-pinagem.md)
3. [Modos de Operação](03-modos-operacao.md)
4. [Sensores e Coleta](04-sensores-coleta.md)
5. [Comunicação LoRa e WiFi](05-comunicacao.md)
6. [Armazenamento e Persistência](06-armazenamento.md)
7. [Sistema de Energia](07-energia-bateria.md)
8. [Ground Nodes e Relay](08-ground-nodes-relay.md)
9. [Controle de Missão](09-controle-missao.md)
10. [Sistema de Saúde](10-sistema-saude.md)
11. [Comandos e Interface](11-comandos-interface.md)
12. [Estruturas de Dados](12-estruturas-dados.md)
13. [Configurações e Constantes](13-configuracoes-constantes.md)
14. [Guia de Troubleshooting](14-troubleshooting.md)
15. [Referência de API](15-referencia-api.md)
16. [Fluxogramas e Diagramas](16-fluxogramas.md)
17. [Guia de Desenvolvimento](17-guia-desenvolvimento.md)