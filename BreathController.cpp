#include "BreathController.h"

// 常量定义
constexpr int STORE_SIZE = 10;
constexpr int FILTER_WINDOW = 5;
constexpr float EWMA_ALPHA = 0.3;
constexpr int ADAPT_CYCLES = 5;
constexpr unsigned long RECONNECT_INTERVAL = 5000;

BreathController::BreathController(I2CMux* mux) : _mux(mux), acd1100(mux, 4, COMM_I2C), ads1115(nullptr), oxygenSensor(nullptr) {
    // 初始化滤波历史数组
    for (int i = 0; i < FILTER_WINDOW; i++) {
        pressureHistory[i] = NAN;
    }
}

void BreathController::begin() {
    Wire.begin(); // 初始化I2C
    Wire.setClock(100000); // 设置I2C时钟为100kHz，适配ACD1100传感器
    
    // 初始化多路复用器
    if (_mux) {
        _mux->begin();
        // 重置I2C总线确保干净状态
        _mux->resetI2CBus();
        // 扫描I2C设备
        _mux->scanI2CDevices();
        
        // 添加完整的I2C总线扫描
        Serial.println("=== 完整I2C总线扫描 ===");
        scanI2CBus();
    }
    
    // 初始化气阀控制
    pinMode(VALVE_PIN, OUTPUT);
    analogWrite(VALVE_PIN, 0); // 初始关闭气阀
    
    // 初始化传感器
    initSensor();

    // 初始化OLED
    Serial.println("正在初始化OLED...");
    
    // 先测试OLED是否可以通过多路复用器访问
    Serial.println("测试OLED通过多路复用器访问...");
    if (_mux && _mux->selectChannel(2)) {
        Serial.println("成功选择OLED通道2");
        delay(100);
        
        // 测试I2C通信
        Wire.beginTransmission(0x3C);
        uint8_t error = Wire.endTransmission();
        Serial.print("OLED I2C测试结果: ");
        if (error == 0) {
            Serial.println("成功");
        } else {
            Serial.print("失败，错误代码: ");
            Serial.println(error);
        }
    } else {
        Serial.println("无法选择OLED通道2");
    }
    
    oled.setMuxChannel(_mux, 2); // OLED现在在通道2
    if (!oled.begin()) {
        Serial.println("OLED初始化失败! 请检查:");
        Serial.println("1. OLED模块是否正确连接");
        Serial.println("2. I2C地址是否正确 (当前: 0x3C)");
        Serial.println("3. 电源和地线连接");
        Serial.println("4. 多路复用器通道2是否正常工作");
    } else {
        Serial.println("OLED初始化成功!");
        
        // 重置显示确保干净状态
        oled.resetDisplay();
        delay(200);
        
        // 使用稳定化方法
        oled.stabilizeDisplay();
        
        // 使用简单测试
        oled.simpleTest();
        
        // 再次重置为正常显示做准备
        oled.resetDisplay();
        delay(200);
    }
    
    // 探测并记录流量传感器通道
    probeFlowSensor();

    // 初始化气体浓度传感器
    Serial.println("正在初始化ACD1100气体浓度传感器...");
    
    // 根据通信模式初始化
    bool initResult = false;
    if (acd1100.getCommunicationMode() == COMM_UART) {
        // UART模式：传入Serial1或Serial2指针
        initResult = acd1100.begin(Wire, &Serial1);  // 使用Serial1，可以根据需要改为Serial2
    } else {
        // I2C模式：默认使用Wire
        initResult = acd1100.begin();
    }
    
    if (!initResult) {
        Serial.println("ACD1100初始化失败! 请检查:");
        if (acd1100.getCommunicationMode() == COMM_UART) {
            Serial.println("1. UART连接是否正确（TX连接到RX，RX连接到TX）");
            Serial.println("2. 波特率是否正确（1200）");
            Serial.println("3. 传感器电源是否正常");
        } else {
            Serial.println("1. ACD1100模块是否正确连接");
            Serial.println("2. I2C地址是否正确 (当前: 0x2A)");
            Serial.println("3. 多路复用器通道4是否正常工作");
        }
    } else {
        Serial.println("ACD1100初始化成功!");
    }
    
    // 初始化氧传感器（如果已配置）
    if (oxygenSensor != nullptr) {
        oxygenSensor->begin();
        Serial.println("氧传感器初始化完成！");
        Serial.println("提示: 使用calibrateShortCircuit()和calibrateAirEnvironment()进行校准");
    }
    
    // 连接WiFi
    if (_ssid && _password) {
        connectToWiFi();
    }
    
    // 执行初始校准
    //calibrateZeroPoint();
}

