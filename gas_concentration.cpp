#include "gas_concentration.h"

ACD1100::ACD1100(I2CMux* mux, uint8_t channel, ACD1100_COMM_MODE mode) 
    : _mux(mux), _channel(channel), _commMode(mode), _serialPort(nullptr) {
    _lastCO2 = 0;
    _lastTemp = 0.0;
    _lastError = ERROR_NONE;
    bufferIndex = 0;
    previousCO2 = 0;
    previousTemp = 0;
    
    // 初始化滤波缓冲区
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        co2Buffer[i] = 0;
        tempBuffer[i] = 0;
    }
}

//ACD1100初始化
bool ACD1100::begin(TwoWire &wirePort, HardwareSerial* serialPort) {
    if (_commMode == COMM_I2C) {
        _i2cPort = &wirePort;
        _i2cPort->begin();
    } else {
        // UART模式
        if (serialPort == nullptr) {
            Serial.println("ACD1100: UART模式需要传入有效的serialPort指针");
            return false;
        }
        _serialPort = serialPort;
        _serialPort->begin(ACD1100_UART_BAUD);
        Serial.print("ACD1100: UART串口已初始化，波特率: ");
        Serial.println(ACD1100_UART_BAUD);
    }
    
    // 检查传感器是否连接
    return isConnected();
}

//检测ACD1100是否完成连接
bool ACD1100::isConnected() {
    if (_commMode == COMM_UART) {
        // UART模式：只需检查串口是否已初始化
        Serial.println("ACD1100 UART模式: 检查串口初始化");
        if (_serialPort == nullptr) {
            Serial.println("ACD1100 UART: 串口未初始化");
            return false;
        }
        Serial.println("ACD1100 UART: 串口已初始化（后续通过实际读取测试连接）");
        return true;  // UART模式无法通过类似I2C的方式测试，需要通过实际读取来验证
    } else {
        // I2C模式：原有检测逻辑
        if (!selectSensorChannel()) {
            Serial.println("ACD1100: 无法选择通道");
            return false;
        }
        
        Serial.print("ACD1100: 测试传感器地址0x");
        Serial.println(ACD1100_I2C_ADDR, HEX);
        
        // 测试传感器地址
        _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
        uint8_t result = _i2cPort->endTransmission();
        
        Serial.print("ACD1100: 传感器地址测试结果: ");
        Serial.println(result);
        
        // 如果连接失败，尝试扫描I2C总线
        if (result != 0) {
            Serial.println("ACD1100: 标准地址无响应，开始详细诊断...");
            
            // 首先检查多路复用器状态
            Serial.println("ACD1100: 检查多路复用器状态...");
            checkMuxStatus();
            
            // 然后扫描I2C总线
            Serial.println("ACD1100: 开始I2C扫描...");
            scanI2CAddresses();
            
            // 最后测试多路复用器通道
            Serial.println("ACD1100: 测试多路复用器通道...");
            testMuxChannels();
        }
        
        return (result == 0);
    }
}

// 设置通信模式
void ACD1100::setCommunicationMode(ACD1100_COMM_MODE mode) {
    _commMode = mode;
    Serial.print("ACD1100: 通信模式切换为: ");
    Serial.println(mode == COMM_I2C ? "I2C" : "UART");
}

// 获取通信模式
ACD1100_COMM_MODE ACD1100::getCommunicationMode() {
    return _commMode;
}

//ACD1100开始读取CO2浓度数据
bool ACD1100::readCO2(uint32_t &co2_ppm, float &temperature) {
    if (_commMode == COMM_UART) {
        return readCO2UART(co2_ppm, temperature);
    } else {
        return readCO2I2C(co2_ppm, temperature);
    }
}

