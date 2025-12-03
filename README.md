# AgroSat-IoT

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

AgroSat-IoT is an advanced IoT platform designed for precision agriculture applications using ESP32-based hardware with LoRa communication capabilities. The project implements a modular firmware architecture optimized for low-power operation, sensor data acquisition, and satellite IoT integration for remote agricultural monitoring.

## Features

- **Dual Communication**: LoRaWAN and satellite IoT connectivity
- **Low-Power Design**: Optimized for long-term field deployment
- **Modular Architecture**: ESP32 with Arduino framework and ESP-IDF integration
- **Sensor Integration**: Soil moisture, temperature, humidity, and environmental sensors
- **Flight-Ready**: Designed for agricultural satellite missions
- **Mesh Networking**: LoRa mesh capabilities for extended coverage

## Hardware Requirements

- **Primary Board**: ESP32-S3 or ESP32-C3 variant
- **LoRa Module**: SX1262/SX1276 compatible
- **Sensors**: 
  - Soil moisture sensor (capacitive)
  - Temperature/humidity (DHT22/SHT3x)
  - Light intensity (TSL2561/BH1750)
  - Optional: GPS module (NEO-6M/7M)
- **Power**: Solar panel + LiPo battery with charging circuit

## Software Architecture

```
AgroSat-IoT/
├── src/
│   ├── core/           # Core system modules
│   │   ├── sensors/
│   │   ├── comm/
│   │   ├── power/
│   │   └── state_machine/
│   ├── lora/           # LoRaWAN implementation
│   ├── satellite/      # Satellite comm protocol
│   └── main.cpp
├── lib/
│   └── deps/           # External dependencies
├── test/
└── platformio.ini
```

## Quick Start

### Prerequisites
- [PlatformIO](https://platformio.org/) extension for VSCode
- ESP32 board support
- Serial monitor (PlatformIO Serial or Arduino IDE)

### Development Setup
```bash
# Clone repository
git clone https://github.com/mathasilv/AgroSat-IoT.git
cd AgroSat-IoT

# Install dependencies
pio lib install "LoRa","ArduinoJson","LittleFS"

# Build and upload
pio run -e esp32-s3-devkitc-1 -t upload
```

### Configuration
1. Update `src/config.h` with your LoRaWAN credentials
2. Configure sensor pins in `src/sensors/sensor_config.h`
3. Set satellite communication parameters in `src/satellite/config.h`

## Build Targets

| Environment | Board | Description |
|-------------|-------|-------------|
| `esp32-s3-devkitc-1` | ESP32-S3 | Development board |
| `lilygo-lora32` | TTGO LoRa32 | LoRa field deployment |
| `esp32-c3-devkit` | ESP32-C3 | Low-power variant |

## Power Optimization

The firmware implements aggressive power saving:
- Deep sleep cycles with 8s wake intervals
- Sensor sampling optimization
- Dynamic transmission scheduling
- Brown-out detection and recovery

**Expected Battery Life**: 6-12 months with solar charging

## API Documentation

### Sensor Data Structure
```cpp
struct SensorData {
    float soil_moisture;
    float temperature;
    float humidity;
    uint16_t light;
    float battery_voltage;
    uint32_t timestamp;
};
```

## Testing

```bash
# Unit tests
pio test

# Hardware-in-loop testing
pio run -e test-suite
```

## Deployment

1. Flash firmware to target board
2. Configure via serial terminal
3. Deploy with solar panel connected
4. Monitor via LoRaWAN gateway or satellite ground station

## Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/sensor-calibration`)
3. Commit changes (`git commit -am 'Add sensor calibration'`)
4. Push to branch (`git push origin feature/sensor-calibration`)
5. Create Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- LilyGO for ESP32 LoRa boards
- PlatformIO for embedded development workflow
- OBSAT/MCTI for agricultural satellite initiative support

---

*Optimized for agricultural IoT satellite missions*