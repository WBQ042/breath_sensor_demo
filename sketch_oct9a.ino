/*
 * 医用呼吸机边缘控制系统 - 主程序
 * 
 * 已集成传感器：
 * - 气压传感器（I2C多路复用器）
 * - 流量传感器（I2C多路复用器）
 * - OLED显示屏（I2C多路复用器）
 * - ACD1100 CO2传感器（I2C多路复用器或UART）
 * - 电化学氧传感器（ADC直接读取，GPIO34）
 * 
 * ACD1100通信模式切换：
 * - 默认使用I2C模式（连接到多路复用器通道4）
 * - 如需使用UART模式，取消注释第135行的setACD1100CommunicationMode调用
 * - UART模式需要将传感器连接到ESP32的Serial1或Serial2
 * 
 * 关于串口波特率的说明：
 * - 串口监视器始终使用115200波特率（不需要改！）
 * - ACD1100的UART通信使用1200波特率（通过Serial1/Serial2）
 * - 这两者是完全独立的，不会互相干扰
 * - Serial.print()调试输出通过Serial（115200）发送给电脑
 * - 传感器数据通过Serial1/Serial2（1200）与ACD1100通信
 * 
 * 氧传感器使用说明：
 * 1. 使用ADS1115 16位ADC读取（I2C地址0x4A）
 * 2. ADS1115通过I2C多路复用器通道5连接
 * 3. 需要先校准：调用calibrateShortCircuit()和calibrateAirEnvironment()
 * 4. 校准后传感器将自动读取并输出氧气浓度
 * 
 * 连接方法：
 * - 传感器正极(Vsensor+) -> ADS1115 AIN0
 * - 传感器负极(Vsensor-) -> ADS1115 GND
 * - ADS1115通过I2C多路复用器连接
 * 
 * 推荐使用方法：
 * 在void loop()之前添加校准函数调用
 */
#include <Arduino.h>
#include "BreathController.h"
#include "gas_concentration.h"
#include "I2CMux.h"

// WiFi配置
const char* ssid = "Pressure_Breath";       // WiFi名称
const char* password = "pressure";          // WiFi密码
const char* host = "10.181.245.186";        // 接收数据的电脑IP
const int port = 8080;                      // 接收数据的端口

// 创建多路复用器实例
I2CMux i2cMux(0x70); // TCA9548地址为0x70
// 创建呼吸控制器，传入多路复用器
BreathController breathController(&i2cMux);

void setup() {
    Serial.begin(115200);
    while (!Serial); // 等待串口连接
    
    Serial.println("\n医用呼吸机边缘控制系统启动");
    Serial.println("=== WiFi连接诊断工具 ===");
    
    // 配置ADS1115和氧传感器（使用I2C多路复用器通道5）
    // ADS1115地址为0x4A
    breathController.setADS1115Channel(5);
    
    // 配置多路复用器通道
    i2cMux.addChannel(0, 0x50, "流量传感器");     // 流量传感器在通道0
    i2cMux.addChannel(1, 0x6D, "SENSOR");     // 主气压传感器在通道1
    i2cMux.addChannel(2, 0x3C, "OLED Display");       // OLED在通道2
    i2cMux.addChannel(3, 0x6D, "备用气压传感器");   // 备用气压传感器在通道3
    i2cMux.addChannel(4, 0x2A, "ACD1100气体传感器"); // ACD1100在通道4
    i2cMux.addChannel(5, 0x4A, "ADS1115 ADC");    // ADS1115在通道5
    
    // 启用需要的通道
    i2cMux.enableChannel(0, false);  // 流量传感器
    i2cMux.enableChannel(1, true);  // 主气压传感器
    i2cMux.enableChannel(2, true);  // OLED
    i2cMux.enableChannel(3, true);  // 启用备用气压传感器
    i2cMux.enableChannel(4, true);  // 启用ACD1100气体传感器
    i2cMux.enableChannel(5, true);  // 启用ADS1115 ADC
    
    // 打印通道信息
    i2cMux.printChannelInfo();
    
    // 测试ACD1100通道
    Serial.println("\n=== ACD1100通道测试 ===");
    Serial.println("测试通道4上的ACD1100...");
    
    if (i2cMux.selectChannel(4)) {
        Serial.println("通道4选择成功");
        
        // 测试I2C通信
        Wire.beginTransmission(0x2A);  // 7位地址，Arduino自动处理8位转换
        uint8_t result = Wire.endTransmission();
        
        Serial.print("传感器地址0x2A测试结果: ");
        Serial.println(result);
        
        if (result == 0) {
            Serial.println("ACD1100在通道4上响应正常！");
        } else {
            Serial.println("ACD1100在通道4上无响应");
            
            // 扫描通道4上的所有I2C设备
            Serial.println("扫描通道4上的I2C设备...");
            int deviceCount = 0;
            for (uint8_t addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                uint8_t error = Wire.endTransmission();
                if (error == 0) {
                    Serial.print("找到设备，地址: 0x");
                    if (addr < 16) Serial.print("0");
                    Serial.print(addr, HEX);
                    Serial.print(" (");
                    Serial.print(addr);
                    Serial.println(")");
                    deviceCount++;
                }
            }
            if (deviceCount == 0) {
                Serial.println("通道4上未找到任何I2C设备");
            }
        }
    } else {
        Serial.println("通道4选择失败");
    }
    Serial.println("=== ACD1100测试完成 ===\n");
    
    // 设置WiFi凭据
    breathController.setWiFiCredentials(ssid, password, host, port);
    
    // 打印WiFi配置信息
    Serial.println("\n=== WiFi配置信息 ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("目标IP: ");
    Serial.println(host);
    Serial.print("端口: ");
    Serial.println(port);
    
    // 如果要使用UART模式，请在下面取消注释：
    // 1. 先设置通信模式
    // breathController.setACD1100CommunicationMode(COMM_UART);
    // 2. 然后初始化对应的串口
    // Serial1.begin(1200);  // 使用Serial1，波特率1200
    // 或者使用Serial2: Serial2.begin(1200);
    
    // 初始化气压、温度以及控制器
    breathController.begin();
    
    // 初始化氧传感器
    Serial.println("\n=== 初始化氧传感器 ===");
    breathController.initializeOxygenSensor();
    
    Serial.println("\n=== 系统初始化完成 ===");
    Serial.println("开始主循环...");
    Serial.println("ACD1100当前通信模式: I2C");
    Serial.println("如果WiFi连接有问题，请检查:");
    Serial.println("1. WiFi密码是否正确");
    Serial.println("2. 电脑IP地址是否还是 " + String(host));
    Serial.println("3. Python服务器是否在运行");
    Serial.println("4. 防火墙是否阻止了8080端口");
    Serial.println("========================\n");
}

void loop() {
    // 更新气压、温度以及控制器状态（包含ACD1100）
    breathController.update();
}