void BreathController::setWiFiCredentials(const char* ssid, const char* password, const char* host, int port) {
    _ssid = ssid;
    _password = password;
    _host = host;
    _port = port;
}

// ADS1115和氧传感器配置
void BreathController::setADS1115Channel(uint8_t channel) {
    if (ads1115 != nullptr) {
        delete ads1115;
    }
    if (oxygenSensor != nullptr) {
        delete oxygenSensor;
    }
    
    // 创建ADS1115实例（地址0x4A，使用多路复用器）
    ads1115 = new ADS1115(0x4A, _mux, channel);
    
    Serial.print("ADS1115已配置在I2C多路复用器通道 ");
    Serial.println(channel);
}

// 初始化氧传感器
void BreathController::initializeOxygenSensor() {
    if (ads1115 == nullptr) {
        Serial.println("错误: ADS1115未初始化！");
        return;
    }
    
    // 初始化ADS1115
    if (!ads1115->begin()) {
        Serial.println("ADS1115初始化失败！");
        return;
    }
    
    // 创建氧传感器实例
    if (oxygenSensor != nullptr) {
        delete oxygenSensor;
    }
    oxygenSensor = new OxygenSensor(ads1115, ADS1115_MUX_AIN0_GND);
    oxygenSensor->begin();
    
    Serial.println("氧传感器初始化完成！");
}

