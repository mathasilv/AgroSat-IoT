/**
 * @file SensorManager.cpp
 * @brief Implementação do gerenciador de sensores
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
    _filterIndex(0)
{
    // Inicializar buffers de filtro
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0;
        _accelYBuffer[i] = 0.0;
        _accelZBuffer[i] = 0.0;
    }
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores...");
    
    // Inicializar I2C
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    Wire.setClock(I2C_FREQUENCY);
    
    // Inicializar MPU6050
    if (_mpu.begin(MPU6050_ADDRESS, &Wire)) {
        DEBUG_PRINTLN("[SensorManager] MPU6050 OK");
        
        // Configurar MPU6050
        _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        
        _mpuOnline = true;
    } else {
        DEBUG_PRINTLN("[SensorManager] ERRO: MPU6050 não encontrado!");
        _mpuOnline = false;
    }
    
    // Inicializar BMP280
    if (_bmp.begin(BMP280_ADDRESS, BMP280_CHIPID)) {
        DEBUG_PRINTLN("[SensorManager] BMP280 OK");
        
        // Configurar BMP280 para máxima precisão
        _bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Modo operação
                        Adafruit_BMP280::SAMPLING_X16,     // Temp oversampling
                        Adafruit_BMP280::SAMPLING_X16,     // Pressure oversampling
                        Adafruit_BMP280::FILTER_X16,       // Filtering
                        Adafruit_BMP280::STANDBY_MS_500);  // Standby time
        
        _bmpOnline = true;
    } else {
        DEBUG_PRINTLN("[SensorManager] ERRO: BMP280 não encontrado!");
        _bmpOnline = false;
    }
    
    // Aguardar estabilização
    delay(100);
    
    // Realizar calibração automática
    if (_mpuOnline) {
        DEBUG_PRINTLN("[SensorManager] Calibrando MPU6050...");
        calibrateMPU6050();
    }
    
    return _mpuOnline && _bmpOnline;
}

void SensorManager::update() {
    uint32_t currentTime = millis();
    
    // Atualizar conforme intervalo configurado
    if (currentTime - _lastReadTime >= SENSOR_READ_INTERVAL) {
        _lastReadTime = currentTime;
        
        // Ler MPU6050
        if (_mpuOnline) {
            sensors_event_t accel, gyro, temp;
            _mpu.getEvent(&accel, &gyro, &temp);
            
            // Aplicar offsets de calibração e converter unidades
            _gyroX = gyro.gyro.x - _gyroOffsetX;  // rad/s
            _gyroY = gyro.gyro.y - _gyroOffsetY;
            _gyroZ = gyro.gyro.z - _gyroOffsetZ;
            
            // Aplicar filtro e offsets ao acelerômetro
            float rawAccelX = accel.acceleration.x - _accelOffsetX;  // m/s²
            float rawAccelY = accel.acceleration.y - _accelOffsetY;
            float rawAccelZ = accel.acceleration.z - _accelOffsetZ;
            
            _accelX = _applyFilter(rawAccelX, _accelXBuffer);
            _accelY = _applyFilter(rawAccelY, _accelYBuffer);
            _accelZ = _applyFilter(rawAccelZ, _accelZBuffer);
        }
        
        // Ler BMP280
        if (_bmpOnline) {
            _temperature = _bmp.readTemperature();  // °C
            _pressure = _bmp.readPressure() / 100.0;  // Pa -> hPa
            _altitude = _calculateAltitude(_pressure);
        }
    }
}

bool SensorManager::calibrateMPU6050() {
    if (!_mpuOnline) return false;
    
    DEBUG_PRINTLN("[SensorManager] Iniciando calibração (mantenha imóvel)...");
    
    float sumGyroX = 0, sumGyroY = 0, sumGyroZ = 0;
    float sumAccelX = 0, sumAccelY = 0, sumAccelZ = 0;
    
    // Coletar amostras
    for (int i = 0; i < MPU6050_CALIBRATION_SAMPLES; i++) {
        sensors_event_t accel, gyro, temp;
        _mpu.getEvent(&accel, &gyro, &temp);
        
        sumGyroX += gyro.gyro.x;
        sumGyroY += gyro.gyro.y;
        sumGyroZ += gyro.gyro.z;
        
        sumAccelX += accel.acceleration.x;
        sumAccelY += accel.acceleration.y;
        sumAccelZ += accel.acceleration.z;
        
        delay(10);
    }
    
    // Calcular offsets
    _gyroOffsetX = sumGyroX / MPU6050_CALIBRATION_SAMPLES;
    _gyroOffsetY = sumGyroY / MPU6050_CALIBRATION_SAMPLES;
    _gyroOffsetZ = sumGyroZ / MPU6050_CALIBRATION_SAMPLES;
    
    _accelOffsetX = sumAccelX / MPU6050_CALIBRATION_SAMPLES;
    _accelOffsetY = sumAccelY / MPU6050_CALIBRATION_SAMPLES;
    _accelOffsetZ = (sumAccelZ / MPU6050_CALIBRATION_SAMPLES) - 9.81;  // Subtrair gravidade
    
    _calibrated = true;
    
    DEBUG_PRINTLN("[SensorManager] Calibração concluída!");
    DEBUG_PRINTF("  Gyro offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                 _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ);
    DEBUG_PRINTF("  Accel offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                 _accelOffsetX, _accelOffsetY, _accelOffsetZ);
    
    return true;
}

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

void SensorManager::resetMPU6050() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando MPU6050...");
    _mpuOnline = _mpu.begin(MPU6050_ADDRESS, &Wire);
    if (_mpuOnline) {
        _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
}

void SensorManager::resetBMP280() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando BMP280...");
    _bmpOnline = _bmp.begin(BMP280_ADDRESS, BMP280_CHIPID);
}

void SensorManager::getRawData(sensors_event_t& accel, sensors_event_t& gyro, sensors_event_t& temp) {
    if (_mpuOnline) {
        _mpu.getEvent(&accel, &gyro, &temp);
    }
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

float SensorManager::_applyFilter(float newValue, float* buffer) {
    // Atualizar buffer circular
    buffer[_filterIndex] = newValue;
    _filterIndex = (_filterIndex + 1) % FILTER_SIZE;
    
    // Calcular média
    float sum = 0.0;
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += buffer[i];
    }
    
    return sum / FILTER_SIZE;
}

float SensorManager::_calculateAltitude(float pressure) {
    // Fórmula barométrica internacional
    // h = 44330 * (1 - (P/P0)^(1/5.255))
    // onde P0 é a pressão ao nível do mar (1013.25 hPa)
    
    float ratio = pressure / _seaLevelPressure;
    float altitude = 44330.0 * (1.0 - pow(ratio, 0.1903));
    
    return altitude;
}
