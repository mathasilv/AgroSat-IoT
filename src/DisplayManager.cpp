#include "DisplayManager.h"
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LINE_HEIGHT 8
#define MAX_LINES 8
#define CHAR_WIDTH 6
#define UPDATE_INTERVAL 1000

DisplayManager::DisplayManager() :
    _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST),
    _currentState(DISPLAY_BOOT),
    _lastTelemetryScreen(DISPLAY_TELEMETRY_1),
    _lastScreenChange(0),
    _screenInterval(UPDATE_INTERVAL),
    _isDisplayOn(false),
    _scrollOffset(0),
    _lastUpdateTime(0),
    _initLineCount(0)
{
}

bool DisplayManager::begin() {
    delay(100);
    
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false)) {
        DEBUG_PRINTLN("[DisplayManager] Falha ao inicializar display");
        return false;
    }
    
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setTextWrap(false);
    _display.display();
    
    _isDisplayOn = true;
    DEBUG_PRINTLN("[DisplayManager] Display inicializado com sucesso");
    
    return true;
}

void DisplayManager::clear() {
    if (!_isDisplayOn) return;
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::turnOff() {
    if (!_isDisplayOn) return;
    _display.clearDisplay();
    _display.display();
    _display.ssd1306_command(SSD1306_DISPLAYOFF);
    _isDisplayOn = false;
    DEBUG_PRINTLN("[DisplayManager] Display DESLIGADO");
}

void DisplayManager::turnOn() {
    if (_isDisplayOn) return;
    _display.ssd1306_command(SSD1306_DISPLAYON);
    _isDisplayOn = true;
    DEBUG_PRINTLN("[DisplayManager] Display LIGADO");
}

// ========================================
// TELA DE BOOT
// ========================================
void DisplayManager::showBoot() {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_BOOT;
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("===================");
    
    _display.setCursor(0, 10);
    _display.println("  AgroSat-IoT HAB");
    
    _display.setCursor(0, 20);
    _display.println("  Equipe Orbitalis");
    
    _display.setCursor(0, 30);
    _display.println("===================");
    
    _display.setCursor(0, 42);
    _display.println("Firmware: v6.0.1");
    
    _display.setCursor(0, 54);
    _display.println("Inicializando...");
    
    _display.display();
}

// ========================================
// INICIALIZAÇÃO DE SENSORES
// ========================================
void DisplayManager::showSensorInit(const char* sensorName, bool status) {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_INIT_SENSORS;
    
    // Primeira chamada - limpar e cabeçalho
    if (_initLineCount == 0) {
        _display.clearDisplay();
        _display.setTextSize(1);
        _display.setCursor(0, 0);
        _display.println("--- INIT SENSORS ---");
        _initLineCount = 1;
    }
    
    // Resetar se ultrapassar limite
    if (_initLineCount >= MAX_LINES) {
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.println("--- INIT SENSORS ---");
        _initLineCount = 1;
    }
    
    _display.setTextSize(1);
    _display.setCursor(0, _initLineCount * LINE_HEIGHT);
    
    char buffer[22];
    if (status) {
        snprintf(buffer, sizeof(buffer), "%-10s [OK]", sensorName);
    } else {
        snprintf(buffer, sizeof(buffer), "%-10s [FAIL]", sensorName);
    }
    
    _display.println(buffer);
    _initLineCount++;
    
    _display.display();
    delay(400);  // Tempo de visualização
}

// ========================================
// CALIBRAÇÃO COM BARRA DE PROGRESSO
// ========================================
void DisplayManager::showCalibration(uint8_t progress) {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_CALIBRATION;
    _display.clearDisplay();
    _display.setTextSize(1);
    
    // Cabeçalho
    _display.setCursor(0, 0);
    _display.println("CALIBRANDO MPU9250");
    
    // Instrução
    _display.setCursor(0, 12);
    _display.println("Rotacione em todos");
    _display.setCursor(0, 20);
    _display.println("os eixos (X,Y,Z)");
    
    // Barra de progresso
    const uint8_t barX = 4;
    const uint8_t barY = 36;
    const uint8_t barWidth = 120;
    const uint8_t barHeight = 14;
    
    if (progress > 100) progress = 100;
    
    // Borda dupla da barra
    _display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
    _display.drawRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2, SSD1306_WHITE);
    
    // Preenchimento
    uint8_t fillWidth = ((barWidth - 6) * progress) / 100;
    if (fillWidth > 0) {
        _display.fillRect(barX + 3, barY + 3, fillWidth, barHeight - 6, SSD1306_WHITE);
    }
    
    // Percentual abaixo da barra
    _display.setCursor(54, 54);
    _display.printf("%3d%%", progress);
    
    _display.display();
}

