#include "DisplayManager.h"
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_DURATION 3000

DisplayManager::DisplayManager() :
    _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST),
    _currentState(DISPLAY_BOOT),
    _lastTelemetryScreen(DISPLAY_TELEMETRY_1),
    _lastScreenChange(0),
    _screenInterval(SCREEN_DURATION),
    _isDisplayOn(false)
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
// TELA DE BOOT - SEM CABEÇALHO
// ========================================
void DisplayManager::showBoot() {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_BOOT;
    _display.clearDisplay();
    
    _display.setTextSize(3);
    _display.setCursor(5, 8);
    _display.println("Agro");
    _display.setCursor(15, 32);
    _display.println("Sat");
    
    _display.setTextSize(1);
    _display.setCursor(25, 58);
    _display.println("HAB v6.0");
    
    _display.display();
}

// ========================================
// INICIALIZAÇÃO DE SENSORES - COMPACTO
// ========================================
void DisplayManager::showSensorInit(const char* sensorName, bool status) {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_INIT_SENSORS;
    
    static uint8_t line = 0;
    
    if (line == 0) {
        _display.clearDisplay();
        _display.setTextSize(1);
        _display.setCursor(0, 0);
        _display.println("Init Sensors:");
        line = 1;
    }
    
    if (line >= 8) {
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.println("Init Sensors:");
        line = 1;
    }
    
    _display.setTextSize(1);
    _display.setCursor(0, line * 8);
    _display.print(sensorName);
    _display.print(":");
    
    if (status) {
        _display.println("OK");
    } else {
        _display.println("FAIL");
    }
    
    line++;
    _display.display();
    delay(200);
}

// ========================================
// CALIBRAÇÃO - BARRA GRANDE
// ========================================
void DisplayManager::showCalibration(uint8_t progress) {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_CALIBRATION;
    _display.clearDisplay();
    
    _display.setTextSize(1);
    _display.setCursor(10, 5);
    _display.println("CALIBRANDO IMU");
    
    _display.setCursor(5, 20);
    _display.println("Rotacione em");
    _display.setCursor(5, 30);
    _display.println("todos os eixos");
    
    // Barra de progresso grande
    uint16_t barX = 10;
    uint16_t barY = 46;
    uint16_t barWidth = 108;
    uint16_t barHeight = 12;
    
    if (progress > 100) progress = 100;
    
    _display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
    
    uint16_t fillWidth = ((barWidth - 4) * progress) / 100;
    if (fillWidth > 0) {
        _display.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, SSD1306_WHITE);
    }
    
    _display.setTextSize(1);
    _display.setCursor(55, 44);
    _display.printf("%d%%", progress);
    
    _display.display();
}

void DisplayManager::showCalibrationResult(float offsetX, float offsetY, float offsetZ) {
    if (!_isDisplayOn) return;
    
    _display.clearDisplay();
    
    _display.setTextSize(2);
    _display.setCursor(15, 10);
    _display.println("CALIBR.");
    _display.setCursor(30, 30);
    _display.println("OK!");
    
    _display.setTextSize(1);
    _display.setCursor(10, 52);
    _display.printf("Mag:%.0f,%.0f,%.0f", offsetX, offsetY, offsetZ);
    
    _display.display();
    delay(2000);
}

// ========================================
// SISTEMA PRONTO
// ========================================
void DisplayManager::showReady() {
    if (!_isDisplayOn) return;
    
    _currentState = DISPLAY_READY;
    _display.clearDisplay();
    
    _display.setTextSize(3);
    _display.setCursor(15, 15);
    _display.println("READY");
    
    _display.setTextSize(1);
    _display.setCursor(30, 50);
    _display.println("Aguarde...");
    
    _display.display();
    delay(2000);
    
    _currentState = DISPLAY_TELEMETRY_1;
    _lastScreenChange = millis();
}

// ========================================
// ATUALIZAÇÃO DE TELEMETRIA
// ========================================
void DisplayManager::updateTelemetry(const TelemetryData& data) {
    if (!_isDisplayOn) return;
    
    uint32_t currentTime = millis();
    
    if (currentTime - _lastScreenChange >= _screenInterval) {
        _lastScreenChange = currentTime;
        
        switch (_currentState) {
            case DISPLAY_TELEMETRY_1: _currentState = DISPLAY_TELEMETRY_2; break;
            case DISPLAY_TELEMETRY_2: _currentState = DISPLAY_TELEMETRY_3; break;
            case DISPLAY_TELEMETRY_3: _currentState = DISPLAY_TELEMETRY_4; break;
            case DISPLAY_TELEMETRY_4: _currentState = DISPLAY_STATUS; break;
            case DISPLAY_STATUS:      _currentState = DISPLAY_TELEMETRY_1; break;
            default:                  _currentState = DISPLAY_TELEMETRY_1;
        }
    }
    
    switch (_currentState) {
        case DISPLAY_TELEMETRY_1: _showTelemetry1(data); break;
        case DISPLAY_TELEMETRY_2: _showTelemetry2(data); break;
        case DISPLAY_TELEMETRY_3: _showTelemetry3(data); break;
        case DISPLAY_TELEMETRY_4: _showTelemetry4(data); break;
        case DISPLAY_STATUS:      _showStatus(data); break;
        default: break;
    }
}

