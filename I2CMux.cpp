#include "I2CMux.h"

I2CMux::I2CMux(uint8_t address) 
    : _address(address), _activeChannel(255), _channelCount(0) {
    
    // 初始化通道配置
    for (int i = 0; i < MAX_MUX_CHANNELS; i++) {
        _channels[i] = {i, 0, "Unused", false};
    }
}

void I2CMux::begin() {
    // 禁用所有通道开始
    disableAllChannels();
    Serial.println("I2C多路复用器初始化完成");
}

void I2CMux::setAddress(uint8_t address) {
    _address = address;
}

void I2CMux::addChannel(uint8_t channel, uint8_t sensorAddr, const char* sensorName) {
    if (channel < MAX_MUX_CHANNELS) {
        _channels[channel] = {channel, sensorAddr, sensorName, true};
        if (channel >= _channelCount) {
            _channelCount = channel + 1;
        }
        Serial.print("添加多路复用器通道: ");
        Serial.print(channel);
        Serial.print(", 传感器地址: 0x");
        Serial.print(sensorAddr, HEX);
        Serial.print(", 名称: ");
        Serial.println(sensorName);
    }
}

void I2CMux::enableChannel(uint8_t channel, bool enable) {
    if (channel < MAX_MUX_CHANNELS) {
        _channels[channel].enabled = enable;
        Serial.print("通道 ");
        Serial.print(channel);
        Serial.println(enable ? " 已启用" : " 已禁用");
    }
}

bool I2CMux::selectChannel(uint8_t channel) {
    if (channel < MAX_MUX_CHANNELS && _channels[channel].enabled) {
        // 如果已经是目标通道，直接返回成功
        if (_activeChannel == channel) {
            return true;
        }
        
        // 先禁用所有通道，确保干净的状态
        Wire.beginTransmission(_address);
        Wire.write(0);
        Wire.endTransmission();
        delay(10); // 减少延迟时间，提高切换速度
        
        // 选择目标通道
        Wire.beginTransmission(_address);
        Wire.write(1 << channel); // 选择对应通道
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            _activeChannel = channel;
            delay(20); // 减少延迟时间，提高切换速度
            return true;
        } else {
            Serial.print("选择多路复用器通道失败，错误代码: ");
            Serial.println(error);
            return false;
        }
    }
    return false;
}

void I2CMux::disableAllChannels() {
    Wire.beginTransmission(_address);
    Wire.write(0); // 禁用所有通道
    Wire.endTransmission();
    _activeChannel = 255; // 表示无活动通道
}

MuxChannelConfig I2CMux::getChannelConfig(uint8_t channel) const {
    if (channel < MAX_MUX_CHANNELS) {
        return _channels[channel];
    }
    return {0, 0, "Invalid", false};
}

bool I2CMux::isChannelEnabled(uint8_t channel) const {
    if (channel < MAX_MUX_CHANNELS) {
        return _channels[channel].enabled;
    }
    return false;
}

void I2CMux::printChannelInfo() {
    Serial.println("=== I2C多路复用器通道信息 ===");
    for (int i = 0; i < _channelCount; i++) {
        Serial.print("通道 ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(_channels[i].sensorName);
        Serial.print(" (0x");
        Serial.print(_channels[i].sensorAddr, HEX);
        Serial.print(") - ");
        Serial.println(_channels[i].enabled ? "启用" : "禁用");
    }
    Serial.println("============================");
}

void I2CMux::resetI2CBus() {
    // 禁用所有通道
    disableAllChannels();
    delay(10);
    
    // 重新初始化I2C总线
    Wire.end();
    delay(10);
    Wire.begin();
    Wire.setClock(400000);
    
    Serial.println("I2C总线已重置");
}

void I2CMux::scanI2CDevices() {
    Serial.println("=== I2C设备扫描 ===");
    
    // 扫描多路复用器本身
    Wire.beginTransmission(_address);
    uint8_t error = Wire.endTransmission();
    Serial.print("多路复用器 (0x");
    Serial.print(_address, HEX);
    Serial.print("): ");
    if (error == 0) {
        Serial.println("找到");
    } else {
        Serial.print("未找到，错误代码: ");
        Serial.println(error);
    }
    
    // 扫描每个通道上的设备
    for (uint8_t channel = 0; channel < MAX_MUX_CHANNELS; channel++) {
        if (_channels[channel].enabled) {
            Serial.print("通道 ");
            Serial.print(channel);
            Serial.print(" (");
            Serial.print(_channels[channel].sensorName);
            Serial.print("): ");
            
            // 选择通道
            if (selectChannel(channel)) {
                // 扫描该通道上的设备
                Wire.beginTransmission(_channels[channel].sensorAddr);
                error = Wire.endTransmission();
                if (error == 0) {
                    Serial.print("设备 0x");
                    Serial.print(_channels[channel].sensorAddr, HEX);
                    Serial.println(" 找到");
                } else {
                    Serial.print("设备 0x");
                    Serial.print(_channels[channel].sensorAddr, HEX);
                    Serial.print(" 未找到，错误代码: ");
                    Serial.println(error);
                }
            } else {
                Serial.println("通道选择失败");
            }
        }
    }
    
    Serial.println("=== 扫描完成 ===");
}

void I2CMux::lockOLEDChannel() {
    // 选择OLED通道并保持锁定状态
    selectChannel(2); // OLED现在在通道2
    Serial.println("OLED通道已锁定");
}

void I2CMux::unlockOLEDChannel() {
    // 禁用所有通道，释放锁定
    disableAllChannels();
    Serial.println("OLED通道已解锁");
}