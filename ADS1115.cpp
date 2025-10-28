#include "ADS1115.h"

// 构造函数
ADS1115::ADS1115(uint8_t address, I2CMux* mux, uint8_t channel) 
    : _address(address), _mux(mux), _channel(channel), _currentConfig(ADS1115_DEFAULT_CONFIG) {
    _i2cPort = nullptr;
}

// 初始化
bool ADS1115::begin(TwoWire &wirePort) {
    _i2cPort = &wirePort;
    
    // 检查连接
    if (!isConnected()) {
        Serial.print("ADS1115: 无法连接到地址 0x");
        Serial.println(_address, HEX);
        return false;
    }
    
    // 配置默认值
    configure();
    
    Serial.print("ADS1115: 初始化成功，地址 0x");
    Serial.println(_address, HEX);
    
    return true;
}

// 检查连接
bool ADS1115::isConnected() {
    if (!selectChannel()) {
        return false;
    }
    
    _i2cPort->beginTransmission(_address);
    uint8_t error = _i2cPort->endTransmission();
    return (error == 0);
}

// 选择多路复用器通道
bool ADS1115::selectChannel() {
    if (_mux == nullptr) {
        return true; // 没有多路复用器，直接返回成功
    }
    
    return _mux->selectChannel(_channel);
}

// 设置多路复用器通道
void ADS1115::setMuxChannel(I2CMux* mux, uint8_t channel) {
    _mux = mux;
    _channel = channel;
}

// 写入寄存器
bool ADS1115::writeRegister(uint8_t reg, uint16_t value) {
    if (!selectChannel()) {
        return false;
    }
    
    _i2cPort->beginTransmission(_address);
    _i2cPort->write(reg);                    // 寄存器地址
    _i2cPort->write((uint8_t)(value >> 8));   // 高字节
    _i2cPort->write((uint8_t)(value & 0xFF)); // 低字节
    uint8_t error = _i2cPort->endTransmission();
    
    return (error == 0);
}

// 读取寄存器
uint16_t ADS1115::readRegister(uint8_t reg) {
    if (!selectChannel()) {
        return 0;
    }
    
    _i2cPort->beginTransmission(_address);
    _i2cPort->write(reg);
    if (_i2cPort->endTransmission() != 0) {
        return 0;
    }
    
    uint8_t bytesRead = _i2cPort->requestFrom(_address, (uint8_t)2);
    if (bytesRead != 2) {
        return 0;
    }
    
    uint8_t highByte = _i2cPort->read();
    uint8_t lowByte = _i2cPort->read();
    
    return ((uint16_t)highByte << 8) | lowByte;
}

// 写入配置寄存器
void ADS1115::writeConfig(uint16_t config) {
    _currentConfig = config;
    writeRegister(ADS1115_REG_CONFIG, config);
}

// 配置ADS1115
bool ADS1115::configure(uint16_t config) {
    _currentConfig = config;
    return writeRegister(ADS1115_REG_CONFIG, config);
}

// 获取当前配置
uint16_t ADS1115::getConfig() {
    return readRegister(ADS1115_REG_CONFIG);
}

// 等待转换完成
bool ADS1115::waitForConversion(unsigned long timeout) {
    unsigned long startTime = millis();
    
    while (millis() - startTime < timeout) {
        uint16_t config = readRegister(ADS1115_REG_CONFIG);
        if (config & ADS1115_OS_BUSY) {
            // 转换仍在进行中
            delay(10);
        } else {
            // 转换完成
            return true;
        }
    }
    
    return false; // 超时
}

// 读取原始ADC值
int16_t ADS1115::readRaw(uint8_t mux) {
    if (!selectChannel()) {
        return 0;
    }
    
    // 启动单次转换
    uint16_t config = _currentConfig;
    config &= ~0x7000; // 清除MUX位
    config |= (mux & 0x7000); // 设置新的MUX值
    config |= ADS1115_OS_BUSY; // 启动转换
    
    if (!writeRegister(ADS1115_REG_CONFIG, config)) {
        return 0;
    }
    
    // 等待转换完成
    delay(10); // 最短延迟（128SPS模式）
    
    // 读取转换结果
    uint16_t result = readRegister(ADS1115_REG_CONVERSION);
    
    return (int16_t)result;
}

// 读取电压值
float ADS1115::readVoltage(uint8_t mux) {
    int16_t raw = readRaw(mux);
    
    if (raw == 0 && _currentConfig == 0) {
        return 0.0; // 未初始化
    }
    
    // 获取当前PGA设置
    uint16_t pga = (_currentConfig >> 9) & 0x0007;
    float fsr;
    
    switch (pga) {
        case 0: fsr = 6.144; break;
        case 1: fsr = 4.096; break;
        case 2: fsr = 2.048; break;
        case 3: fsr = 1.024; break;
        case 4: fsr = 0.512; break;
        case 5: fsr = 0.256; break;
        default: fsr = 2.048; break;
    }
    
    // 16位ADC，LSB = FSR / 32768
    float voltage = (raw * fsr) / 32768.0;
    
    return voltage;
}

// 设置增益
void ADS1115::setGain(uint8_t gain) {
    uint16_t pgaValue = (gain << 9) & 0x0E00;
    _currentConfig &= ~0x0E00;
    _currentConfig |= pgaValue;
    writeConfig(_currentConfig);
}

// 设置数据速率
void ADS1115::setDataRate(uint8_t dr) {
    uint16_t drValue = (dr << 5) & 0x00E0;
    _currentConfig &= ~0x00E0;
    _currentConfig |= drValue;
    writeConfig(_currentConfig);
}

// 设置模式
void ADS1115::setMode(uint8_t mode) {
    if (mode == ADS1115_MODE_CONTINUOUS) {
        _currentConfig &= ~ADS1115_MODE_SINGLE;
    } else {
        _currentConfig |= ADS1115_MODE_SINGLE;
    }
    writeConfig(_currentConfig);
}

// 扫描地址
void ADS1115::scanAddress() {
    if (!_i2cPort) return;
    
    Serial.println("扫描ADS1115地址...");
    for (uint8_t addr = 0x48; addr <= 0x4B; addr++) {
        _i2cPort->beginTransmission(addr);
        uint8_t error = _i2cPort->endTransmission();
        
        if (error == 0) {
            Serial.print("找到ADS1115，地址: 0x");
            Serial.println(addr, HEX);
        }
    }
}

