#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "time.h"

//----------------------------------- Time variables

const char* ssid = "myNET";
const char* password = "Li#lu~2014";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800;  // GMT+3
const int daylightOffset_sec = 0;

bool firstTimeRead = true; // flag for fitst void loop iteration
//----------------------------------- Init

Adafruit_BME280 bme;
TFT_eSPI tft = TFT_eSPI();

//----------------------------------- Sensors

struct sensorData {
    float temp;
    float hum;
    float press;
};

unsigned long lastSensorRead = 0;
const int sensorInterval = 10000;
bool firstSensorsRead = true; // flag for fitst void loop iteration

sensorData readSensors() {
    sensorData data;

    data.temp = bme.readTemperature();
    data.hum = bme.readHumidity();
    data.press = bme.readPressure() / 133.322F;

    return data;
}

//----------------------------------- UI Draw

void drawValue(float value, int y, const char* unit, uint16_t color) {
    tft.setTextSize(3);
    tft.setCursor(50, y);
    tft.print(value, 1);
    tft.setTextColor(color, TFT_BLACK);
    tft.print(unit);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

//----------------------------------- Setup

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Booting...");

  // BME
  if (bme.begin(0x76)) {
    tft.setTextColor(TFT_GREEN); tft.println("BME280 OK");
  } else {
    tft.setTextColor(TFT_RED);
    tft.println("BME280 FAIL");
  }
  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  tft.setTextColor(TFT_BLUE);
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    tft.print(".");
    delay(500);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN);
    tft.println("\nWiFi OK");
  } else {
    tft.setTextColor(TFT_RED);
    tft.println("\nWiFi FAIL");
  }
  // Time sync
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo, 3000)) {
    tft.setTextColor(TFT_GREEN);
    tft.println("Time OK");
  } else {
    tft.setTextColor(TFT_RED);
    tft.println("Time FAIL");
  }

  tft.setTextColor(TFT_BLUE);
  tft.print("Free RAM: ");
  tft.println(ESP.getFreeHeap());

  delay(2000);

  tft.fillScreen(TFT_BLACK);
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  //------------------- Time update

  if(firstTimeRead || millis() - lastUpdate >= 30000) {
    firstTimeRead = false;
    lastUpdate = millis();
    
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      tft.setTextSize(5);
      tft.setCursor(80, 10);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print(&timeinfo, "%H:%M");
    }
  }

  //------------------- Values update

  if (firstSensorsRead || now - lastSensorRead >= sensorInterval) {
    firstSensorsRead = false;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    sensorData readings = readSensors();
    lastSensorRead = now;

    drawValue(readings.temp, 50, " C", TFT_RED);
    drawValue(readings.hum, 75, " %", TFT_BLUE);
    drawValue(readings.press, 100, " mmHg", TFT_GREEN);
  }
}