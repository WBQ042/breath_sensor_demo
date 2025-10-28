#ifndef ADS1115_h
#define ADS1115_h

#include <Arduino.h>
#include <Wire.h>
#include "I2CMux.h"

// ADS1115 寄存器地址
#define ADS1115_REG_CONVERSION   0x00
#define ADS1115_REG_CONFIG        0x01
#define ADS1115_REG_LO_THRESH     0x02
#define ADS1115_REG_HI_THRESH     0x03

// ADS1115 I2C地址（7位地址）
#define ADS1115_DEFAULT_ADDRESS  0x48  // A0=GND, A1=GND

// 配置寄存器位定义
// MUX [14:12] - 输入选择器
#define ADS1115_MUX_AIN0_AIN1    0x0000  // AINP = AIN0, AINN = AIN1 (默认)
#define ADS1115_MUX_AIN0_AIN3    0x1000  // AINP = AIN0, AINN = AIN3
#define ADS1115_MUX_AIN1_AIN3    0x2000  // AINP = AIN1, AINN = AIN3
#define ADS1115_MUX_AIN2_AIN3    0x3000  // AINP = AIN2, AINN = AIN3
#define ADS1115_MUX_AIN0_GND    0x4000  // AINP = AIN0, AINN = GND
#define ADS1115_MUX_AIN1_GND    0x5000  // AINP = AIN1, AINN = GND
#define ADS1115_MUX_AIN2_GND    0x6000  // AINP = AIN2, AINN = GND
#define ADS1115_MUX_AIN3_GND    0x7000  // AINP = AIN3, AINN = GND

// PGA [11:9] - 可编程增益放大器
#define ADS1115_PGA_6144V        0x0000  // ±6.144V
#define ADS1115_PGA_4096V        0x0200  // ±4.096V
#define ADS1115_PGA_2048V        0x0400  // ±2.048V (默认)
#define ADS1115_PGA_1024V        0x0600  // ±1.024V
#define ADS1115_PGA_512V         0x0800  // ±0.512V
#define ADS1115_PGA_256V         0x0A00  // ±0.256V

// MODE [8] - 工作模式
#define ADS1115_MODE_CONTINUOUS  0x0000  // 连续转换模式
#define ADS1115_MODE_SINGLE      0x0100  // 单次转换模式 (默认)

// DR [7:5] - 数据速率
#define ADS1115_DR_8SPS          0x0000
#define ADS1115_DR_16SPS         0x0020
#define ADS1115_DR_32SPS         0x0040
#define ADS1115_DR_64SPS         0x0060
#define ADS1115_DR_128SPS        0x0080  // 默认
#define ADS1115_DR_250SPS       0x00A0
#define ADS1115_DR_475SPS        0x00C0
#define ADS1115_DR_860SPS        0x00E0

// 其他位
#define ADS1115_OS_BUSY          0x8000  // 操作状态位
#define ADS1115_COMP_TRAD       0x0000  // 传统比较器
#define ADS1115_COMP_WINDOW     0x0010  // 窗口比较器
#define ADS1115_COMP_LAT        0x0008  // 锁存
#define ADS1115_COMP_QUE_DIS    0x0003  // 禁用比较器

// 默认配置
#define ADS1115_DEFAULT_CONFIG   (ADS1115_MUX_AIN0_GND | \
                                  ADS1115_PGA_2048V | \
                                  ADS1115_MODE_SINGLE | \
                                  ADS1115_DR_128SPS | \
                                  ADS1115_COMP_QUE_DIS)

class ADS1115 {
public:
    ADS1115(uint8_t address = ADS1115_DEFAULT_ADDRESS, I2CMux* mux = nullptr, uint8_t channel = 0);
    
    // 初始化和配置
    bool begin(TwoWire &wirePort = Wire);
    bool isConnected();
    bool selectChannel();
    
    // 多路复用器支持
    void setMuxChannel(I2CMux* mux, uint8_t channel);
    
    // 读取原始ADC值
    int16_t readRaw(uint8_t mux = ADS1115_MUX_AIN0_GND);
    
    // 读取电压值（V）
    float readVoltage(uint8_t mux = ADS1115_MUX_AIN0_GND);
    
    // 配置函数
    void setGain(uint8_t gain);
    void setDataRate(uint8_t dr);
    void setMode(uint8_t mode);
    
    // 完整配置
    bool configure(uint16_t config = ADS1115_DEFAULT_CONFIG);
    
    // 获取当前配置
    uint16_t getConfig();
    
    // 等待转换完成
    bool waitForConversion(unsigned long timeout = 100);
    
    // 测试函数
    void scanAddress();

private:
    TwoWire* _i2cPort;
    uint8_t _address;
    I2CMux* _mux;
    uint8_t _channel;
    uint16_t _currentConfig;
    
    // I2C通信函数
    bool writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t reg);
    void writeConfig(uint16_t config);
};

#endif