// ========================================
// TELA 1: TEMPERATURA - SEM CABEÇALHO
// ========================================
void DisplayManager::_showTelemetry1(const TelemetryData& data) {
    _display.clearDisplay();
    _display.setTextSize(1);
    
    // Linha 1: BMP280
    _display.setCursor(0, 0);
    _display.printf("BMP: %.2f C", data.temperature);
    
    // Linha 2: SI7021
    _display.setCursor(0, 12);
    if (!isnan(data.temperatureSI)) {
        _display.printf("SI7: %.2f C", data.temperatureSI);
    } else {
        _display.println("SI7: N/A");
    }
    
    // Linha 3: Delta
    _display.setCursor(0, 24);
    if (!isnan(data.temperatureSI)) {
        float delta = data.temperature - data.temperatureSI;
        _display.printf("Delta: %.2f C", delta);
    }
    
    // Linha 4: Pressão
    _display.setCursor(0, 40);
    _display.printf("P: %.0f hPa", data.pressure);
    
    // Linha 5: Altitude
    _display.setCursor(0, 52);
    _display.printf("Alt: %.0f m", data.altitude);
    
    _display.display();
}

// ========================================
// TELA 2: GIROSCÓPIO - SEM CABEÇALHO
// ========================================
void DisplayManager::_showTelemetry2(const TelemetryData& data) {
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("GYRO (rad/s)");
    
    _display.setCursor(0, 16);
    _display.printf("X: %+.3f", data.gyroX);
    
    _display.setCursor(0, 30);
    _display.printf("Y: %+.3f", data.gyroY);
    
    _display.setCursor(0, 44);
    _display.printf("Z: %+.3f", data.gyroZ);
    
    _display.display();
}

// ========================================
// TELA 3: ACELERÔMETRO - SEM CABEÇALHO
// ========================================
void DisplayManager::_showTelemetry3(const TelemetryData& data) {
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("ACCEL (m/s2)");
    
    _display.setCursor(0, 16);
    _display.printf("X: %+.2f", data.accelX);
    
    _display.setCursor(0, 30);
    _display.printf("Y: %+.2f", data.accelY);
    
    _display.setCursor(0, 44);
    _display.printf("Z: %+.2f", data.accelZ);
    
    _display.display();
}

// ========================================
// TELA 4: QUALIDADE DO AR - SEM CABEÇALHO
// ========================================
void DisplayManager::_showTelemetry4(const TelemetryData& data) {
    _display.clearDisplay();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.println("AMBIENTE");
    
    // Umidade
    _display.setCursor(0, 14);
    if (!isnan(data.humidity)) {
        _display.printf("Umid: %.1f %%", data.humidity);
    } else {
        _display.println("Umid: N/A");
    }
    
    // CO2
    _display.setCursor(0, 28);
    if (!isnan(data.co2)) {
        _display.printf("CO2: %.0f ppm", data.co2);
    } else {
        _display.println("CO2: N/A");
    }
    
    // TVOC
    _display.setCursor(0, 42);
    if (!isnan(data.tvoc)) {
        _display.printf("TVOC: %.0f ppb", data.tvoc);
    } else {
        _display.println("TVOC: N/A");
    }
    
    // Magnetômetro (magnitude)
    _display.setCursor(0, 56);
    if (!isnan(data.magX)) {
        float magMag = sqrt(data.magX*data.magX + data.magY*data.magY + data.magZ*data.magZ);
        _display.printf("Mag: %.0f uT", magMag);
    } else {
        _display.println("Mag: N/A");
    }
    
    _display.display();
}

// ========================================
// TELA 5: STATUS - SEM CABEÇALHO
// ========================================
void DisplayManager::_showStatus(const TelemetryData& data) {
    _display.clearDisplay();
    _display.setTextSize(1);
    
    // Linha 1: Bateria
    _display.setCursor(0, 0);
    _display.printf("Bat: %.2fV", data.batteryVoltage);
    
    _display.setCursor(0, 12);
    _display.printf("     %.0f %%", data.batteryPercentage);
    
    // Linha 3: Uptime
    _display.setCursor(0, 26);
    uint32_t seconds = data.missionTime / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    
    if (hours > 0) {
        _display.printf("Up: %02luh%02lum", hours, minutes);
    } else {
        _display.printf("Up: %02lum%02lus", minutes, seconds);
    }
    
    // Linha 4: Status
    _display.setCursor(0, 40);
    if (data.systemStatus == STATUS_OK) {
        _display.println("Status: OK");
    } else {
        _display.printf("Err: 0x%02X", data.systemStatus);
    }
    
    // Linha 5: Memória
    _display.setCursor(0, 54);
    _display.printf("Mem: %lu KB", ESP.getFreeHeap() / 1024);
    
    _display.display();
}

// ========================================
// UTILITÁRIOS - REMOVIDO _drawHeader()
// ========================================
void DisplayManager::_drawProgressBar(uint8_t progress, const char* label) {
    // Não usado mais (integrado em showCalibration)
}

void DisplayManager::displayMessage(const char* title, const char* msg) {
    if (!_isDisplayOn) return;
    
    _display.clearDisplay();
    
    // Título grande centralizado
    _display.setTextSize(2);
    uint8_t titleLen = strlen(title);
    uint8_t titleX = (128 - titleLen * 12) / 2;
    _display.setCursor(titleX, 10);
    _display.println(title);
    
    // Mensagem pequena centralizada
    _display.setTextSize(1);
    uint8_t msgLen = strlen(msg);
    uint8_t msgX = (128 - msgLen * 6) / 2;
    _display.setCursor(msgX, 40);
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
