/**
 * @file SensorManager.cpp
 * @brief Implementação do gerenciador de sensores - Versão robusta
 */

#include "SensorManager.h"

SensorManager::SensorManager() :
    _temperature(0.0),
    _pressure(0.0),
    _altitude(0.0),
    _seaLevelPressure(1013.25),  // hPa (padrão ao nível do mar)
    _gyroX(0.0), _gyroY(0.0), _gyroZ(0.0),
    _accelX(0.0), _accelY(0.0), _accelZ(0.0),
    _gyroOffsetX(0.0), _gyroOffsetY(0.0), _gyroOffsetZ(0.0),
    _accelOffsetX(0.0), _accelOffsetY(0.0), _accelOffsetZ(0.0),
    _mpuOnline(false),
    _bmpOnline(false),
    _calibrated(false),
    _lastReadTime(0),
    _filterIndex(0),
    _initRetries(0),
    _consecutiveFailures(0),
    _lastHealthCheck(0)
{
    // Inicializar buffers de filtro
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0;
        _accelYBuffer[i] = 0.0;
        _accelZBuffer[i] = 0.0;
    }
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores...");
    
    // Inicializar I2C com guardas de estabilização
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    Wire.setClock(I2C_FREQUENCY);
    
    // Delay crítico para estabilização após Wire.begin
    delay(200);
    
    // Testar barramento I2C antes de inicializar sensores
    if (!_testI2CBus()) {
        DEBUG_PRINTLN("[SensorManager] ERRO: Barramento I2C não responde!");
        return false;
    }
    
    bool success = true;
    _initRetries = 0;
    
    // Inicializar MPU6050 com retry
    _mpuOnline = false;
    for (uint8_t retry = 0; retry < 3 && !_mpuOnline; retry++) {
        if (retry > 0) {
            DEBUG_PRINTF("[SensorManager] Retry MPU6050 %d/3...\n", retry + 1);
            delay(100);
            _initRetries++;
        }
        
        if (_mpu.begin(MPU6050_ADDRESS, &Wire)) {
            DEBUG_PRINTLN("[SensorManager] MPU6050 OK");
            
            // Configurar MPU6050 com verificação
            _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
            _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
            _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
            
            // Verificar se configuração foi aceita
            delay(50);
            sensors_event_t test_accel, test_gyro, test_temp;
            if (_mpu.getEvent(&test_accel, &test_gyro, &test_temp)) {
                _mpuOnline = true;
            } else {
                DEBUG_PRINTLN("[SensorManager] AVISO: MPU6050 conectado mas não responde");
            }
        }
    }
    
    if (!_mpuOnline) {
        DEBUG_PRINTLN("[SensorManager] ERRO: MPU6050 falhou após retries!");
        success = false;
    }
    
    // Inicializar BMP280 com retry e endereços alternativos
    _bmpOnline = false;
    uint8_t bmp_addresses[] = {0x76, 0x77}; // Endereços comuns do BMP280
    
    for (uint8_t addr_idx = 0; addr_idx < 2 && !_bmpOnline; addr_idx++) {
        for (uint8_t retry = 0; retry < 2 && !_bmpOnline; retry++) {
            if (retry > 0 || addr_idx > 0) {
                DEBUG_PRINTF("[SensorManager] Tentando BMP280 addr 0x%02X, retry %d...\n", 
                            bmp_addresses[addr_idx], retry + 1);
                delay(100);
                _initRetries++;
            }
            
            unsigned status = _bmp.begin(bmp_addresses[addr_idx]);
            if (status) {
                DEBUG_PRINTF("[SensorManager] BMP280 OK no endereço 0x%02X\n", bmp_addresses[addr_idx]);
                
                // Configurar BMP280 para máxima precisão
                _bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                                Adafruit_BMP280::SAMPLING_X16,     
                                Adafruit_BMP280::SAMPLING_X16,     
                                Adafruit_BMP280::FILTER_X16,       
                                Adafruit_BMP280::STANDBY_MS_500);
                
                // Verificar primeira leitura
                delay(100);
                float test_temp = _bmp.readTemperature();
                if (!isnan(test_temp) && test_temp > -50 && test_temp < 100) {
                    _bmpOnline = true;
                } else {
                    DEBUG_PRINTLN("[SensorManager] AVISO: BMP280 conectado mas leituras inválidas");
                }
            }
        }
    }
    
    if (!_bmpOnline) {
        DEBUG_PRINTLN("[SensorManager] ERRO: BMP280 falhou em todos os endereços!");
        success = false;
    }
    
    // Aguardar estabilização final
    delay(100);
    
    // Realizar calibração automática do MPU6050
    if (_mpuOnline) {
        DEBUG_PRINTLN("[SensorManager] Calibrando MPU6050...");
        if (calibrateMPU6050()) {
            DEBUG_PRINTLN("[SensorManager] Calibração do MPU6050 concluída");
        } else {
            DEBUG_PRINTLN("[SensorManager] AVISO: Falha na calibração do MPU6050");
        }
    }
    
    // Log de status final
    DEBUG_PRINTF("[SensorManager] Init completo - MPU: %s, BMP: %s, Retries: %d\n",
                _mpuOnline ? "OK" : "FALHA",
                _bmpOnline ? "OK" : "FALHA", 
                _initRetries);
    
    return success;
}

