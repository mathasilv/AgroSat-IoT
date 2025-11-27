#include "hal/spi_manager.h"
#include "config.h"

SPIManager::SPIManager() 
    : _spi(&SPI), _frequency(10000000), _dataMode(SPI_MODE_0), _bitOrder(MSBFIRST), _initialized(false) {
}

SPIManager::~SPIManager() {
    end();
}

SPIManager& SPIManager::getInstance() {
    static SPIManager instance;
    return instance;
}

bool SPIManager::begin() {
    if (_initialized) return true;
    
    _spi->begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    _spi->setFrequency(_frequency);
    _spi->setDataMode(_dataMode);
    _spi->setBitOrder(_bitOrder);
    
    pinMode(LORA_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    
    _initialized = true;
    return true;
}

void SPIManager::end() {
    if (_initialized) {
        _spi->end();
        _initialized = false;
    }
}

void SPIManager::select(uint8_t csPin) {
    digitalWrite(csPin, LOW);
}

void SPIManager::deselect(uint8_t csPin) {
    digitalWrite(csPin, HIGH);
}

bool SPIManager::transfer(uint8_t csPin, uint8_t* txData, uint8_t* rxData, size_t length) {
    select(csPin);
    size_t transferred = _spi->transferBytes(txData, rxData, length);
    deselect(csPin);
    return transferred == length;
}

bool SPIManager::transferBytes(uint8_t csPin, uint8_t* data, size_t length) {
    return transfer(csPin, data, data, length);
}

uint8_t SPIManager::transferByte(uint8_t csPin, uint8_t data) {
    uint8_t rxData;
    transfer(csPin, &data, &rxData, 1);
    return rxData;
}

bool SPIManager::writeByte(uint8_t csPin, uint8_t data) {
    uint8_t dummy;
    return transfer(csPin, &data, &dummy, 1);
}

uint8_t SPIManager::readByte(uint8_t csPin) {
    uint8_t dummy = 0xFF;
    return transferByte(csPin, dummy);
}

void SPIManager::setFrequency(uint32_t freq) {
    _frequency = freq;
    if (_initialized) {
        _spi->setFrequency(_frequency);
    }
}

void SPIManager::setDataMode(uint8_t mode) {
    _dataMode = mode;
    if (_initialized) {
        _spi->setDataMode(_dataMode);
    }
}

void SPIManager::setBitOrder(uint8_t order) {
    _bitOrder = order;
    if (_initialized) {
        _spi->setBitOrder(_bitOrder);
    }
}

bool SPIManager::isReady() {
    return _initialized;
}