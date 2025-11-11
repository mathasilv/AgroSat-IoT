#include "DisplayManager.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Tempo de exibição de cada tela (em ms)
#define SCREEN_DURATION 3000  // 3 segundos por tela

DisplayManager::DisplayManager() :
    _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
    _currentState(DISPLAY_BOOT),
    _lastTelemetryScreen(DISPLAY_TELEMETRY_1),
    _lastScreenChange(0),
    _screenInterval(SCREEN_DURATION),
    _isDisplayOn(false)  // ✅ NOVO: Inicia desligado
{
}

bool DisplayManager::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        return false;
    }
    
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.display();
    
    _isDisplayOn = true;  // ✅ Marca como ligado após init
    
    return true;
}

void DisplayManager::clear() {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _display.clearDisplay();
    _display.display();
}

// ✅ NOVO: Desligamento físico total
void DisplayManager::turnOff() {
    _display.clearDisplay();
    _display.display();
    _display.ssd1306_command(SSD1306_DISPLAYOFF);  // Comando físico de desligamento
    _isDisplayOn = false;
    
    DEBUG_PRINTLN("[DisplayManager] Display DESLIGADO fisicamente");
}

// ✅ NOVO: Religamento
void DisplayManager::turnOn() {
    _display.ssd1306_command(SSD1306_DISPLAYON);
    _isDisplayOn = true;
    
    DEBUG_PRINTLN("[DisplayManager] Display LIGADO");
}

// ========================================
// TELA DE BOOT
// ========================================
void DisplayManager::showBoot() {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _currentState = DISPLAY_BOOT;
    _display.clearDisplay();
    
    // Logo/Título
    _display.setTextSize(2);
    _display.setCursor(10, 5);
    _display.println("AgroSat");
    
    _display.setTextSize(1);
    _display.setCursor(15, 25);
    _display.println("OBSAT Fase 2");
    
    _display.setCursor(10, 40);
    _display.println("Equipe: 666");
    
    _display.setCursor(15, 52);
    _display.println("Inicializando...");
    
    _display.display();
}

// ========================================
// INICIALIZAÇÃO DE SENSORES
// ========================================
void DisplayManager::showSensorInit(const char* sensorName, bool status) {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _currentState = DISPLAY_INIT_SENSORS;
    
    // Não limpar tela, apenas adicionar linha
    _display.setCursor(0, 0);
    _display.setTextSize(1);
    
    // Rolar tela se necessário (simulação de log)
    static uint8_t line = 0;
    
    if (line >= 8) {
        _display.clearDisplay();
        line = 0;
    }
    
    _display.setCursor(0, line * 8);
    _display.print(sensorName);
    _display.print(": ");
    
    if (status) {
        _display.println("OK");
    } else {
        _display.println("FALHOU");
    }
    
    line++;
    _display.display();
}

// ========================================
// CALIBRAÇÃO DO IMU
// ========================================
void DisplayManager::showCalibration(uint8_t progress) {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _currentState = DISPLAY_CALIBRATION;
    _display.clearDisplay();
    
    _drawHeader("CALIBRACAO IMU");
    
    _display.setTextSize(1);
    _display.setCursor(5, 20);
    _display.println("Rotacione o");
    _display.setCursor(5, 30);
    _display.println("CubeSat em");
    _display.setCursor(5, 40);
    _display.println("todos os eixos");
    
    _drawProgressBar(progress, "");
    
    _display.display();
}

void DisplayManager::showCalibrationResult(float offsetX, float offsetY, float offsetZ) {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _display.clearDisplay();
    
    _drawHeader("CALIBRADO!");
    
    _display.setTextSize(1);
    _display.setCursor(0, 20);
    _display.printf("X: %.1f uT\n", offsetX);
    _display.setCursor(0, 30);
    _display.printf("Y: %.1f uT\n", offsetY);
    _display.setCursor(0, 40);
    _display.printf("Z: %.1f uT\n", offsetZ);
    
    _display.display();
    delay(2000);  // Mostrar resultado por 2s
}