void SensorManager::update() {
    uint32_t currentTime = millis();
    
    // Verificação de saúde dos sensores a cada 30 segundos
    if (currentTime - _lastHealthCheck >= 30000) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }
    
    // Atualizar conforme intervalo configurado
    if (currentTime - _lastReadTime >= SENSOR_READ_INTERVAL) {
        _lastReadTime = currentTime;
        
        bool readSuccess = true;
        
        // Ler MPU6050 com proteção contra falhas
        if (_mpuOnline) {
            sensors_event_t accel, gyro, temp;
            if (_mpu.getEvent(&accel, &gyro, &temp)) {
                // Validar leituras (detectar NaN ou valores absurdos)
                if (_validateMPUReadings(accel, gyro)) {
                    // Aplicar offsets de calibração
                    _gyroX = gyro.gyro.x - _gyroOffsetX;
                    _gyroY = gyro.gyro.y - _gyroOffsetY;
                    _gyroZ = gyro.gyro.z - _gyroOffsetZ;
                    
                    // Aplicar filtro ao acelerômetro
                    float rawAccelX = accel.acceleration.x - _accelOffsetX;
                    float rawAccelY = accel.acceleration.y - _accelOffsetY;
                    float rawAccelZ = accel.acceleration.z - _accelOffsetZ;
                    
                    _accelX = _applyFilter(rawAccelX, _accelXBuffer);
                    _accelY = _applyFilter(rawAccelY, _accelYBuffer);
                    _accelZ = _applyFilter(rawAccelZ, _accelZBuffer);
                    
                    _consecutiveFailures = 0; // Reset contador de falhas
                } else {
                    DEBUG_PRINTLN("[SensorManager] AVISO: Leituras inválidas do MPU6050");
                    _consecutiveFailures++;
                    readSuccess = false;
                }
            } else {
                DEBUG_PRINTLN("[SensorManager] ERRO: Falha na comunicação com MPU6050");
                _consecutiveFailures++;
                readSuccess = false;
            }
        }
        
        // Ler BMP280 com proteção contra falhas
        if (_bmpOnline) {
            float temp = _bmp.readTemperature();
            float press = _bmp.readPressure();
            
            // Validar leituras
            if (_validateBMPReadings(temp, press)) {
                _temperature = temp;
                _pressure = press / 100.0;  // Pa -> hPa
                _altitude = _calculateAltitude(_pressure);
                
                if (_consecutiveFailures > 0 && readSuccess) {
                    _consecutiveFailures--; // Reduzir contador gradualmente
                }
            } else {
                DEBUG_PRINTLN("[SensorManager] AVISO: Leituras inválidas do BMP280");
                _consecutiveFailures++;
                readSuccess = false;
            }
        }
        
        // Detectar falhas consecutivas críticas
        if (_consecutiveFailures >= 10) {
            DEBUG_PRINTLN("[SensorManager] CRÍTICO: Muitas falhas consecutivas, tentando reset");
            _attemptSensorRecovery();
            _consecutiveFailures = 5; // Reset parcial
        }
    }
}

