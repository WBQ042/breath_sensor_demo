#ifndef I2CMux_h
#define I2CMux_h

#include <Arduino.h>
#include <Wire.h>

// I2C 多路复用器配置
constexpr uint8_t TCA9548_BASE_ADDR = 0x70;     // TCA9548 基础地址 (A0,A1,A2接地)
constexpr uint8_t MAX_MUX_CHANNELS = 8;         // TCA9548 最大通道数

// I2C 多路复用器通道配置结构体
struct MuxChannelConfig {
    uint8_t channel;        // 通道号 (0-7)
    uint8_t sensorAddr;     // 该通道上的传感器地址
    const char* sensorName; // 传感器名称（用于调试）
    bool enabled;           // 是否启用该通道
};

class I2CMux {
public:
    I2CMux(uint8_t address = TCA9548_BASE_ADDR);
    
    void begin();
    void setAddress(uint8_t address);
    
    // 通道管理
    void addChannel(uint8_t channel, uint8_t sensorAddr, const char* sensorName = "Unknown");
    void enableChannel(uint8_t channel, bool enable);
    bool selectChannel(uint8_t channel);
    void disableAllChannels();
    
    // 获取信息
    uint8_t getActiveChannel() const { return _activeChannel; }
    uint8_t getChannelCount() const { return _channelCount; }
    MuxChannelConfig getChannelConfig(uint8_t channel) const;
    bool isChannelEnabled(uint8_t channel) const;
    
    // 调试信息
    void printChannelInfo();
    
    // I2C总线管理
    void resetI2CBus();
    void scanI2CDevices();
    
    // OLED专用方法
    void lockOLEDChannel();
    void unlockOLEDChannel();

private:
    uint8_t _address;
    MuxChannelConfig _channels[MAX_MUX_CHANNELS];
    uint8_t _activeChannel;
    uint8_t _channelCount;
};

#endif