#include "LoRaHal.h"
#include "config.h"  // Para LORA_FREQUENCY, etc.

LoRaHal::LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio, uint8_t cs, uint8_t rst, uint8_t dio0)
    : _spi(spi), _gpio(gpio), _cs(cs), _rst(rst), _dio0(dio0), _dio0Irq(false) {
    _gpio->pinMode(_cs, HAL::PinMode::OUTPUT);
    _gpio->digitalWrite(_cs, HAL::PinState::HIGH);
}

LoRaHal::~LoRaHal() {}

void LoRaHal::reset() {
    _gpio->digitalWrite(_rst, HAL::PinState::LOW);
    delay(10);
    _gpio->digitalWrite(_rst, HAL::PinState::HIGH);
    delay(100);
}

void LoRaHal::writeRegister(uint8_t address, uint8_t value) {
    _spi->beginTransaction(_cs);
    uint8_t buf[2] = {address | 0x80, value};
    _spi->transfer(buf, 2);
    _spi->endTransaction(_cs);
}

void LoRaHal::writeRegister(uint8_t address, const uint8_t* buffer, uint8_t size) {
    _spi->beginTransaction(_cs);
    _spi->transfer(address | 0x80);
    _spi->transferBuffer(buffer, size);
    _spi->endTransaction(_cs);
}

uint8_t LoRaHal::readRegister(uint8_t address) {
    _spi->beginTransaction(_cs);
    _spi->transfer(address & 0x7F);
    uint8_t value = _spi->transfer(0);
    _spi->endTransaction(_cs);
    return value;
}

void LoRaHal::readRegister(uint8_t address, uint8_t* buffer, uint8_t size) {
    _spi->beginTransaction(_cs);
    _spi->transfer(address & 0x7F);
    _spi->transferBuffer(buffer, size);
    _spi->endTransaction(_cs);
}

bool LoRaHal::begin(uint32_t frequency) {
    _gpio->pinMode(_rst, HAL::PinMode::OUTPUT);
    _gpio->pinMode(_dio0, HAL::PinMode::INPUT);
    reset();
    
    // Verificar chip ID
    uint8_t version = readRegister(0x42);  // RegVersion
    if (version != 0x12) {
        return false;  // Não é SX127x
    }
    
    // Config LoRa mode
    writeRegister(REG_OP_MODE, 0x80);  // Long Range mode
    delay(10);
    
    explicitHeaderMode();
    
    // Set frequency
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
    writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
    writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
    writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
    
    writeRegister(REG_PA_CONFIG, 0xFF);  // Max power
    writeRegister(REG_LR_OCP, 0x2F);     // Over current protection
    
    setSpreadingFactor(7);
    setSignalBandwidth(125E3);
    setCodingRate4(5);
    setPreambleLength(8);
    setSyncWord(0x12);
    enableCrc();
    
    writeRegister(REG_LR_MODEM_CONFIG_3, 0x04);  // Low data rate optimize
    
    receive();
    return true;
}

void LoRaHal::setTxPower(int8_t power, int outputPin) {
    if (PA_OUTPUT_RFO_PIN == outputPin) {
        // RFO
        if (power < 2) power = 2;
        else if (power > 17) power = 17;
        writeRegister(REG_PA_CONFIG, 0x70 | (power - 2));
    } else {
        // PA BOOST
        if (power > 17) power = 17;
        else if (power < -1) power = -1;
        uint8_t paConfig = 0x80;
        if (power > 14) {
            paConfig |= 0x70;
        } else if (power > 10) {
            paConfig |= (power - 10) << 4;
        } else {
            paConfig |= 0x60 | (power + 1);
        }
        writeRegister(REG_PA_CONFIG, paConfig);
    }
}

void LoRaHal::setSpreadingFactor(uint8_t sf) {
    if (sf < 6) sf = 6;
    else if (sf > 12) sf = 12;
    writeRegister(REG_LR_MODEM_CONFIG_2, (readRegister(REG_LR_MODEM_CONFIG_2) & 0x0F) | ((sf << 4) & 0xF0));
}

void LoRaHal::setSignalBandwidth(uint32_t sbw) {
    uint8_t bw;
    if (sbw <= 7.8E3) { bw = 7; }
    else if (sbw <= 10.4E3) { bw = 6; }
    else if (sbw <= 15.6E3) { bw = 5; }
    else if (sbw <= 20.8E3) { bw = 4; }
    else if (sbw <= 31.25E3) { bw = 3; }
    else if (sbw <= 41.4E3) { bw = 2; }
    else if (sbw <= 62.5E3) { bw = 1; }
    else if (sbw <= 125E3) { bw = 0; }
    else if (sbw <= 250E3) { bw = 8; }
    else { bw = 9; }
    writeRegister(REG_LR_MODEM_CONFIG_1, (readRegister(REG_LR_MODEM_CONFIG_1) & 0x0F) | (bw << 4));
}

