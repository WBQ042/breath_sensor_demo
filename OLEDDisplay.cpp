#include "OLEDDisplay.h"

OLEDDisplay::OLEDDisplay(I2CMux* mux, uint8_t channel) 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire), _mux(mux), _channel(channel) {}

void OLEDDisplay::setMuxChannel(I2CMux* mux, uint8_t channel) {
    _mux = mux;
    _channel = channel;
}

void OLEDDisplay::selectDisplayChannel() {
    if (_mux) {
        if (!_mux->selectChannel(_channel)) {
            Serial.print("OLED通道选择失败: ");
            Serial.println(_channel);
        }
    }
}

bool OLEDDisplay::begin() {
    Serial.print("开始初始化OLED，通道: ");
    Serial.println(_channel);
    
    selectDisplayChannel(); // 选择OLED所在通道
    delay(100); // 增加延迟时间确保通道稳定
    
    Serial.print("尝试连接OLED，地址: 0x");
    Serial.println(OLED_ADDR, HEX);
    
    // 检查I2C设备是否存在
    Wire.beginTransmission(OLED_ADDR);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.print("OLED I2C通信失败，错误代码: ");
        Serial.println(error);
        return false;
    }
    Serial.println("OLED I2C通信正常");
    
    // 使用更保守的初始化方法
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED display.begin() 失败");
        return false;
    }
    Serial.println("OLED display.begin() 成功");
    
    // 多次重置显示，确保完全干净
    Serial.println("重置显示缓冲区...");
    for (int i = 0; i < 3; i++) {
        display.clearDisplay();
        display.display();
        delay(100);
    }
    
    // 设置基本显示参数
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    // 显示初始化信息
    Serial.println("显示初始化信息...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Initializing...");
    display.display();
    delay(1000); // 增加显示时间
    
    // 最终清除并显示空白屏幕
    display.clearDisplay();
    display.display();
    delay(100);
    
    Serial.println("OLED初始化完成");
    return true;
}

void OLEDDisplay::testDisplay() {
    selectDisplayChannel(); // 选择OLED所在通道
    
    // 添加延迟确保通道切换完成
    delay(5);
    
    // 完全清除显示缓冲区
    display.clearDisplay();
    display.display();
    delay(10);
    
    // 重新设置显示参数
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // 显示测试信息
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.print("OLED TEST");
    
    display.setCursor(0, 20);
    display.setTextSize(1);
    display.print("If you can see this,");
    
    display.setCursor(0, 30);
    display.print("OLED is working!");
    
    display.setCursor(0, 45);
    display.print("Channel: ");
    display.print(_channel);
    
    display.setCursor(0, 55);
    display.print("Time: ");
    display.print(millis() / 1000);
    display.print("s");
    
    // 确保显示更新
    display.display();
    delay(10);
    
    Serial.println("OLED测试显示完成");
}

void OLEDDisplay::update(float pressure, float temperature, const String& state, float valvePercent, float flow) {
    if (millis() - lastUpdate < 500) return; // 减少刷新间隔到500ms
    lastUpdate = millis();
    
    // 选择OLED通道并等待稳定
    selectDisplayChannel();
    delay(20); // 减少延迟提高响应速度
    
    // 完全清除显示缓冲区
    display.clearDisplay();
    display.display(); // 先显示空白屏幕
    delay(10);
    
    // 重新设置显示参数
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // 第一行：标题和通道信息
    display.setCursor(0, 0);
    display.print("Breath Monitor");
    if (_mux) {
        display.print(" CH");
        display.print(_channel);
    }
    
    // 第二行：气压
    display.setCursor(0, 16);
    display.print("Pressure: ");
    display.print(pressure, 2);
    display.print(" kPa");
    
    // 第三行：温度
    display.setCursor(0, 26);
    display.print("Temp: ");
    display.print(temperature, 1);
    display.print(" C");
    
    // 第四行：流量显示 
    display.setCursor(0, 36);
    display.print("Flow: ");
    display.print(flow, 0);
    display.print("ml/min");
    
    // 第五行：阀门
    display.setCursor(0, 46);
    display.print("Valve: ");
    display.print(valvePercent, 0);
    display.print("%");
    
    // 第六行：状态
    display.setCursor(0, 56);
    display.print("State: ");
    display.print(state);
    
    // 确保显示更新
    display.display();
    delay(10); // 减少延迟提高响应速度
}

void OLEDDisplay::clearGraphs() {
    // 简化版本不需要清除图形
}

void OLEDDisplay::resetDisplay() {
    selectDisplayChannel();
    delay(5);
    
    // 完全重置显示
    display.clearDisplay();
    display.display();
    delay(10);
    
    // 重新初始化显示参数
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    Serial.println("OLED显示已重置");
}

void OLEDDisplay::simpleTest() {
    Serial.println("开始OLED文字测试...");
    
    selectDisplayChannel();
    delay(100); // 增加延迟确保通道稳定
    
    // 完全重置显示
    display.clearDisplay();
    display.display();
    delay(200);
    
    // 只进行文字测试
    Serial.println("显示测试文字");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("OLED TEST");
    display.setCursor(20, 35);
    display.print("Channel: ");
    display.print(_channel);
    display.display();
    delay(2000);
    
    Serial.println("OLED文字测试完成");
}

void OLEDDisplay::stabilizeDisplay() {
    Serial.println("稳定化OLED显示...");
    
    selectDisplayChannel();
    delay(200); // 长时间延迟确保通道稳定
    
    // 多次清除和重置
    for (int i = 0; i < 3; i++) {
        display.clearDisplay();
        display.display();
        delay(100);
    }
    
    // 重新初始化显示参数
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    // 显示稳定化信息
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print("Display");
    display.setCursor(10, 35);
    display.print("Stabilized");
    display.display();
    delay(1000);
    
    // 最终清除
    display.clearDisplay();
    display.display();
    
    Serial.println("OLED显示稳定化完成");
}