void BreathController::update() {
    static unsigned long lastLogTime = 0;
    static unsigned long lastSensorLogTime = 0;
    
    // 如果没有多路复用器，使用默认方式
    if (!_mux) {
        // 原有的单传感器逻辑...
        return;
    }
    
    // 遍历所有启用的多路复用器通道读取传感器数据（跳过OLED通道）
    for (uint8_t i = 0; i < _mux->getChannelCount(); i++) {
        if (_mux->isChannelEnabled(i)) {
            MuxChannelConfig config = _mux->getChannelConfig(i);
            
            // 跳过OLED通道，避免干扰显示
            if (i == 2) { // OLED现在在通道2
                continue;
            }
            
            // 选择当前通道
            if (_mux->selectChannel(i)) {
                // 移除调试输出，减少串口信息
                
                // 启动数据采集
                startAcquisition();
                
                // 等待采集完成
                unsigned long startTime = millis();
                while (!operateCheck() && !dataCheck()) {
                    if (millis() - startTime > 100) {
                        Serial.println("采集超时!");
                        break;
                    }
                    delay(5);
                }
                
                // 根据传感器类型进行不同操作
                if (config.sensorAddr == SENSOR_ADDR) {
                    // 气压传感器
                    int32_t pressure_adc = readPressureADC();
                    int16_t temperature_adc = readTemperatureADC();
                    
                    // 计算k值
                    uint32_t k_value = getKValue(PRESSURE_RANGE);
                    
                    // 转换为实际值
                    float temperature_c = calculateTemperature(temperature_adc);
                    float pressure_kpa = (calculatePressure(pressure_adc, k_value, temperature_c) + 1032) / 12.10111;
                    
                    // 应用双重滤波
                    float filtered_pressure = applyMovingAverage(pressure_kpa);
                    filtered_pressure = applyEWMA(filtered_pressure);
                    
                    // 设置基准值
                    if (!isBaseSet) {
                        basePressure = filtered_pressure;
                        baseTemperature = temperature_c;
                        isBaseSet = true;
                    }
                    
                    // 计算相对于基准值的差值
                    float pressureDiff = filtered_pressure - basePressure;
                    
                    // 存储差值
                    storedPressures[storeIndex] = pressureDiff;
                    storedTemperatures[storeIndex] = temperature_c - baseTemperature;
                    
                    // 呼吸状态检测（使用主气压传感器所在通道：1）
                    if (i == 1) {
                        currentState = detectBreathState(filtered_pressure);
                        
                        // 气阀控制
                        if (assistEnabled) {
                            controlValve();
                        }
                        
                        // 显示信息（降低频率到每500ms一次）
                        if (millis() - lastSensorLogTime > 500) {
                            Serial.print("主传感器 - 压力: ");
                            Serial.print(filtered_pressure, 2);
                            Serial.print("kPa, 温度: ");
                            Serial.print(temperature_c, 1);
                            Serial.print("°C, 状态: ");
                            switch(currentState) {
                                case INHALE: Serial.print("吸气"); break;
                                case EXHALE: Serial.print("呼气"); break;
                                case PEAK: Serial.print("峰值"); break;
                                case TROUGH: Serial.print("谷值"); break;
                            }
                            Serial.println();
                            lastSensorLogTime = millis();
                        }
                        
                        // 自适应调整
                        adaptiveModelAdjustment();
                        
                        // 通过WiFi发送数据
                        if (millis() - lastLogTime > 100 && wifiConnected) {
                            sendDataOverWiFi(filtered_pressure, temperature_c, valveOpening);
                            lastLogTime = millis();
                        }
                    } else if (i == 3) {
                        // 备用传感器输出（降低频率到每500ms一次）
                        static unsigned long lastBackupLogTime = 0;
                        if (millis() - lastBackupLogTime > 500) {
                            Serial.print("备用传感器 - 压力: ");
                            Serial.print(filtered_pressure, 2);
                            Serial.print("kPa, 温度: ");
                            Serial.print(temperature_c, 1);
                            Serial.print("°C, 差值: ");
                            Serial.print(pressureDiff, 3);
                            Serial.println("kPa");
                            lastBackupLogTime = millis();
                        }
                    }
                    
                } else if (config.sensorAddr == FLOW_SENSOR_ADDR) {
                    // 流量传感器（仅在探测到可用时读取）
                    if (flowSensorAvailable && (int)i == flowSensorChannel) {
                        flowRate = readFlowRate();
                        static unsigned long lastFlowLogTime = 0;
                        if (millis() - lastFlowLogTime > 1000) {
                            Serial.print("流量: ");
                            Serial.print(flowRate, 0);
                            Serial.println(" ml/min");
                            lastFlowLogTime = millis();
                        }
                    }
                }
            }
        }
    }
    
    // 更新气体浓度传感器数据
    static unsigned long lastGasLogTime = 0;
    static unsigned long lastDebugTime = 0;
    
    // 每5秒输出一次调试信息
    if (millis() - lastDebugTime > 5000) {
        Serial.print("ACD1100调试 - 连接状态: ");
        Serial.print(acd1100.isConnected() ? "已连接" : "未连接");
        Serial.print(", 错误码: ");
        Serial.println(acd1100.getLastError());
        
        // 如果连接失败，尝试简化测试
        if (!acd1100.isConnected()) {
            Serial.println("ACD1100: 尝试简化测试读取");
            acd1100.testSimpleRead();
        }
        
        lastDebugTime = millis();
    }
    
    if (acd1100.update()) {
        // 每2秒输出一次气体浓度数据
        if (millis() - lastGasLogTime > 2000) {
            Serial.print("ACD1100 - CO2: ");
            Serial.print(acd1100.getFilteredCO2(), 0);
            Serial.print("ppm, 温度: ");
            Serial.print(acd1100.getFilteredTemperature(), 1);
            Serial.print("°C, 空气质量: ");
            Serial.print(acd1100.getAirQuality());
            Serial.println("级");
            lastGasLogTime = millis();
        }
    }
    
    // 读取氧传感器数据
    static unsigned long lastOxygenLogTime = 0;
    if (oxygenSensor != nullptr && oxygenSensor->isCalibrated()) {
        float oxygenPercent = oxygenSensor->readOxygenConcentration();
        if (millis() - lastOxygenLogTime > 2000) {
            Serial.print("氧传感器 - 氧气浓度: ");
            Serial.print(oxygenPercent, 2);
            Serial.println("%");
            lastOxygenLogTime = millis();
        }
    }
    
    // 更新OLED显示（使用第一个通道的数据）
    String stateStr;
    switch(currentState) {
        case INHALE: stateStr = "INHALE"; break;
        case EXHALE: stateStr = "EXHALE"; break;
        case PEAK: stateStr = "PEAK"; break;
        case TROUGH: stateStr = "TROUGH"; break;
    }
    oled.update(filteredPressure, baseTemperature, stateStr, (valveOpening/MAX_VALVE_OPEN)*100, flowRate);
    
    // 移动到下一个存储位置
    storeIndex = (storeIndex + 1) % STORE_SIZE;
    
    delay(100); // 每100ms读取一次，提高读取速度
}