void DisplayManager::showCalibrationResult(float offsetX, float offsetY, float offsetZ) {
    if (!_isDisplayOn) return;
    
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("CALIBRACAO OK!");
    
    _display.setCursor(0, 14);
    _display.println("-------------------");
    
    char offsetStr[22];
    _display.setCursor(0, 24);
    snprintf(offsetStr, sizeof(offsetStr), "Mag X:%.0f uT", offsetX);
    _display.println(offsetStr);
    
    _display.setCursor(0, 34);
    snprintf(offsetStr, sizeof(offsetStr), "Mag Y:%.0f uT", offsetY);
    _display.println(offsetStr);
    
    _display.setCursor(0, 44);
    snprintf(offsetStr, sizeof(offsetStr), "Mag Z:%.0f uT", offsetZ);
    _display.println(offsetStr);
    
    _display.setCursor(0, 56);
    _display.println("Continuando...");
    
    _display.display();
    delay(1200);  // 1.2s é suficiente
}

// ========================================
// SISTEMA PRONTO
// ========================================
void DisplayManager::showReady() {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_READY;
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _initLineCount = 0;  // Reset contador
    
    _display.setCursor(0, 0);
    _display.println("===================");
    
    _display.setCursor(0, 14);
    _display.println("  SISTEMA PRONTO");
    
    _display.setCursor(0, 28);
    _display.println("===================");
    
    _display.setCursor(0, 42);
    _display.println("Iniciando missao...");
    
    _display.setCursor(0, 54);
    _display.println("Aguarde...");
    
    _display.display();
    delay(2000);
    
    _currentState = DISPLAY_TELEMETRY_1;
    _lastScreenChange = millis();
}