// ========================================
// SISTEMA PRONTO
// ========================================
void DisplayManager::showReady() {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _currentState = DISPLAY_READY;
    _display.clearDisplay();
    
    _display.setTextSize(2);
    _display.setCursor(20, 15);
    _display.println("PRONTO!");
    
    _display.setTextSize(1);
    _display.setCursor(10, 40);
    _display.println("Aguardando...");
    
    _display.display();
    delay(2000);
    
    // Iniciar rotação de telas
    _currentState = DISPLAY_TELEMETRY_1;
    _lastScreenChange = millis();
}

// ========================================
// ATUALIZAÇÃO DE TELEMETRIA (ROTAÇÃO)
// ========================================
void DisplayManager::updateTelemetry(const TelemetryData& data) {
    if (!_isDisplayOn) return;  // ✅ CRÍTICO: Não atualiza se display está off
    
    uint32_t currentTime = millis();
    
    // Rotacionar telas automaticamente
    if (currentTime - _lastScreenChange >= _screenInterval) {
        _lastScreenChange = currentTime;
        
        // Próxima tela
        switch (_currentState) {
            case DISPLAY_TELEMETRY_1:
                _currentState = DISPLAY_TELEMETRY_2;
                break;
            case DISPLAY_TELEMETRY_2:
                _currentState = DISPLAY_TELEMETRY_3;
                break;
            case DISPLAY_TELEMETRY_3:
                _currentState = DISPLAY_TELEMETRY_4;
                break;
            case DISPLAY_TELEMETRY_4:
                _currentState = DISPLAY_STATUS;
                break;
            case DISPLAY_STATUS:
                _currentState = DISPLAY_TELEMETRY_1;
                break;
            default:
                _currentState = DISPLAY_TELEMETRY_1;
        }
    }
    
    // Renderizar tela atual
    switch (_currentState) {
        case DISPLAY_TELEMETRY_1:
            _showTelemetry1(data);
            break;
        case DISPLAY_TELEMETRY_2:
            _showTelemetry2(data);
            break;
        case DISPLAY_TELEMETRY_3:
            _showTelemetry3(data);
            break;
        case DISPLAY_TELEMETRY_4:
            _showTelemetry4(data);
            break;
        case DISPLAY_STATUS:
            _showStatus(data);
            break;
        default:
            break;
    }
}

// ========================================
// TELA 1: TEMPERATURA + PRESSÃO + ALTITUDE
// ========================================
void DisplayManager::_showTelemetry1(const TelemetryData& data) {
    _display.clearDisplay();
    _drawHeader("TEMPERATURA");
    
    _display.setTextSize(1);
    
    // Temperatura BMP280
    _display.setCursor(0, 16);
    _display.printf("BMP: %.2f C", data.temperature);
    
    // Temperatura SI7021
    if (!isnan(data.temperatureSI)) {
        _display.setCursor(0, 26);
        _display.printf("SI7: %.2f C", data.temperatureSI);
        
        float delta = data.temperature - data.temperatureSI;
        _display.setCursor(0, 36);
        _display.printf("D: %.2f C", delta);
    }
    
    // Pressão e Altitude
    _display.setCursor(0, 48);
    _display.printf("P:%.0fhPa A:%.0fm", data.pressure, data.altitude);
    
    _display.display();
}

// ========================================
// TELA 2: IMU (GYRO + ACCEL)
// ========================================
void DisplayManager::_showTelemetry2(const TelemetryData& data) {
    _display.clearDisplay();
    _drawHeader("IMU");
    
    _display.setTextSize(1);
    
    // Giroscópio
    _display.setCursor(0, 16);
    _display.println("GYRO (rad/s):");
    _display.setCursor(0, 26);
    _display.printf("X:%.3f", data.gyroX);
    _display.setCursor(0, 36);
    _display.printf("Y:%.3f", data.gyroY);
    _display.setCursor(0, 46);
    _display.printf("Z:%.3f", data.gyroZ);
    
    _display.display();
}