void BreathController::probeFlowSensor() {
    flowSensorAvailable = false;
    flowSensorChannel = -1;
    if (!_mux) return;
    
    for (uint8_t i = 0; i < _mux->getChannelCount(); i++) {
        if (!_mux->isChannelEnabled(i)) continue;
        MuxChannelConfig config = _mux->getChannelConfig(i);
        if (config.sensorAddr != FLOW_SENSOR_ADDR) continue;
        if (!_mux->selectChannel(i)) continue;
        
        // 简单探测：请求2字节，若能返回>=2则认为存在
        Wire.requestFrom((int)config.sensorAddr, (int)2);
        if (Wire.available() >= 2) {
            flowSensorAvailable = true;
            flowSensorChannel = (int)i;
            Serial.print("检测到流量传感器于通道 ");
            Serial.println(flowSensorChannel);
            // 读取并丢弃探测字节
            while (Wire.available()) (void)Wire.read();
            break;
        }
        while (Wire.available()) (void)Wire.read();
    }
    if (!flowSensorAvailable) {
        Serial.println("未检测到流量传感器");
    }
}

void BreathController::scanI2CBus() {
    Serial.println("扫描I2C总线上的所有设备...");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("找到I2C设备，地址: 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.print(" (");
            Serial.print(address);
            Serial.println(")");
            deviceCount++;
        } else if (error == 4) {
            Serial.print("地址 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.println(" 未知错误");
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("未找到任何I2C设备！");
    } else {
        Serial.print("总共找到 ");
        Serial.print(deviceCount);
        Serial.println(" 个I2C设备");
    }
    Serial.println("=== I2C扫描完成 ===");
    
    // 测试绕过多路复用器直接连接OLED
    Serial.println("=== 测试绕过多路复用器直接连接OLED ===");
    if (_mux) {
        _mux->disableAllChannels(); // 禁用所有多路复用器通道
        delay(100);
        
        Serial.println("多路复用器已禁用，测试直接I2C连接...");
        Wire.beginTransmission(0x3C);
        uint8_t error = Wire.endTransmission();
        Serial.print("直接连接OLED测试结果: ");
        if (error == 0) {
            Serial.println("成功 - OLED可以直接访问");
        } else {
            Serial.print("失败，错误代码: ");
            Serial.println(error);
            Serial.println("OLED可能没有直接连接到I2C总线");
        }
    }
}

// 以下为私有方法的实现
void BreathController::initSensor() {
    if (!_mux) return;
    for (uint8_t i = 0; i < _mux->getChannelCount(); i++) {
        if (_mux->isChannelEnabled(i)) {
            MuxChannelConfig config = _mux->getChannelConfig(i);
            if (config.sensorAddr == SENSOR_ADDR) {
                if (_mux->selectChannel(i)) {
                    uint8_t special_val = readRegister(REG_SPECIAL);
                    writeRegister(REG_SPECIAL, special_val & CMD_CLEAR);
                    delay(10);
                }
            }
        }
    }
}

uint32_t BreathController::getKValue(float range_kpa) {
    if (range_kpa > 1000) return 4;
    else if (range_kpa > 500) return 8;
    else if (range_kpa > 260) return 16;  
    else if (range_kpa > 131) return 32;
    else if (range_kpa > 65) return 64;
    else if (range_kpa > 32) return 128;
    else if (range_kpa > 16) return 256;
    else if (range_kpa > 8) return 512;
    else if (range_kpa > 4) return 1024;
    else if (range_kpa > 2) return 2048;
    else if (range_kpa > 1) return 4096;
    else return 8192;
}

void BreathController::writeRegister(uint8_t reg, uint8_t value) {
    if (!_mux) return;
    
    uint8_t currentSensorAddr = _mux->getChannelConfig(_mux->getActiveChannel()).sensorAddr;
    
    Wire.beginTransmission(currentSensorAddr);
    Wire.write(reg);
    Wire.write(value);
    if (Wire.endTransmission() != 0) {
        Serial.print("I2C写入失败 @ 通道 ");
        Serial.print(_mux->getActiveChannel());
        Serial.print(", 寄存器 0x");
        Serial.println(reg, HEX);
    }
}

uint8_t BreathController::readRegister(uint8_t reg) {
    if (!_mux) return 0;
    
    uint8_t currentSensorAddr = _mux->getChannelConfig(_mux->getActiveChannel()).sensorAddr;
    
    Wire.beginTransmission(currentSensorAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        Serial.print("I2C寻址失败 @ 通道 ");
        Serial.print(_mux->getActiveChannel());
        Serial.print(", 寄存器 0x");
        Serial.println(reg, HEX);
        return 0;
    }
    
    uint8_t bytes = Wire.requestFrom((int)currentSensorAddr, (int)1);
    if (bytes == 1) {
        return Wire.read();
    }
    Serial.print("读取失败, 通道 ");
    Serial.print(_mux->getActiveChannel());
    Serial.print(", 收到");
    Serial.print(bytes);
    Serial.println("字节");
    return 0;
}

float BreathController::readFlowRate() {
    if (!_mux) return -1.0f;
    
    uint8_t currentSensorAddr = _mux->getChannelConfig(_mux->getActiveChannel()).sensorAddr;
    
    Wire.requestFrom((int)currentSensorAddr, (int)2);
    if (Wire.available() >= 2) {
        uint8_t highByte = Wire.read();
        uint8_t lowByte = Wire.read();
        uint16_t rawValue = (highByte << 8) | lowByte;
        
        float flow_lpm = rawValue / 100.0f;
        return flow_lpm * 1000.0f;
    }
    return -1.0f;
}

// ... 其余方法保持不变
int32_t BreathController::readPressureADC() {
    uint8_t msb = readRegister(REG_DATA_MSB);
    uint8_t csb = readRegister(REG_DATA_CSB);
    uint8_t lsb = readRegister(REG_DATA_LSB);
    return ((uint32_t)msb << 16) | ((uint32_t)csb << 8) | lsb;
}

int16_t BreathController::readTemperatureADC() {
    uint8_t msb = readRegister(REG_TEMP_MSB);
    uint8_t lsb = readRegister(REG_TEMP_LSB);
    return ((uint16_t)msb << 8) | lsb;
}

bool BreathController::dataCheck() {
    uint8_t datacheck = readRegister(REG_STATUS);
    return (datacheck & 0x01);
}

bool BreathController::operateCheck() {
    uint8_t operatecheck = readRegister(REG_CMD);
    return !(operatecheck & 0x08);
}

void BreathController::startAcquisition() {
    writeRegister(REG_CMD, CMD_COLLECT);
}

float BreathController::calculateTemperature(uint16_t adc_value) {
    if (adc_value & 0x8000) {
        return (adc_value - 65536.0) / 256.0;
    } else {
        return adc_value / 256.0;
    }
}

float BreathController::calculatePressure(uint32_t adc_value, uint32_t k, float temperature) {
    float pressure;
    if(k==0) k=16;
    if (adc_value & 0x800000) {
        pressure = (adc_value - 16777216.0) / k;
    } else {
        pressure = adc_value / (float)k;
    }
    
    //const float TEMP_COMP_COEF = 0.01;
    //pressure = pressure * (1.0 + TEMP_COMP_COEF * (25 - temperature));
    
    return pressure;
}

float BreathController::applyMovingAverage(float newValue) {
    pressureHistory[historyIndex] = newValue;
    historyIndex = (historyIndex + 1) % FILTER_WINDOW;
    
    float sum = 0.0;
    int count = 0;
    for (int i = 0; i < FILTER_WINDOW; i++) {
        if (!isnan(pressureHistory[i])) {
            sum += pressureHistory[i];
            count++;
        }
    }
    
    return (count > 0) ? (sum / count) : newValue;
}

float BreathController::applyEWMA(float newValue) {
    if (!isFilterInitialized) {
        filteredPressure = newValue;
        isFilterInitialized = true;
        return newValue;
    }
    
    filteredPressure = EWMA_ALPHA * newValue + (1 - EWMA_ALPHA) * filteredPressure;
    return filteredPressure;
}

void BreathController::calibrateZeroPoint() {
    const int CALIB_SAMPLES = 10;
    float sum = 0.0;
    
    Serial.println("\n开始零点校准...");
    
    for (int i = 0; i < CALIB_SAMPLES; i++) {
        startAcquisition();
        unsigned long startTime = millis();
        while (!operateCheck() && !dataCheck()) {
            if (millis() - startTime > 100) {
                Serial.println("校准采集超时!");
                return;
            }
            delay(5);
        }
        
        int32_t pressure_adc = readPressureADC();
        uint32_t k_value = getKValue(PRESSURE_RANGE);
        float pressure = calculatePressure(pressure_adc, k_value, baseTemperature);
        sum += pressure;
        
       Serial.print(".");
        delay(100);
    }
    
    basePressure = sum / CALIB_SAMPLES;
    Serial.println("\n零点校准完成!");
    Serial.print("新基准压力: ");
    Serial.print(basePressure, 4);
    Serial.println(" kPa");
    Serial.println("---------------------");
}

BreathState BreathController::detectBreathState(float pressure) {
    static float lastPressure = NAN;
    static bool firstCall = true;
    
    BreathState newState = currentState;
    
    if (firstCall || isnan(lastPressure)) {
        lastPressure = pressure;
        firstCall = false;
        return currentState;
    }

    switch(currentState) {
        case EXHALE:
            if (pressure > lastPressure + pressureThreshold) {
                newState = INHALE;
                minPressure = pressure;
            }
            break;
            
        case INHALE:
            if (pressure < lastPressure) {
                newState = PEAK;
                maxPressure = pressure;
            }
            break;
            
        case PEAK:
            if (pressure < lastPressure - pressureThreshold) {
                newState = EXHALE;
                unsigned long currentTime = millis();
                if (lastBreathTime > 0) {
                    breathPeriod = 0.8 * breathPeriod + 0.2 * (currentTime - lastBreathTime);
                }
                lastBreathTime = currentTime;
                breathCount++;
            }
            break;
            
        case TROUGH:
            if (pressure > lastPressure) {
                newState = INHALE;
            }
            break;
    }
    
    lastPressure = pressure;
    return newState;
}

void BreathController::controlValve() {
    switch(currentState) {
        case INHALE:
            valveOpening = constrain(valveOpening + 10 * responseFactor, 0, MAX_VALVE_OPEN * assistLevel);
            break;
            
        case PEAK:
            break;
            
        case EXHALE:
            valveOpening = constrain(valveOpening - 20, 0, MAX_VALVE_OPEN);
            break;
            
        case TROUGH:
            valveOpening = 0;
            break;
    }
    
    analogWrite(VALVE_PIN, (int)valveOpening); 
}

void BreathController::adaptiveModelAdjustment() {
    if (breathCount % ADAPT_CYCLES == 0) {
        float avgPressureDiff = 0;
        for (int i = 0; i < STORE_SIZE; i++) {
            avgPressureDiff += fabs(storedPressures[i]);
        }
        avgPressureDiff /= STORE_SIZE;
        
        if (avgPressureDiff > 1.5 * pressureThreshold) {
            pressureThreshold *= 1.1;
            responseFactor *= 1.05;
            Serial.println("模型调整: 增加灵敏度");
        } 
        else if (avgPressureDiff < 0.7 * pressureThreshold) {
            pressureThreshold *= 0.9;
            responseFactor *= 0.95;
            Serial.println("模型调整: 降低灵敏度");
        }
        
        pressureThreshold = constrain(pressureThreshold, 0.2, 2.0);
        responseFactor = constrain(responseFactor, 0.5, 2.0);
        
        Serial.print("新阈值: ");
        Serial.print(pressureThreshold, 2);
        Serial.print(" kPa, 响应因子: ");
        Serial.println(responseFactor, 2);
    }
}

void BreathController::connectToWiFi() {
    if (!_ssid || !_password) return;
    
    Serial.println();
    Serial.print("正在连接到: ");
    Serial.println(_ssid);

    WiFi.begin(_ssid, _password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("");
        Serial.println("WiFi已连接!");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
    } else {
        wifiConnected = false;
        Serial.println("");
        Serial.println("WiFi连接失败!");
    }
}

bool BreathController::connectToServer() {
    if (!_host || !_port) return false;
    
    // 检查WiFi连接状态
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi未连接，无法连接服务器");
        wifiConnected = false;
        return false;
    }
    
    Serial.print("正在连接到服务器: ");
    Serial.print(_host);
    Serial.print(":");
    Serial.println(_port);
    
    // 设置连接超时
    client.setTimeout(5000);
    
    if (client.connect(_host, _port)) {
        Serial.println("服务器连接成功!");
        Serial.print("本地IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("目标IP: ");
        Serial.println(_host);
        return true;
    } else {
        Serial.println("服务器连接失败!");
        Serial.print("错误代码: ");
        Serial.println(client.getWriteError());
        Serial.println("可能的原因:");
        Serial.println("1. 服务器未运行");
        Serial.println("2. IP地址错误");
        Serial.println("3. 端口被防火墙阻止");
        Serial.println("4. 网络不通");
        return false;
    }
}

void BreathController::sendDataOverWiFi(float pressure, float temp, float valve) {
    if (!client.connected()) {
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
            lastReconnectAttempt = millis();
            if (!connectToServer()) {
                return;
            }
        } else {
            return;
        }
    }
    
    String data = String(millis()) + ",";
    data += String(pressure, 4) + ",";
    data += String(temp, 2) + ",";
    data += String(valve/MAX_VALVE_OPEN, 2) + ",";
    
    switch(currentState) {
        case INHALE: data += "INHALE"; break;
        case EXHALE: data += "EXHALE"; break;
        case PEAK: data += "PEAK"; break;
        case TROUGH: data += "TROUGH"; break;
    }
    
    client.println(data);
}

// 设置ACD1100通信模式
void BreathController::setACD1100CommunicationMode(ACD1100_COMM_MODE mode) {
    acd1100.setCommunicationMode(mode);
    Serial.print("ACD1100通信模式已切换为: ");
    Serial.println(mode == COMM_I2C ? "I2C" : "UART");
}