bool SensorManager::calibrateMPU6050() {
    if (!_mpuOnline) return false;
    
    DEBUG_PRINTLN("[SensorManager] Iniciando calibração (mantenha imóvel)...");
    
    float sumGyroX = 0, sumGyroY = 0, sumGyroZ = 0;
    float sumAccelX = 0, sumAccelY = 0, sumAccelZ = 0;
    uint16_t validSamples = 0;
    
    // Coletar amostras com validação
    for (int i = 0; i < MPU6050_CALIBRATION_SAMPLES; i++) {
        sensors_event_t accel, gyro, temp;
        
        if (_mpu.getEvent(&accel, &gyro, &temp) && 
            _validateMPUReadings(accel, gyro)) {
            
            sumGyroX += gyro.gyro.x;
            sumGyroY += gyro.gyro.y;
            sumGyroZ += gyro.gyro.z;
            
            sumAccelX += accel.acceleration.x;
            sumAccelY += accel.acceleration.y;
            sumAccelZ += accel.acceleration.z;
            
            validSamples++;
        } else {
            DEBUG_PRINT("x"); // Marcar amostra inválida
        }
        
        delay(10);
        
        // Feedback visual a cada 10 amostras
        if (i % 10 == 0) {
            DEBUG_PRINTF(".");
        }
    }
    DEBUG_PRINTLN("");
    
    // Verificar se temos amostras suficientes
    if (validSamples < (MPU6050_CALIBRATION_SAMPLES * 0.8)) {
        DEBUG_PRINTF("[SensorManager] ERRO: Poucas amostras válidas (%d/%d)\n", 
                    validSamples, MPU6050_CALIBRATION_SAMPLES);
        return false;
    }
    
    // Calcular offsets
    _gyroOffsetX = sumGyroX / validSamples;
    _gyroOffsetY = sumGyroY / validSamples;
    _gyroOffsetZ = sumGyroZ / validSamples;
    
    _accelOffsetX = sumAccelX / validSamples;
    _accelOffsetY = sumAccelY / validSamples;
    _accelOffsetZ = (sumAccelZ / validSamples) - 9.81;  // Subtrair gravidade
    
    _calibrated = true;
    
    DEBUG_PRINTLN("[SensorManager] Calibração concluída!");
    DEBUG_PRINTF("  Amostras válidas: %d/%d\n", validSamples, MPU6050_CALIBRATION_SAMPLES);
    DEBUG_PRINTF("  Gyro offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                 _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ);
    DEBUG_PRINTF("  Accel offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                 _accelOffsetX, _accelOffsetY, _accelOffsetZ);
    
    return true;
}

// Getters permanecem inalterados
float SensorManager::getTemperature() { return _temperature; }
float SensorManager::getPressure() { return _pressure; }
float SensorManager::getAltitude() { return _altitude; }
float SensorManager::getGyroX() { return _gyroX; }
float SensorManager::getGyroY() { return _gyroY; }
float SensorManager::getGyroZ() { return _gyroZ; }
float SensorManager::getAccelX() { return _accelX; }
float SensorManager::getAccelY() { return _accelY; }
float SensorManager::getAccelZ() { return _accelZ; }

float SensorManager::getAccelMagnitude() {
    return sqrt(_accelX * _accelX + _accelY * _accelY + _accelZ * _accelZ);
}

bool SensorManager::isMPU6050Online() { return _mpuOnline; }
bool SensorManager::isBMP280Online() { return _bmpOnline; }
bool SensorManager::isCalibrated() { return _calibrated; }

void SensorManager::resetMPU6050() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando MPU6050...");
    _mpuOnline = _mpu.begin(MPU6050_ADDRESS, &Wire);
    if (_mpuOnline) {
        _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        DEBUG_PRINTLN("[SensorManager] MPU6050 reiniciado com sucesso");
    } else {
        DEBUG_PRINTLN("[SensorManager] Falha ao reiniciar MPU6050");
    }
}

void SensorManager::resetBMP280() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando BMP280...");
    unsigned status = _bmp.begin();
    _bmpOnline = (status != 0);
    
    if (_bmpOnline) {
        _bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                        Adafruit_BMP280::SAMPLING_X16,     
                        Adafruit_BMP280::SAMPLING_X16,     
                        Adafruit_BMP280::FILTER_X16,       
                        Adafruit_BMP280::STANDBY_MS_500);
        DEBUG_PRINTLN("[SensorManager] BMP280 reiniciado com sucesso");
    } else {
        DEBUG_PRINTLN("[SensorManager] Falha ao reiniciar BMP280");
    }
}

