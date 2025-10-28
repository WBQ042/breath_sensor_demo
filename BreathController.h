#ifndef BreathController_h
#define BreathController_h

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "OLEDDisplay.h"
#include "I2CMux.h"  // 包含新的多路复用器库
#include "gas_concentration.h"  // 包含气体浓度传感器库
#include "ADS1115.h"
#include "oxygen_sensor.h"

// 硬件配置
constexpr uint8_t VALVE_PIN = 3;          // 气阀控制引脚
constexpr float BREATH_THRESHOLD = 0.5;    // 呼吸检测阈值(kPa)
constexpr uint8_t MAX_VALVE_OPEN = 255;    // 气阀最大开度

// 传感器配置
constexpr uint8_t SENSOR_ADDR = 0x6D;      // 气压传感器I2C地址
constexpr uint8_t FLOW_SENSOR_ADDR = 0x50; // 流量传感器I2C地址
constexpr uint8_t ACD1100_ADDR = 0x2A;     // ACD1100气体浓度传感器I2C地址

// 寄存器地址
constexpr uint8_t REG_SPI_CTRL = 0x00;
constexpr uint8_t REG_PART_ID = 0x01;
constexpr uint8_t REG_STATUS = 0x02;
constexpr uint8_t REG_DATA_MSB = 0x06;
constexpr uint8_t REG_DATA_CSB = 0x07;
constexpr uint8_t REG_DATA_LSB = 0x08;
constexpr uint8_t REG_TEMP_MSB = 0x09;
constexpr uint8_t REG_TEMP_LSB = 0x0A;
constexpr uint8_t REG_CMD = 0x30;
constexpr uint8_t REG_OTP_CMD = 0x6C;
constexpr uint8_t REG_SPECIAL = 0xA5;

// 命令常量
constexpr uint8_t CMD_COLLECT = 0x0A;      // 组合采集模式命令
constexpr uint8_t CMD_CLEAR = 0xFD;        // 清除特殊寄存器命令

// 量程配置
constexpr float MIN_PRESSURE = -100.0;     // kPa
constexpr float MAX_PRESSURE = 300.0;      // kPa
constexpr float PRESSURE_RANGE = MAX_PRESSURE - MIN_PRESSURE;

// 呼吸状态
enum BreathState { INHALE, EXHALE, PEAK, TROUGH };

class BreathController {
public:
    BreathController(I2CMux* mux = nullptr); // 可传入外部多路复用器实例
    void begin();
    void update();
    
    // WiFi 配置
    void setWiFiCredentials(const char* ssid, const char* password, const char* host, int port);
    
    // 多路复用器访问
    void setMux(I2CMux* mux) { _mux = mux; }
    I2CMux* getMux() { return _mux; }
    
    // ADS1115和氧传感器配置
    void setADS1115Channel(uint8_t channel);  // 设置ADS1115的I2C多路复用器通道
    void initializeOxygenSensor();  // 初始化氧传感器
    
    // 设置ACD1100通信模式
    void setACD1100CommunicationMode(ACD1100_COMM_MODE mode);

private:
    // 传感器操作
    void initSensor();
    void startAcquisition();
    uint32_t getKValue(float range_kpa);
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    int32_t readPressureADC();
    int16_t readTemperatureADC();
    bool dataCheck();
    bool operateCheck();

    float readFlowRate();       // 流量计读取操作
    
    // 数据处理
    float calculateTemperature(uint16_t adc_value);
    float calculatePressure(uint32_t adc_value, uint32_t k, float temperature);
    float applyMovingAverage(float newValue);
    float applyEWMA(float newValue);
    
    // 校准
    void calibrateZeroPoint();
    
    // 呼吸检测与控制
    BreathState detectBreathState(float pressure);
    void controlValve();
    void adaptiveModelAdjustment();
    
    // WiFi 功能
    void connectToWiFi();
    bool connectToServer();
    void sendDataOverWiFi(float pressure, float temp, float valve);
    
    // 设备探测
    void probeFlowSensor();
    
    // I2C 诊断
    void scanI2CBus();
    
    // 成员变量
    float storedPressures[10] = {0};
    float storedTemperatures[10] = {0};
    int storeIndex = 0;
    bool isBaseSet = false;
    float basePressure = 0.0;
    float baseTemperature = 0.0;

    float flowRate = 0.0;   // 当前流量值(ml/min)
    
    float pressureHistory[5];
    int historyIndex = 0;
    float filteredPressure = 0.0;
    bool isFilterInitialized = false;
    
    BreathState currentState = EXHALE;
    unsigned long lastBreathTime = 0;
    float breathPeriod = 3000;
    float minPressure = 0;
    float maxPressure = 0;
    int breathCount = 0;
    
    float valveOpening = 0;
    float assistLevel = 0.5;
    bool assistEnabled = true;
    
    float pressureThreshold = BREATH_THRESHOLD;
    float responseFactor = 1.0;
    
    // WiFi 相关
    const char* _ssid = nullptr;
    const char* _password = nullptr;
    const char* _host = nullptr;
    int _port = 0;
    WiFiClient client;
    bool wifiConnected = false;
    unsigned long lastReconnectAttempt = 0;
    
    // I2C 多路复用器
    I2CMux* _mux;
    
    // OLED 显示
    OLEDDisplay oled;
    
    // 气体浓度传感器
    ACD1100 acd1100;
    
    // ADS1115 ADC模块
    ADS1115* ads1115;
    
    // 电化学氧传感器
    OxygenSensor* oxygenSensor;

    // 流量传感器状态
    bool flowSensorAvailable = false;
    int8_t flowSensorChannel = -1;
};

#endif