// ========================================
// TELEMETRIA CONTÍNUA
// ========================================
void DisplayManager::updateTelemetry(const TelemetryData& data) {
    if (!_isDisplayOn) return;
    
    uint32_t currentTime = millis();
    
    if (currentTime - _lastUpdateTime < UPDATE_INTERVAL) {
        return;
    }
    _lastUpdateTime = currentTime;
    
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextWrap(false);
    
    static const uint8_t MAX_DISPLAY_LINES = 30;
    String lines[MAX_DISPLAY_LINES];
    uint8_t lineCount = 0;
    
    // Cabeçalho
    lines[lineCount++] = "=== AgroSat HAB ===";
    
    // Tempo e bateria
    uint32_t seconds = data.missionTime / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Up:%02luh%02lum%02lus", hours, minutes, seconds);
    lines[lineCount++] = String(timeStr);
    
    char batStr[32];
    snprintf(batStr, sizeof(batStr), "Bat:%.2fV %.0f%%", data.batteryVoltage, data.batteryPercentage);
    lines[lineCount++] = String(batStr);
    
    lines[lineCount++] = "-------------------";
    
    // Temperatura
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "T:%.1fC", data.temperature);
    lines[lineCount++] = String(tempStr);
    
    if (!isnan(data.temperatureSI)) {
        char tempSIStr[32];
        snprintf(tempSIStr, sizeof(tempSIStr), "T_SI:%.1fC", data.temperatureSI);
        lines[lineCount++] = String(tempSIStr);
    }
    
    // Pressão e altitude
    char pressStr[32];
    snprintf(pressStr, sizeof(pressStr), "P:%.0fhPa", data.pressure);
    lines[lineCount++] = String(pressStr);
    
    char altStr[32];
    snprintf(altStr, sizeof(altStr), "Alt:%.0fm", data.altitude);
    lines[lineCount++] = String(altStr);
    
    // Umidade
    if (!isnan(data.humidity)) {
        char humStr[32];
        snprintf(humStr, sizeof(humStr), "Hum:%.0f%%", data.humidity);
        lines[lineCount++] = String(humStr);
    }
    
    // Qualidade do ar
    if (!isnan(data.co2)) {
        char co2Str[32];
        snprintf(co2Str, sizeof(co2Str), "CO2:%.0fppm", data.co2);
        lines[lineCount++] = String(co2Str);
    }
    
    if (!isnan(data.tvoc)) {
        char tvocStr[32];
        snprintf(tvocStr, sizeof(tvocStr), "VOC:%.0fppb", data.tvoc);
        lines[lineCount++] = String(tvocStr);
    }
    
    lines[lineCount++] = "-------------------";
    
    // Acelerômetro
    char accelStr[32];
    snprintf(accelStr, sizeof(accelStr), "Ax:%.1f", data.accelX);
    lines[lineCount++] = String(accelStr);
    
    snprintf(accelStr, sizeof(accelStr), "Ay:%.1f", data.accelY);
    lines[lineCount++] = String(accelStr);
    
    snprintf(accelStr, sizeof(accelStr), "Az:%.1f", data.accelZ);
    lines[lineCount++] = String(accelStr);
    
    // Giroscópio
    char gyroStr[32];
    snprintf(gyroStr, sizeof(gyroStr), "Gx:%.2f", data.gyroX);
    lines[lineCount++] = String(gyroStr);
    
    snprintf(gyroStr, sizeof(gyroStr), "Gy:%.2f", data.gyroY);
    lines[lineCount++] = String(gyroStr);
    
    snprintf(gyroStr, sizeof(gyroStr), "Gz:%.2f", data.gyroZ);
    lines[lineCount++] = String(gyroStr);
    
    // Magnetômetro
    if (!isnan(data.magX)) {
        char magStr[32];
        snprintf(magStr, sizeof(magStr), "Mx:%.0f", data.magX);
        lines[lineCount++] = String(magStr);
        
        snprintf(magStr, sizeof(magStr), "My:%.0f", data.magY);
        lines[lineCount++] = String(magStr);
        
        snprintf(magStr, sizeof(magStr), "Mz:%.0f", data.magZ);
        lines[lineCount++] = String(magStr);
    }
    
    lines[lineCount++] = "-------------------";
    
    // Status
    if (data.systemStatus == STATUS_OK) {
        lines[lineCount++] = "Stat:OK";
    } else {
        char statStr[32];
        snprintf(statStr, sizeof(statStr), "Err:0x%02X", data.systemStatus);
        lines[lineCount++] = String(statStr);
    }
    
    // Memória
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Mem:%luKB", ESP.getFreeHeap() / 1024);
    lines[lineCount++] = String(memStr);
    
    // Renderização com rolagem
    const uint8_t visibleLines = MAX_LINES;
    static uint8_t scrollPosition = 0;
    
    if (lineCount > visibleLines) {
        scrollPosition++;
        if (scrollPosition > lineCount - visibleLines) {
            scrollPosition = 0;
        }
    } else {
        scrollPosition = 0;
    }
    
    for (uint8_t i = 0; i < visibleLines && (scrollPosition + i) < lineCount; i++) {
        _display.setCursor(0, i * LINE_HEIGHT);
        _display.print(lines[scrollPosition + i]);
    }
    
    _display.display();
}

// Funções legadas (não usadas)
void DisplayManager::_showTelemetry1(const TelemetryData& data) {}
void DisplayManager::_showTelemetry2(const TelemetryData& data) {}
void DisplayManager::_showTelemetry3(const TelemetryData& data) {}
void DisplayManager::_showTelemetry4(const TelemetryData& data) {}
void DisplayManager::_showStatus(const TelemetryData& data) {}
void DisplayManager::_drawProgressBar(uint8_t progress, const char* label) {}

// ========================================
// MENSAGENS UTILITÁRIAS
// ========================================
void DisplayManager::displayMessage(const char* title, const char* msg) {
    if (!_isDisplayOn) return;
    
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("===================");
    
    uint8_t titleLen = strlen(title);
    uint8_t titleX = (21 - titleLen) / 2;
    _display.setCursor(titleX * CHAR_WIDTH, 14);
    _display.println(title);
    
    _display.setCursor(0, 28);
    _display.println("===================");
    
    uint8_t msgLen = strlen(msg);
    uint8_t msgX = (21 - msgLen) / 2;
    _display.setCursor(msgX * CHAR_WIDTH, 42);
    _display.println(msg);
    
    _display.display();
}

void DisplayManager::nextScreen() {
    _lastScreenChange = 0;
}

void DisplayManager::setScreen(DisplayState state) {
    _currentState = state;
    _lastScreenChange = millis();
}
