#ifndef gas_concentration_h
#define gas_concentration_h

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "OLEDDisplay.h"
#include "I2CMux.h"
#include "oxygen_sensor.h"

#define ACD1100_I2C_ADDR 0x2A  // 7位地址，Arduino自动处理8位转换
#define ACD1100_UART_BAUD 1200  // UART波特率

// 通信模式枚举
enum ACD1100_COMM_MODE {
    COMM_I2C = 0,
    COMM_UART = 1
};

class ACD1100 {
public:
    // 构造函数
    ACD1100(I2CMux* mux = nullptr, uint8_t channel = 0, ACD1100_COMM_MODE mode = COMM_I2C);
    bool update();  // 主要更新函数
    bool isDataReady();  // 检查数据是否准备好
    float getFilteredCO2();  // 获取滤波后的CO2值
    float getFilteredTemperature();  // 获取滤波后的温度值
    uint8_t getAirQuality();  // 获取空气质量等级
    
    // 初始化函数
    bool begin(TwoWire &wirePort = Wire, HardwareSerial* serialPort = nullptr);
    
    // 通信模式切换
    void setCommunicationMode(ACD1100_COMM_MODE mode);
    ACD1100_COMM_MODE getCommunicationMode();
    
    // 多路复用器相关
    void setMuxChannel(I2CMux* mux, uint8_t channel);
    bool selectSensorChannel();
    
    // 测试函数
    bool testSimpleRead();
    void scanI2CAddresses();
    void testMuxChannels();
    void checkMuxStatus();
    
    // 主要功能函数
    bool readCO2(uint32_t &co2_ppm, float &temperature);
    bool readCO2I2C(uint32_t &co2_ppm, float &temperature);
    bool readCO2UART(uint32_t &co2_ppm, float &temperature);
    uint32_t getCO2();
    float getTemperature();
    
    // 校准功能
    bool setCalibrationMode(bool autoMode);  // true=自动, false=手动
    bool getCalibrationMode();               // 返回当前模式
    bool manualCalibration(uint16_t targetPPM = 450);  // 手动校准，默认450ppm
    bool factoryReset();
    
    // 信息读取
    String getSoftwareVersion();
    String getSensorID();
    
    // 状态检查
    bool isConnected();
    uint8_t getLastError();

    float filteredCO2;
    float filteredTemperature;
    uint32_t lastUpdateTime;
    uint8_t airQuality;
    bool dataValid;

private:
    // I2C相关
    TwoWire *_i2cPort;
    I2CMux* _mux;
    uint8_t _channel;
    
    // UART相关
    HardwareSerial* _serialPort;
    ACD1100_COMM_MODE _commMode;
    
    // 数据缓存
    uint32_t _lastCO2;
    float _lastTemp;
    uint8_t _lastError;

    // 滤波相关
    float applyMovingAverage(float newValue);
    float applyEWMA(float newValue);
    void updateAirQuality();

    // 滤波缓冲区
    static const uint8_t MOVING_AVG_SIZE = 5;
    float co2Buffer[MOVING_AVG_SIZE];
    float tempBuffer[MOVING_AVG_SIZE];
    uint8_t bufferIndex;
    float previousCO2;
    float previousTemp;
    
    // I2C通信函数（内部使用，保持兼容性）
    bool sendCommand(uint8_t cmdHigh, uint8_t cmdLow, uint8_t *data = nullptr, uint8_t dataLen = 0);
    bool readResponse(uint8_t *buffer, uint8_t bufferSize);
    bool sendCommandI2C(uint8_t cmdHigh, uint8_t cmdLow, uint8_t *data = nullptr, uint8_t dataLen = 0);
    bool readResponseI2C(uint8_t *buffer, uint8_t bufferSize);
    
    // UART通信函数
    bool sendCommandUART(uint8_t cmd, uint8_t *data = nullptr, uint8_t dataLen = 0);
    bool readResponseUART(uint8_t *buffer, uint8_t bufferSize);
    uint8_t calculateCheckSum(uint8_t *data, uint8_t length);
    
    // 通用CRC函数
    uint8_t calculateCRC8(uint8_t *data, uint8_t length);
    
    // 错误码定义
    enum ErrorCode {
        ERROR_NONE = 0,
        ERROR_I2C_COMMUNICATION,
        ERROR_CRC_MISMATCH,
        ERROR_SENSOR_NOT_RESPONDING,
        ERROR_INVALID_DATA
    };
};

#endif