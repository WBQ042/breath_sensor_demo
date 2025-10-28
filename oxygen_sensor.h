#ifndef oxygen_sensor_h
#define oxygen_sensor_h

#include <Arduino.h>
#include "ADS1115.h"

// 电化学氧传感器类
class OxygenSensor {
public:
    // 构造函数（使用ADS1115）
    // ads: ADS1115指针
    // muxChannel: 用于校准的MUX通道设置（默认ADS1115_MUX_AIN0_GND）
    OxygenSensor(ADS1115* ads, uint8_t muxChannel = ADS1115_MUX_AIN0_GND);
    
    // 初始化传感器
    void begin();
    
    // 读取ADC原始值（16位ADC: 0-32767）
    int16_t readRawADC();
    
    // 读取电压值（V）
    float readVoltage();
    
    // 读取当前氧气浓度（需要先校准）
    // 返回氧气浓度百分比
    float readOxygenConcentration();
    
    // 校准函数
    // 测量短接时的ADC值作为A0
    int16_t calibrateShortCircuit();
    
    // 测量空气中（21%氧气）的ADC值作为A1
    int16_t calibrateAirEnvironment();
    
    // 手动设置校准参数
    void setCalibrationParams(int16_t a0, int16_t a1);
    
    // 获取当前校准参数
    void getCalibrationParams(int16_t &a0, int16_t &a1);
    
    // 检查是否已校准
    bool isCalibrated();
    
    // 应用移动平均滤波
    void enableFilter(bool enable) { _filterEnabled = enable; }
    
    // 设置滤波窗口大小
    void setFilterWindow(uint8_t windowSize);

private:
    ADS1115* _ads;          // ADS1115 ADC模块
    uint8_t _muxChannel;     // MUX通道设置
    int16_t _a0;             // 短接时的ADC值
    int16_t _a1;             // 空气中（21%氧气）的ADC值
    bool _isCalibrated;      // 是否已校准
    
    // 滤波相关
    bool _filterEnabled;
    static const uint8_t MAX_FILTER_SIZE = 10;
    uint8_t _filterSize;
    int16_t _filterBuffer[MAX_FILTER_SIZE];
    uint8_t _filterIndex;
    
    // 应用滤波
    int16_t applyFilter(int16_t rawValue);
};

#endif