// I2C方式读取CO2
bool ACD1100::readCO2I2C(uint32_t &co2_ppm, float &temperature) {
    if (!selectSensorChannel()) {
        _lastError = ERROR_I2C_COMMUNICATION;
        return false;
    }
    
    // 发送读取命令
    Serial.println("ACD1100: 发送读取命令");
    _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
    _i2cPort->write(0x03);
    _i2cPort->write(0x00);
    if (_i2cPort->endTransmission() != 0) {
        Serial.println("ACD1100: 命令发送失败");
        _lastError = ERROR_I2C_COMMUNICATION;
        return false;
    }
    Serial.println("ACD1100: 命令发送成功");
    
    delay(100); // 增加等待时间
    
    // 按照手册，上行数据格式为：地址(1) + 4字节CO2 + 2字节CRC + 2字节Temp + 1字节CRC = 10 字节
    // 实际上是： 地址(1) + PPM3(1) + PPM2(1) + CRC1(1) + PPM1(1) + PPM0(1) + CRC2(1) + TempH(1) + TempL(1) + CRC3(1)
    // 传感器实际响应数据是 10 字节，第一个字节是响应地址 0x55。
    // requestFrom读取：PPM3, PPM2, CRC1, PPM1, PPM0, CRC2, TempH, TempL, CRC3
    // 这里我们请求 9 字节数据（跳过地址0x55），或者请求10字节（包含地址0x55）
    // 为了兼容，我们请求 10 字节，然后检查第一个字节是否为 0x55
    uint8_t response[10]; // 地址 + 9字节数据
    
    // 读取传感器数据
    Serial.println("ACD1100: 读取传感器数据");
    uint8_t bytesRead = _i2cPort->requestFrom(ACD1100_I2C_ADDR, 10);
    Serial.print("ACD1100: 请求10字节，实际收到");
    Serial.print(bytesRead);
    Serial.println("字节");
    
    if (bytesRead != 10) {
        Serial.print("ACD1100: 期望10字节，实际收到");
        Serial.print(bytesRead);
        Serial.println("字节");
        _lastError = ERROR_SENSOR_NOT_RESPONDING;
        return false;
    }
    
    // 读取数据
    for (uint8_t i = 0; i < 10; i++) {
        response[i] = _i2cPort->read();
    }
    
    // 输出原始数据用于调试
    Serial.print("ACD1100原始数据: ");
    for (uint8_t i = 0; i < 10; i++) {
        Serial.print("0x");
        if (response[i] < 16) Serial.print("0");
        Serial.print(response[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // 检查地址字节 - 根据实际数据调整
    if (response[0] != 0x55 && response[0] != 0x00) {
        Serial.print("ACD1100: 地址错误，期望0x55或0x00，实际0x");
        Serial.println(response[0], HEX);
        _lastError = ERROR_INVALID_DATA;
        return false;
    }
    
    // 如果第一个字节是0x00，可能需要跳过地址字节
    uint8_t dataStart = (response[0] == 0x55) ? 1 : 0;
    Serial.print("ACD1100: 数据起始位置: ");
    Serial.println(dataStart);
    
    // 根据实际数据调整CRC校验
    uint8_t calculatedCRC;
    bool crcValid = true;
    
    // 1. 校验 CO2 高位 2 字节
    calculatedCRC = calculateCRC8(&response[dataStart + 0], 2);
    Serial.print("ACD1100: CO2高位CRC - 计算值: 0x");
    Serial.print(calculatedCRC, HEX);
    Serial.print(", 实际值: 0x");
    Serial.println(response[dataStart + 2], HEX);
    
    if (response[dataStart + 2] != calculatedCRC) {
        Serial.println("ACD1100: CO2高位CRC错误，但继续处理");
        crcValid = false;
    }

    // 2. 校验 CO2 低位 2 字节
    calculatedCRC = calculateCRC8(&response[dataStart + 3], 2);
    Serial.print("ACD1100: CO2低位CRC - 计算值: 0x");
    Serial.print(calculatedCRC, HEX);
    Serial.print(", 实际值: 0x");
    Serial.println(response[dataStart + 5], HEX);
    
    if (response[dataStart + 5] != calculatedCRC) {
        Serial.println("ACD1100: CO2低位CRC错误，但继续处理");
        crcValid = false;
    }
    
    // 3. 校验 温度 2 字节
    calculatedCRC = calculateCRC8(&response[dataStart + 6], 2);
    Serial.print("ACD1100: 温度CRC - 计算值: 0x");
    Serial.print(calculatedCRC, HEX);
    Serial.print(", 实际值: 0x");
    Serial.println(response[dataStart + 8], HEX);
    
    if (response[dataStart + 8] != calculatedCRC) {
        Serial.println("ACD1100: 温度CRC错误，但继续处理");
        crcValid = false;
    }
    
    if (!crcValid) {
        Serial.println("ACD1100: CRC校验失败，但尝试解析数据");
    }
    
    // 计算 CO2 浓度 (4 字节，高字节在前)
    // 根据实际数据调整索引
    co2_ppm = ((uint32_t)response[dataStart + 0] << 24) | 
              ((uint32_t)response[dataStart + 1] << 16) | 
              ((uint32_t)response[dataStart + 3] << 8) | 
              response[dataStart + 4];
    
    // 计算温度
    int16_t temp_raw = ((int16_t)response[dataStart + 6] << 8) | response[dataStart + 7];
    temperature = temp_raw / 100.0; // 温度转换
    
    Serial.print("ACD1100: 原始CO2数据 - ");
    Serial.print(response[dataStart + 0], HEX);
    Serial.print(" ");
    Serial.print(response[dataStart + 1], HEX);
    Serial.print(" ");
    Serial.print(response[dataStart + 3], HEX);
    Serial.print(" ");
    Serial.print(response[dataStart + 4], HEX);
    Serial.print(" -> ");
    Serial.print(co2_ppm);
    Serial.println(" ppm");
    
    Serial.print("ACD1100: 原始温度数据 - ");
    Serial.print(response[dataStart + 6], HEX);
    Serial.print(" ");
    Serial.print(response[dataStart + 7], HEX);
    Serial.print(" -> ");
    Serial.print(temperature);
    Serial.println(" °C");
    
    _lastCO2 = co2_ppm;
    _lastTemp = temperature;
    _lastError = ERROR_NONE;
    
    return true;
}

uint32_t ACD1100::getCO2() {
    uint32_t co2;
    float temp;
    if (readCO2(co2, temp)) {
        return co2;
    }
    return 0;
}

float ACD1100::getTemperature() {
    uint32_t co2;
    float temp;
    if (readCO2(co2, temp)) {
        return temp;
    }
    return -273.15; // 错误时返回绝对零度
}

bool ACD1100::setCalibrationMode(bool autoMode) {
    uint8_t data[2] = {0x00, autoMode ? 0x01 : 0x00};
    
    if (!sendCommand(0x53, 0x06, data, 2)) {
        _lastError = ERROR_I2C_COMMUNICATION;
        return false;
    }
    
    delay(5); // 等待5ms以上
    
    // 验证设置是否成功
    uint8_t response[4];
    if (sendCommand(0x53, 0x06) && readResponse(response, 4)) {
        return (response[3] == (autoMode ? 0x01 : 0x00));
    }
    
    return false;
}

bool ACD1100::getCalibrationMode() {
    uint8_t response[4];
    
    if (sendCommand(0x53, 0x06) && readResponse(response, 4)) {
        return (response[3] == 0x01); // 1=自动, 0=手动
    }
    
    return false;
}

bool ACD1100::manualCalibration(uint16_t targetPPM) {
    uint8_t data[2] = {(uint8_t)(targetPPM >> 8), (uint8_t)(targetPPM & 0xFF)};
    
    if (!sendCommand(0x52, 0x04, data, 2)) {
        _lastError = ERROR_I2C_COMMUNICATION;
        return false;
    }
    
    delay(5); // 等待5ms以上
    
    // 验证校准值是否设置成功
    uint8_t response[4];
    if (sendCommand(0x52, 0x04) && readResponse(response, 4)) {
        uint16_t readValue = (response[1] << 8) | response[2];
        return (readValue == targetPPM);
    }
    
    return false;
}

bool ACD1100::factoryReset() {
    uint8_t data[1] = {0x00};
    
    if (!sendCommand(0x52, 0x02, data, 1)) {
        _lastError = ERROR_I2C_COMMUNICATION;
        return false;
    }
    
    delay(5);
    
    // 检查恢复结果
    uint8_t response[4];
    if (sendCommand(0x52, 0x02) && readResponse(response, 4)) {
        return (response[3] == 0x01); // 1=成功
    }
    
    return false;
}

String ACD1100::getSoftwareVersion() {
    uint8_t response[11]; // 地址 + 10个ASCII码 + 停止位
    
    if (sendCommand(0xD1, 0x00) && readResponse(response, 11)) {
        char version[11] = {0};
        for (int i = 0; i < 10; i++) {
            version[i] = (char)response[i+1];
        }
        return String(version);
    }
    
    return "Unknown";
}

String ACD1100::getSensorID() {
    uint8_t response[11]; // 地址 + 10个ASCII码
    
    if (sendCommand(0xD2, 0x01) && readResponse(response, 11)) {
        char sensorID[11] = {0};
        for (int i = 0; i < 10; i++) {
            sensorID[i] = (char)response[i+1];
        }
        return String(sensorID);
    }
    
    return "Unknown";
}

// 私有通信函数
// 原sendCommand和readResponse保留为sendCommandI2C和readResponseI2C
bool ACD1100::sendCommand(uint8_t cmdHigh, uint8_t cmdLow, uint8_t *data, uint8_t dataLen) {
    return sendCommandI2C(cmdHigh, cmdLow, data, dataLen);
}

bool ACD1100::readResponse(uint8_t *buffer, uint8_t bufferSize) {
    return readResponseI2C(buffer, bufferSize);
}

uint8_t ACD1100::calculateCRC8(uint8_t *data, uint8_t length) {
    uint8_t crc = 0xFF;
    
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    
    return crc;
}

uint8_t ACD1100::getLastError() {
    return _lastError;
}

bool ACD1100::update() {
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    // 检查是否达到数据刷新间隔(2秒)
    if (currentTime - lastReadTime < 2000) {
        return dataValid; // 未到读取时间，返回之前的状态
    }
    
    lastReadTime = currentTime;
    
    uint32_t rawCO2;
    float rawTemperature;
    
    // 读取传感器数据
    if (!readCO2(rawCO2, rawTemperature)) {
        dataValid = false;
        _lastError = ERROR_SENSOR_NOT_RESPONDING;
        return false;
    }
    
    // 数据有效性检查
    if (rawCO2 < 400 || rawCO2 > 5000) {
        dataValid = false;
        _lastError = ERROR_INVALID_DATA;
        return false;
    }
    
    // 应用双重滤波
    float tempFiltered = applyMovingAverage(rawTemperature);
    tempFiltered = applyEWMA(tempFiltered);
    
    float co2Filtered = applyMovingAverage((float)rawCO2);
    co2Filtered = applyEWMA(co2Filtered);
    
    // 更新滤波后的值
    filteredTemperature = tempFiltered;
    filteredCO2 = co2Filtered;
    
    // 更新空气质量评估
    updateAirQuality();
    
    dataValid = true;
    _lastError = ERROR_NONE;
    
    delay(200);
    
    return true;
}

bool ACD1100::isDataReady() {
    return (millis() - lastUpdateTime >= 2000) && dataValid;
}

float ACD1100::getFilteredCO2() {
    return filteredCO2;
}

float ACD1100::getFilteredTemperature() {
    return filteredTemperature;
}

uint8_t ACD1100::getAirQuality() {
    return airQuality;
}

float ACD1100::applyMovingAverage(float newValue) {
    // 更新循环缓冲区
    co2Buffer[bufferIndex] = newValue;
    bufferIndex = (bufferIndex + 1) % MOVING_AVG_SIZE;
    
    // 计算移动平均
    float sum = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        if (co2Buffer[i] > 0) { // 只计算有效数据
            sum += co2Buffer[i];
            count++;
        }
    }
    
    return (count > 0) ? (sum / count) : newValue;
}

float ACD1100::applyEWMA(float newValue) {
    const float alpha = 0.3; // EWMA系数
    float &previous = (newValue == filteredCO2) ? previousCO2 : previousTemp;
    
    if (previous == 0) {
        previous = newValue; // 首次使用
    } else {
        previous = alpha * newValue + (1 - alpha) * previous;
    }
    
    return previous;
}

void ACD1100::updateAirQuality() {
    // 根据CO2浓度评估空气质量
    if (filteredCO2 <= 800) {
        airQuality = 1; // 优秀
    } else if (filteredCO2 <= 1200) {
        airQuality = 2; // 良好
    } else if (filteredCO2 <= 2000) {
        airQuality = 3; // 一般
    } else if (filteredCO2 <= 3000) {
        airQuality = 4; // 差
    } else {
        airQuality = 5; // 极差
    }
}

// 多路复用器相关方法
void ACD1100::setMuxChannel(I2CMux* mux, uint8_t channel) {
    _mux = mux;
    _channel = channel;
}

bool ACD1100::selectSensorChannel() {
    if (!_mux) {
        return true; // 没有多路复用器，直接返回成功
    }
    
    if (!_mux->selectChannel(_channel)) {
        Serial.print("ACD1100: 无法选择通道 ");
        Serial.println(_channel);
        return false;
    }
    
    delay(20); // 给多路复用器切换时间
    return true;
}

// 添加简化的测试读取函数
bool ACD1100::testSimpleRead() {
    if (!selectSensorChannel()) {
        Serial.println("ACD1100: 无法选择通道");
        return false;
    }
    
    Serial.println("ACD1100: 尝试简化读取测试");
    
    // 发送读取命令
    _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
    _i2cPort->write(0x03);
    _i2cPort->write(0x00);
    uint8_t sendResult = _i2cPort->endTransmission();
    Serial.print("ACD1100: 发送命令结果: ");
    Serial.println(sendResult);
    
    if (sendResult != 0) {
        return false;
    }
    
    delay(100);
    
    // 尝试读取数据
    uint8_t testBytes = _i2cPort->requestFrom(ACD1100_I2C_ADDR, 1);
    Serial.print("ACD1100: 测试读取1字节，收到");
    Serial.print(testBytes);
    Serial.println("字节");
    
    if (testBytes > 0) {
        uint8_t testData = _i2cPort->read();
        Serial.print("ACD1100: 测试数据: 0x");
        Serial.println(testData, HEX);
        return true;
    }
    
    return false;
}

// I2C地址扫描函数
void ACD1100::scanI2CAddresses() {
    Serial.println("ACD1100: 开始I2C地址扫描...");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        _i2cPort->beginTransmission(address);
        uint8_t error = _i2cPort->endTransmission();
        
        if (error == 0) {
            Serial.print("ACD1100: 找到设备，地址: 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.print(" (");
            Serial.print(address);
            Serial.println(")");
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("ACD1100: 未找到任何I2C设备！");
        Serial.println("ACD1100: 可能的问题:");
        Serial.println("1. 传感器未连接");
        Serial.println("2. 多路复用器通道错误");
        Serial.println("3. 电源问题");
        Serial.println("4. I2C接线问题");
    } else {
        Serial.print("ACD1100: 总共找到 ");
        Serial.print(deviceCount);
        Serial.println(" 个I2C设备");
    }
}

// 测试多路复用器通道
void ACD1100::testMuxChannels() {
    if (_mux == nullptr) {
        Serial.println("ACD1100: 多路复用器未设置");
        return;
    }
    
    Serial.print("ACD1100: 当前配置通道: ");
    Serial.println(_channel);
    Serial.print("ACD1100: 多路复用器总通道数: ");
    Serial.println(_mux->getChannelCount());
    
    // 测试所有通道
    for (uint8_t i = 0; i < _mux->getChannelCount(); i++) {
        Serial.print("ACD1100: 测试通道 ");
        Serial.print(i);
        Serial.print("...");
        
        if (_mux->selectChannel(i)) {
            Serial.print(" 选择成功");
            
            // 测试I2C通信
            _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
            uint8_t testResult = _i2cPort->endTransmission();
            
            if (testResult == 0) {
                Serial.println(" - 找到ACD1100！");
                Serial.print("ACD1100: 建议将传感器配置到通道 ");
                Serial.println(i);
                return;
            } else {
                Serial.print(" - 无响应 (结果:");
                Serial.print(testResult);
                Serial.println(")");
            }
        } else {
            Serial.println(" - 选择失败");
        }
    }
    
    Serial.println("ACD1100: 在所有通道上都未找到传感器");
}

// 检查多路复用器状态
void ACD1100::checkMuxStatus() {
    if (_mux == nullptr) {
        Serial.println("ACD1100: 多路复用器未设置！");
        return;
    }
    
    Serial.print("ACD1100: 多路复用器地址: 0x");
    Serial.println(0x70, HEX);  // TCA9548默认地址
    Serial.print("ACD1100: 配置通道: ");
    Serial.println(_channel);
    Serial.print("ACD1100: 总通道数: ");
    Serial.println(_mux->getChannelCount());
    
    // 检查多路复用器是否响应
    Serial.println("ACD1100: 测试多路复用器I2C通信...");
    _i2cPort->beginTransmission(0x70);  // TCA9548地址
    uint8_t muxResult = _i2cPort->endTransmission();
    Serial.print("ACD1100: 多路复用器通信结果: ");
    Serial.println(muxResult);
    
    if (muxResult != 0) {
        Serial.println("ACD1100: 多路复用器无响应！");
        return;
    }
    
    // 检查当前通道是否启用
    Serial.print("ACD1100: 检查通道 ");
    Serial.print(_channel);
    Serial.print(" 是否启用...");
    
    // 尝试选择通道
    if (_mux->selectChannel(_channel)) {
        Serial.println(" 成功");
        
        // 测试通道选择后的I2C通信
        Serial.println("ACD1100: 测试通道选择后的I2C通信...");
        _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
        uint8_t testResult = _i2cPort->endTransmission();
        
        Serial.print("ACD1100: 通道选择后测试结果: ");
        Serial.println(testResult);
        
    } else {
        Serial.println(" 失败");
        Serial.println("ACD1100: 无法选择配置的通道！");
    }
}

// UART方式读取CO2
bool ACD1100::readCO2UART(uint32_t &co2_ppm, float &temperature) {
    if (_serialPort == nullptr) {
        Serial.println("ACD1100: UART端口未初始化");
        _lastError = ERROR_SENSOR_NOT_RESPONDING;
        return false;
    }
    
    // UART协议格式: FEA60001A7
    // 帧头(0xFE) + 固定码(0xA6) + 长度(0x00) + 命令(0x01) + 校验和(0xA7)
    uint8_t cmd[] = {0xFE, 0xA6, 0x00, 0x01, 0xA7};
    
    Serial.print("ACD1100 UART: 发送命令: ");
    for (uint8_t i = 0; i < 5; i++) {
        Serial.print("0x");
        if (cmd[i] < 0x10) Serial.print("0");
        Serial.print(cmd[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // 清空接收缓冲区
    while (_serialPort->available()) {
        uint8_t dummy = _serialPort->read();
    }
    Serial.println("ACD1100 UART: 缓冲区已清空");
    
    // 发送命令
    Serial.println("ACD1100 UART: 开始发送命令...");
    for (uint8_t i = 0; i < 5; i++) {
        _serialPort->write(cmd[i]);
        Serial.print("  发送字节");
        Serial.print(i);
        Serial.print(": 0x");
        if (cmd[i] < 0x10) Serial.print("0");
        Serial.println(cmd[i], HEX);
        delay(5); // 每个字节之间延迟5ms
    }
    _serialPort->flush();  // 确保数据发送完毕
    Serial.println("ACD1100 UART: 命令发送完成，等待响应...");
    
    delay(1000); // 增加等待时间到1秒
    
    // 读取响应
    uint8_t response[10];
    uint8_t bytesRead = 0;
    
    Serial.print("ACD1100 UART: 可用字节数: ");
    Serial.println(_serialPort->available());
    
    // 读取所有可用数据（最多10字节）
    uint8_t availableBytes = _serialPort->available();
    uint8_t maxRead = (availableBytes > 10) ? 10 : availableBytes;
    for (uint8_t i = 0; i < maxRead; i++) {
        response[i] = _serialPort->read();
        bytesRead++;
    }
    
    // 打印接收到的原始数据
    if (bytesRead > 0) {
        Serial.print("ACD1100 UART: 接收到 ");
        Serial.print(bytesRead);
        Serial.print(" 字节，原始数据: ");
        for (uint8_t i = 0; i < bytesRead; i++) {
            Serial.print("0x");
            if (response[i] < 0x10) Serial.print("0");
            Serial.print(response[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    if (bytesRead != 10) {
        Serial.print("ACD1100 UART: 期望10字节，实际收到");
        Serial.print(bytesRead);
        Serial.println("字节");
        
        if (bytesRead == 0) {
            Serial.println("ACD1100 UART: 未收到任何数据！请检查:");
            Serial.println("  1. TX和RX连接是否正确（TX-RX交叉连接）");
            Serial.println("  2. GND是否连接");
            Serial.println("  3. 传感器是否通电");
            Serial.println("  4. 传感器是否配置为UART模式（Pin5接低电平）");
        }
        
        _lastError = ERROR_SENSOR_NOT_RESPONDING;
        return false;
    }
    
    // 解析响应数据: FEA60401D1D2D3D4CS
    // 帧头(0xFE) + 固定码(0xA6) + 长度(0x04) + 命令(0x01) + CO2高字节(D1) + CO2低字节(D2) + Temp高字节(D3) + Temp低字节(D4) + 校验和(CS)
    
    // 计算校验和
    uint8_t calcCS = calculateCheckSum(response, 9); // 校验前9字节
    uint8_t receivedCS = response[9];
    
    if (calcCS != receivedCS) {
        Serial.print("ACD1100 UART: 校验和错误 - 计算值: 0x");
        Serial.print(calcCS, HEX);
        Serial.print(", 接收值: 0x");
        Serial.println(receivedCS, HEX);
        _lastError = ERROR_CRC_MISMATCH;
        return false;
    }
    
    // 解析CO2浓度 (D1 * 256 + D2)
    co2_ppm = ((uint32_t)response[4] << 8) | response[5];
    
    // 解析温度 (D3 * 256 + D4) / 100
    int16_t temp_raw = ((int16_t)response[6] << 8) | response[7];
    temperature = temp_raw / 100.0;
    
    Serial.print("ACD1100 UART: CO2=");
    Serial.print(co2_ppm);
    Serial.print("ppm, 温度=");
    Serial.println(temperature);
    
    _lastCO2 = co2_ppm;
    _lastTemp = temperature;
    _lastError = ERROR_NONE;
    
    return true;
}

// 计算UART校验和
uint8_t ACD1100::calculateCheckSum(uint8_t *data, uint8_t length) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// 发送I2C命令（重命名原函数）
bool ACD1100::sendCommandI2C(uint8_t cmdHigh, uint8_t cmdLow, uint8_t *data, uint8_t dataLen) {
    if (!selectSensorChannel()) {
        return false;
    }
    
    _i2cPort->beginTransmission(ACD1100_I2C_ADDR);
    _i2cPort->write(cmdHigh);
    _i2cPort->write(cmdLow);
    
    if (data != nullptr && dataLen > 0) {
        for (uint8_t i = 0; i < dataLen; i++) {
            _i2cPort->write(data[i]);
        }
    }
    
    return (_i2cPort->endTransmission() == 0);
}

// 读取I2C响应（重命名原函数）
bool ACD1100::readResponseI2C(uint8_t *buffer, uint8_t bufferSize) {
    if (!selectSensorChannel()) {
        return false;
    }
    
    uint8_t bytesRead = _i2cPort->requestFrom(ACD1100_I2C_ADDR, bufferSize);
    
    if (bytesRead == bufferSize) {
        for (uint8_t i = 0; i < bufferSize; i++) {
            buffer[i] = _i2cPort->read();
        }
        return true;
    }
    
    return false;
}

// 发送UART命令
bool ACD1100::sendCommandUART(uint8_t cmd, uint8_t *data, uint8_t dataLen) {
    if (_serialPort == nullptr) {
        return false;
    }
    
    uint8_t length = dataLen + 1; // 命令 + 数据长度
    uint8_t frame[32]; // 足够大的缓冲区
    uint8_t frameLen = 0;
    
    frame[frameLen++] = 0xFE; // 帧头
    frame[frameLen++] = 0xA6; // 固定码
    frame[frameLen++] = length; // 数据长度
    frame[frameLen++] = cmd; // 命令
    
    if (data != nullptr && dataLen > 0) {
        for (uint8_t i = 0; i < dataLen; i++) {
            frame[frameLen++] = data[i];
        }
    }
    
    // 计算校验和
    uint8_t checkSum = calculateCheckSum(&frame[1], frameLen - 1);
    frame[frameLen++] = checkSum;
    
    // 发送帧
    for (uint8_t i = 0; i < frameLen; i++) {
        _serialPort->write(frame[i]);
    }
    
    return true;
}

// 读取UART响应
bool ACD1100::readResponseUART(uint8_t *buffer, uint8_t bufferSize) {
    if (_serialPort == nullptr) {
        return false;
    }
    
    delay(100); // 等待响应
    
    uint8_t bytesRead = 0;
    bool headerFound = false;
    
    // 读取响应
    while (_serialPort->available() && bytesRead < bufferSize) {
        uint8_t byte = _serialPort->read();
        
        if (!headerFound && byte == 0xFE) {
            buffer[bytesRead++] = byte;
            headerFound = true;
        } else if (headerFound) {
            buffer[bytesRead++] = byte;
        }
    }
    
    // 检查校验和
    if (bytesRead >= 4) {
        uint8_t calcCS = calculateCheckSum(&buffer[1], bytesRead - 2);
        if (calcCS == buffer[bytesRead - 1]) {
            return true;
        }
    }
    
    return false;
}