void LoRaHal::setPreambleLength(uint16_t len) {
    writeRegister(REG_LR_PREAMBLE_MSB, (uint8_t)(len >> 8));
    writeRegister(REG_LR_PREAMBLE_LSB, (uint8_t)(len >> 0));
}

void LoRaHal::setSyncWord(uint8_t sw) {
    writeRegister(REG_LR_SYNC_WORD, sw);
}

void LoRaHal::setCodingRate4(uint8_t denominator) {
    if (denominator < 5) denominator = 5;
    else if (denominator > 8) denominator = 8;
    uint8_t cr = denominator - 4;
    writeRegister(REG_LR_MODEM_CONFIG_1, (readRegister(REG_LR_MODEM_CONFIG_1) & 0xF1) | (cr << 1));
}

void LoRaHal::enableCrc() {
    writeRegister(REG_LR_MODEM_CONFIG_2, readRegister(REG_LR_MODEM_CONFIG_2) | 0x04);
}

void LoRaHal::disableCrc() {
    writeRegister(REG_LR_MODEM_CONFIG_2, readRegister(REG_LR_MODEM_CONFIG_2) & ~0x04);
}

void LoRaHal::disableInvertIQ() {
    writeRegister(REG_LR_INVERT_IQ, readRegister(REG_LR_INVERT_IQ) & ~(1 << 0));
}

bool LoRaHal::beginPacket() {
    writeRegister(REG_LR_MODEM_CONFIG_1, readRegister(REG_LR_MODEM_CONFIG_1) & 0xFE);
    writeRegister(REG_LR_MODEM_CONFIG_2, readRegister(REG_LR_MODEM_CONFIG_2) | 0x04);
    writeRegister(REG_OP_MODE, 0x80 | 0x03);  // TX
    delay(10);
    writeRegister(REG_LR_IRQ_FLAGS, 0xFF);
    writeRegister(REG_LR_FIFO_TX_BASE_ADDR, 0);
    writeRegister(REG_LR_FIFO_RX_BYTE_ADDR, 0);
    writeRegister(REG_LR_FIFO_RX_CURRENT_ADDR, 0);
    return true;
}

int LoRaHal::endPacket(bool async) {
    writeRegister(REG_LR_IRQ_FLAGS, 0xFF);
    
    // Aguarda TX completa
    uint32_t start = millis();
    while ((readRegister(REG_LR_IRQ_FLAGS) & 0x08) == 0) {
        if (millis() - start > 3000) return 0;
        yield();
    }
    
    writeRegister(REG_LR_IRQ_FLAGS, 0x08);
    
    if (!async) {
        receive();
    }
    return 1;
}

size_t LoRaHal::write(const uint8_t* buffer, size_t size) {
    writeRegister(REG_FIFO, buffer, size);
    return size;
}

int LoRaHal::parsePacket() {
    uint8_t irq = readRegister(REG_LR_IRQ_FLAGS);
    if ((irq & 0x40) == 0) return 0;
    
    writeRegister(REG_LR_IRQ_FLAGS, irq);
    
    uint8_t currentAddr = readRegister(REG_LR_FIFO_RX_CURRENT_ADDR);
    uint8_t receivedAddr = readRegister(REG_LR_FIFO_RX_BYTE_ADDR);
    
    writeRegister(REG_LR_FIFO_ADDR_PTR, currentAddr);
    
    uint8_t payloadSize = readRegister(REG_LR_PAYLOAD_LENGTH);
    
    writeRegister(REG_LR_IRQ_FLAGS, 0xFF);
    
    receive();
    
    return payloadSize;
}

int LoRaHal::available() {
    return readRegister(REG_LR_FIFO_RX_BYTE_ADDR) - readRegister(REG_LR_FIFO_RX_CURRENT_ADDR);
}

int LoRaHal::read() {
    uint8_t addr = readRegister(REG_LR_FIFO_RX_CURRENT_ADDR);
    writeRegister(REG_LR_FIFO_ADDR_PTR, addr);
    return readRegister(REG_FIFO);
}

int LoRaHal::packetRssi() {
    return 127 - (readRegister(0x1A) + readRegister(0x1B));
}

float LoRaHal::packetSnr() {
    int8_t snr = (readRegister(0x19) & 0x7F);
    if (snr & 0x80) snr = -128 + snr;
    return snr * 0.25f;
}

int LoRaHal::rssi() {
    return 127 - readRegister(0x1B);
}

void LoRaHal::receive() {
    writeRegister(REG_LR_IRQ_FLAGS, 0xFF);
    writeRegister(REG_LR_FIFO_ADDR_PTR, 0);
    writeRegister(REG_OP_MODE, 0x80 | 0x01);  // RX continuous
}

void LoRaHal::explicitHeaderMode() {
    writeRegister(REG_LR_MODEM_CONFIG_1, readRegister(REG_LR_MODEM_CONFIG_1) | 0x01);
}
