#ifndef OLEDDisplay_h
#define OLEDDisplay_h

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "I2CMux.h"  // 包含多路复用器库

// OLED 配置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

class OLEDDisplay {
public:
    OLEDDisplay(I2CMux* mux = nullptr, uint8_t channel = 0); // 可传入多路复用器和通道
    bool begin();
    void update(float pressure, float temperature, const String& state, float valvePercent, float flow);
    void clearGraphs();
    void testDisplay();
    void resetDisplay();
    void simpleTest();
    void stabilizeDisplay();

    // 多路复用器设置
    void setMuxChannel(I2CMux* mux, uint8_t channel);

private:
    Adafruit_SSD1306 display;
    unsigned long lastUpdate = 0;
    I2CMux* _mux;
    uint8_t _channel;
    
    void selectDisplayChannel();
};

#endif