void SensorManager::getRawData(sensors_event_t& accel, sensors_event_t& gyro, sensors_event_t& temp) {
    if (_mpuOnline) {
        _mpu.getEvent(&accel, &gyro, &temp);
    }
}

// ============================================================================
// MÉTODOS PRIVADOS - ROBUSTEZ E VALIDAÇÃO
// ============================================================================

bool SensorManager::_testI2CBus() {
    // Tentar escaneamento básico do barramento I2C
    Wire.beginTransmission(0x00);
    uint8_t error = Wire.endTransmission();
    
    // Se não há erro ou erro de NACK (normal quando nenhum dispositivo responde em 0x00)
    return (error == 0 || error == 2);
}

bool SensorManager::_validateMPUReadings(const sensors_event_t& accel, const sensors_event_t& gyro) {
    // Verificar NaN
    if (isnan(accel.acceleration.x) || isnan(accel.acceleration.y) || isnan(accel.acceleration.z) ||
        isnan(gyro.gyro.x) || isnan(gyro.gyro.y) || isnan(gyro.gyro.z)) {
        return false;
    }
    
    // Verificar ranges razoáveis
    // Aceleração: -80 a +80 m/s² (muito além de ±8G)
    if (abs(accel.acceleration.x) > 80 || abs(accel.acceleration.y) > 80 || abs(accel.acceleration.z) > 80) {
        return false;
    }
    
    // Giroscópio: -35 a +35 rad/s (muito além de ±500°/s)
    if (abs(gyro.gyro.x) > 35 || abs(gyro.gyro.y) > 35 || abs(gyro.gyro.z) > 35) {
        return false;
    }
    
    return true;
}

bool SensorManager::_validateBMPReadings(float temperature, float pressure) {
    // Verificar NaN
    if (isnan(temperature) || isnan(pressure)) {
        return false;
    }
    
    // Verificar ranges razoáveis
    // Temperatura: -80 a +85°C (conforme especificações do projeto)
    if (temperature < -80 || temperature > 85) {
        return false;
    }
    
    // Pressão: 300 a 1100 hPa (range típico atmosférico + margem)
    float pressureHPa = pressure / 100.0;
    if (pressureHPa < 300 || pressureHPa > 1100) {
        return false;
    }
    
    return true;
}

void SensorManager::_performHealthCheck() {
    DEBUG_PRINTF("[SensorManager] Health Check - MPU: %s, BMP: %s, Failures: %d\n",
                _mpuOnline ? "OK" : "FAIL",
                _bmpOnline ? "OK" : "FAIL",
                _consecutiveFailures);
                
    // Log de heap para monitoramento de memória
    DEBUG_PRINTF("[SensorManager] Free heap: %lu bytes\n", ESP.getFreeHeap());
}

void SensorManager::_attemptSensorRecovery() {
    DEBUG_PRINTLN("[SensorManager] Tentando recuperação dos sensores...");
    
    if (_mpuOnline) {
        resetMPU6050();
    }
    
    if (_bmpOnline) {
        resetBMP280();
    }
    
    // Pequeno delay para estabilização
    delay(200);
}

float SensorManager::_applyFilter(float newValue, float* buffer) {
    // Atualizar buffer circular
    buffer[_filterIndex] = newValue;
    _filterIndex = (_filterIndex + 1) % CUSTOM_FILTER_SIZE;
    
    // Calcular média móvel
    float sum = 0.0;
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        sum += buffer[i];
    }
    
    return sum / CUSTOM_FILTER_SIZE;
}

float SensorManager::_calculateAltitude(float pressure) {
    if (pressure <= 0.0) return 0.0; // Evitar divisão por zero
    
    float ratio = pressure / _seaLevelPressure;
    float altitude = 44330.0 * (1.0 - pow(ratio, 0.1903));
    
    return altitude;
}
