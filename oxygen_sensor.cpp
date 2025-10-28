#include "oxygen_sensor.h"

// 构造函数
OxygenSensor::OxygenSensor(ADS1115* ads, uint8_t muxChannel)
    : _ads(ads), _muxChannel(muxChannel), _a0(0), _a1(0), _isCalibrated(false),
      _filterEnabled(true), _filterSize(5), _filterIndex(0) {
    
    // 初始化滤波缓冲区
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        _filterBuffer[i] = 0;
    }
}

// 初始化传感器
void OxygenSensor::begin() {
    if (_ads == nullptr) {
        Serial.println("氧传感器: ADS1115未初始化！");
        return;
    }
    
    Serial.println("氧传感器初始化");
    Serial.println("使用ADS1115 16位ADC进行读取");
    Serial.println("请确保:");
    Serial.println("1. 传感器正极（Vsensor+）连接到ADS1115的AIN0");
    Serial.println("2. 传感器负极（Vsensor-）连接到ADS1115的GND");
}

// 读取ADC原始值
int16_t OxygenSensor::readRawADC() {
    if (_ads == nullptr) {
        return 0;
    }
    return _ads->readRaw(_muxChannel);
}

// 读取电压值
float OxygenSensor::readVoltage() {
    if (_ads == nullptr) {
        return 0.0;
    }
    return _ads->readVoltage(_muxChannel);
}

// 读取氧气浓度
float OxygenSensor::readOxygenConcentration() {
    if (!_isCalibrated) {
        Serial.println("警告: 氧传感器未校准，返回0");
        return 0.0;
    }
    
    if (_ads == nullptr) {
        Serial.println("警告: ADS1115未初始化");
        return 0.0;
    }
    
    // 读取ADC值
    int16_t rawADC = readRawADC();
    
    // 应用滤波
    if (_filterEnabled) {
        rawADC = applyFilter(rawADC);
    }
    
    // 应用计算公式: 氧气浓度 = (Ax − A0) × 20.9/(A1 − A0)
    if (_a1 == _a0) {
        Serial.println("警告: 校准参数异常，A1 == A0");
        return 0.0;
    }
    
    float oxygenPercent = ((float)(rawADC - _a0) * 20.9) / (float)(_a1 - _a0);
    
    // 限制输出范围在合理范围内（0-30%）
    oxygenPercent = constrain(oxygenPercent, 0.0, 30.0);
    
    return oxygenPercent;
}

// 校准：测量短接时的ADC值
int16_t OxygenSensor::calibrateShortCircuit() {
    if (_ads == nullptr) {
        Serial.println("错误: ADS1115未初始化");
        return 0;
    }
    
    Serial.println("\n=== 开始短接校准（A0） ===");
    Serial.println("请将传感器的正负极（Vsensor+与Vsensor-）短接");
    Serial.println("等待5秒后开始测量...");
    
    delay(5000);
    
    // 读取多次并取平均值
    const int samples = 20;
    long sum = 0;
    
    for (int i = 0; i < samples; i++) {
        int16_t value = readRawADC();
        sum += value;
        delay(100); // ADS1115需要更多时间
    }
    
    _a0 = sum / samples;
    
    Serial.print("短接校准完成！A0 = ");
    Serial.println(_a0);
    Serial.print("对应电压: ");
    Serial.print(_ads->readVoltage(_muxChannel), 4);
    Serial.println(" V");
    Serial.println("=== 短接校准完成 ===\n");
    
    return _a0;
}

// 校准：测量空气中（21%氧气）的ADC值
int16_t OxygenSensor::calibrateAirEnvironment() {
    if (_ads == nullptr) {
        Serial.println("错误: ADS1115未初始化");
        return 0;
    }
    
    Serial.println("\n=== 开始空气环境校准（A1） ===");
    Serial.println("请将传感器置于空气中（21%氧气环境）");
    Serial.println("等待10秒让传感器稳定...");
    
    delay(10000);
    
    // 读取多次并取平均值
    const int samples = 20;
    long sum = 0;
    
    for (int i = 0; i < samples; i++) {
        int16_t value = readRawADC();
        sum += value;
        delay(100); // ADS1115需要更多时间
    }
    
    _a1 = sum / samples;
    
    Serial.print("空气环境校准完成！A1 = ");
    Serial.println(_a1);
    Serial.print("对应电压: ");
    Serial.print(_ads->readVoltage(_muxChannel), 4);
    Serial.println(" V");
    
    // 检查校准参数是否合理
    if (abs(_a1 - _a0) < 100) {
        Serial.println("警告: A1和A0差值过小，可能校准有问题");
    }
    
    _isCalibrated = true;
    Serial.println("=== 空气环境校准完成 ===\n");
    
    return _a1;
}

// 手动设置校准参数
void OxygenSensor::setCalibrationParams(int16_t a0, int16_t a1) {
    _a0 = a0;
    _a1 = a1;
    _isCalibrated = true;
    
    Serial.print("校准参数已设置: A0 = ");
    Serial.print(_a0);
    Serial.print(", A1 = ");
    Serial.println(_a1);
}

// 获取校准参数
void OxygenSensor::getCalibrationParams(int16_t &a0, int16_t &a1) {
    a0 = _a0;
    a1 = _a1;
}

// 检查是否已校准
bool OxygenSensor::isCalibrated() {
    return _isCalibrated;
}

// 设置滤波窗口大小
void OxygenSensor::setFilterWindow(uint8_t windowSize) {
    if (windowSize > 0 && windowSize <= MAX_FILTER_SIZE) {
        _filterSize = windowSize;
    }
}

// 应用移动平均滤波
int16_t OxygenSensor::applyFilter(int16_t rawValue) {
    // 更新滤波缓冲区
    _filterBuffer[_filterIndex] = rawValue;
    _filterIndex = (_filterIndex + 1) % _filterSize;
    
    // 计算平均值
    long sum = 0;
    for (uint8_t i = 0; i < _filterSize; i++) {
        sum += _filterBuffer[i];
    }
    
    return (int16_t)(sum / _filterSize);
}
