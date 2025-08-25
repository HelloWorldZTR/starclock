#include <Arduino.h>
#include <string>
#include <time.h>
#include <FS.h> 
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <WiFiManager.h>
#include <DS3231.h>
#include "pics.h"

using fs::FS;

// === 舵机 ===
const int servoPin = 13;
// Servo myServo;

// === ST7789 显示屏 ===
// 定义SPI引脚
// const int st7789_scl_pin = 12;  // SPI CLK
// const int st7789_sda_pin = 11;  // SPI MOSI
// const int st7789_rst_pin = 10;  // 复位
// const int st7789_dc_pin  = 9;   // 数据/命令选择
// const int st7789_cs_pin  = 0;   // 片选 (如果接GND，可以忽略)
// const int st7789_bl_pin  = 8;   // 背光控制
TFT_eSPI tft = TFT_eSPI();

// === DS1302 ===
const int I2C_SCL = 6;  // I2C SCL
const int I2C_SDA = 7;  // I2C SDA
DS3231 myRTC;

// === 颜色设置 ===
uint16_t font_color = TFT_BLACK;

// === 时间刷新变量 ===
unsigned long lastUpdate = 0;  // 上次更新时间
const unsigned long updateInterval = 30 * 1000; // 每30秒刷新一次

// === UI 常量 ===
const char* dayNames[] = {
    "Sun.", "Mon.", "Tue.", "Wed.", "Thu.", "Fri.", "Sat."
};
const char* ntpServer = "ntp.ntsc.ac.cn"; // 中国科学院国家授时中心
const long  gmtOffset_sec = 8 * 3600;     // 东八区
const int   daylightOffset_sec = 0;       // 无夏令时
const char* holidayServer = "https://api.jiejiariapi.com/v1/holidays/"; // + year
const char* weatherServer = "https://api.seniverse.com/v3/weather/now.json?key"; // + APIKEY

// === 变量 ===
String status;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL, 100000); // Can;t handle
  // ===== 初始化 TFT 屏幕 =====
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextSize(2);
  tft.setTextColor(font_color);
  tft.setSwapBytes(true);

  // ===== WiFiManager 自动联网 =====
  drawGuideUI("Connect via WiFi.");
  WiFiManager wm;
  
  if (!wm.autoConnect("StarClock")) {
    Serial.println("连接失败，重置设置...");
    wm.resetSettings();
    drawGuideUI("Connect failed. Please reset.");
  }
  drawGuideUI("WiFi connected.");
  Serial.println("WiFi 已连接: " + WiFi.localIP().toString());

  drawGuideUI("Syncing time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("等待同步时间");
    delay(1000);
  }

  // ===== 同步 DS3231 =====
  setTimeRTC(timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday,
           timeinfo.tm_wday + 1,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);
  char dateStr[30];
  snprintf(dateStr, sizeof(dateStr), "%s %04d-%02d-%02d %02d:%02d:%02d",
            dayNames[timeinfo.tm_wday],
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
  Serial.println(dateStr);

  drawGuideUI("Completed!");
  Serial.println("初始化完成");
}

void setTimeRTC(uint16_t yy, uint16_t mm, uint16_t dd, uint16_t dow, uint16_t h, uint16_t m, uint16_t s) {
  myRTC.setClockMode(false); 
  myRTC.setYear(yy);
  myRTC.setMonth(mm);
  myRTC.setDate(dd);
  myRTC.setDoW(dow);
  myRTC.setHour(h);
  myRTC.setMinute(m);
  myRTC.setSecond(s);
}

void drawUI() {
  // === 设置字体 ===
  tft.setTextFont(4);
  tft.setTextSize(2);
  tft.setTextColor(font_color);

  // === 获取当前时间 ===
  bool h12Flag;
  bool pmFlag;
  bool century = false;
  uint16_t year = myRTC.getYear();
  uint16_t month = myRTC.getMonth(century);
  uint16_t date = myRTC.getDate();
  uint16_t dOW = myRTC.getDoW();
  uint16_t hour = myRTC.getHour(h12Flag, pmFlag); // 24 hour
  uint16_t minute = myRTC.getMinute();
  uint16_t second = myRTC.getSecond();

  // === 绘制第一行日期 ===
  tft.fillRectVGradient(0, 0, 240, 30, TFT_RED, TFT_ORANGE);
  tft.fillRectVGradient(0, 30, 240, 35, TFT_ORANGE, TFT_RED);
  tft.setCursor(30, 5);
  char dateStr[20];
  snprintf(dateStr, sizeof(dateStr), "%s %d", dayNames[dOW-1], date);
  tft.print(dateStr);

  // === 绘制天气和季节图标 ===
  tft.pushImage(2, 67, 80, 60, StatusSun);
  if (month >= 3 && month <= 5) {
    tft.pushImage(150, 67, 80, 60, Spring);
  } else if (month >= 6 && month <= 8) {
    tft.pushImage(150, 67, 80, 60, Summer);
  } else if (month >= 9 && month <= 11) {
    tft.pushImage(150, 67, 80, 60, Fall);
  } else {
    tft.pushImage(150, 67, 80, 60, Winter);
  }

  // === 绘制第二行时间 ===
  tft.fillRectVGradient(0, 135, 240, 30, TFT_RED, TFT_ORANGE);
  tft.fillRectVGradient(0, 165, 240, 35, TFT_ORANGE, TFT_RED);
  tft.setCursor(30, 140);

  char timeStr[20];
  if (hour > 12) {
    hour = hour - 12;
    snprintf(timeStr, sizeof(timeStr), "%d:%02d %s", hour, minute, "pm");
  }
  else {
    snprintf(timeStr, sizeof(timeStr), "%d:%02d %s", hour, minute, "am");
  }
  tft.print(timeStr);
}

void drawGuideUI(String help_msg) {
  // === 设置字体 ===
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextColor(font_color);

  // === 绘制第一行信息 ===
  tft.fillRectVGradient(0, 0, 240, 30, TFT_RED, TFT_ORANGE);
  tft.fillRectVGradient(0, 30, 240, 35, TFT_ORANGE, TFT_RED);
  tft.setCursor(10, 10);
  tft.print("Star Clock");

  // === 绘制天气和季节图标 ===
  tft.pushImage(2, 67, 80, 60, StatusSun);
  tft.pushImage(150, 67, 80, 60, Spring);

  // === 绘制第二行信息 ===
  tft.fillRectVGradient(0, 135, 240, 30, TFT_RED, TFT_ORANGE);
  tft.fillRectVGradient(0, 165, 240, 35, TFT_ORANGE, TFT_RED);
  tft.setCursor(10, 140);
  tft.print(help_msg);
}

void loop() {
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    drawUI();
  }
}