// ========================================
// TELA 3: ACELERÔMETRO + MAGNETÔMETRO
// ========================================
void DisplayManager::_showTelemetry3(const TelemetryData& data) {
    _display.clearDisplay();
    _drawHeader("ACCEL + MAG");
    
    _display.setTextSize(1);
    
    // Acelerômetro
    _display.setCursor(0, 16);
    _display.println("ACCEL (m/s2):");
    _display.setCursor(0, 26);
    _display.printf("%.2f %.2f %.2f", data.accelX, data.accelY, data.accelZ);
    
    // Magnetômetro
    if (!isnan(data.magX)) {
        _display.setCursor(0, 40);
        _display.println("MAG (uT):");
        _display.setCursor(0, 50);
        _display.printf("%.1f %.1f %.1f", data.magX, data.magY, data.magZ);
    }
    
    _display.display();
}

// ========================================
// TELA 4: QUALIDADE DO AR
// ========================================
void DisplayManager::_showTelemetry4(const TelemetryData& data) {
    _display.clearDisplay();
    _drawHeader("QUALIDADE AR");
    
    _display.setTextSize(1);
    
    // Umidade
    if (!isnan(data.humidity)) {
        _display.setCursor(0, 16);
        _display.printf("Umidade: %.1f%%", data.humidity);
    }
    
    // CO2
    if (!isnan(data.co2)) {
        _display.setCursor(0, 28);
        _display.printf("CO2: %.0f ppm", data.co2);
    }
    
    // TVOC
    if (!isnan(data.tvoc)) {
        _display.setCursor(0, 40);
        _display.printf("TVOC: %.0f ppb", data.tvoc);
    }
    
    _display.display();
}

// ========================================
// TELA 5: STATUS DO SISTEMA
// ========================================
void DisplayManager::_showStatus(const TelemetryData& data) {
    _display.clearDisplay();
    _drawHeader("STATUS");
    
    _display.setTextSize(1);
    
    // Bateria
    _display.setCursor(0, 16);
    _display.printf("Bat: %.2fV (%.0f%%)", data.batteryVoltage, data.batteryPercentage);
    
    // Tempo de missão
    _display.setCursor(0, 28);
    uint32_t seconds = data.missionTime / 1000;
    uint32_t minutes = seconds / 60;
    seconds %= 60;
    _display.printf("Missao: %02lu:%02lu", minutes, seconds);
    
    // Status
    _display.setCursor(0, 40);
    if (data.systemStatus == STATUS_OK) {
        _display.println("Status: OK");
    } else {
        _display.printf("Status: ERR 0x%02X", data.systemStatus);
    }
    
    // Erros
    _display.setCursor(0, 52);
    _display.printf("Erros: %d", data.errorCount);
    
    _display.display();
}

// ========================================
// UTILITÁRIOS
// ========================================
void DisplayManager::_drawHeader(const char* title) {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println(title);
    _display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
}

void DisplayManager::_drawProgressBar(uint8_t progress, const char* label) {
    uint16_t barWidth = SCREEN_WIDTH - 20;
    uint16_t barHeight = 8;
    uint16_t barX = 10;
    uint16_t barY = 52;
    
    // Borda
    _display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
    
    // Preenchimento
    uint16_t fillWidth = (barWidth - 4) * progress / 100;
    _display.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, SSD1306_WHITE);
    
    // Porcentagem
    _display.setCursor(barX + barWidth / 2 - 12, barY - 10);
    _display.printf("%d%%", progress);
}

void DisplayManager::displayMessage(const char* title, const char* msg) {
    if (!_isDisplayOn) return;  // ✅ Proteção
    
    _display.clearDisplay();
    _drawHeader(title);
    
    _display.setTextSize(1);
    _display.setCursor(5, 25);
    _display.println(msg);
    
    _display.display();
}

void DisplayManager::nextScreen() {
    _lastScreenChange = 0;  // Força rotação imediata
}

void DisplayManager::setScreen(DisplayState state) {
    _currentState = state;
    _lastScreenChange = millis();
}
