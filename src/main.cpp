#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SGP30.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "time.h"
#include "LittleFSHelper.h"

using fs::File; // For LittleFS

//----------------------------------- Time variables

const char* ssid = "myNET";
const char* password = "Li#lu~2014";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800;  // GMT+3
const int daylightOffset_sec = 0;

bool firstTimeRead = true; // flag for fitst void loop iteration
//----------------------------------- Init

Adafruit_BME280 bme;
Adafruit_SGP30 sgp;
TFT_eSPI tft = TFT_eSPI();

//----------------------------------- Sensors

struct dataBME280 {
    float temp;
    float hum;
    float press;
};

struct dataSGP30 {
    float eco2;
    float tvoc;
};

unsigned long lastSensorRead = 0;
const int sensorInterval = 5000;
bool firstSensorsRead = true; // flag for fitst void loop iteration

dataBME280 readBME280() {
    dataBME280 data;

    data.temp = bme.readTemperature();
    data.hum = bme.readHumidity();
    data.press = bme.readPressure() / 133.322F;

    return data;
}

dataSGP30 readSGP30() {
    dataSGP30 data;

    if (sgp.IAQmeasure()) {
        data.eco2 = sgp.eCO2;
        data.tvoc = sgp.TVOC;
    }

    return data;
}

void correctHumidity(float temp, float hum) {
    if (!isnan(temp) && !isnan(hum)) {
        float absHumidity = 216.7 * ((hum/100.0) * 6.112 * exp((17.62*temp)/(243.12+temp)) / (273.15+temp));
        sgp.setHumidity(absHumidity * 1000);
    }
}

float calcIAQ(float eCO2, float TVOC) {
  float TVOC_sub = constrain((TVOC / 5000.0) * 500.0, 0, 500);
  float CO2_sub  = constrain(((eCO2 - 400.0) / (2000.0 - 400.0)) * 500.0, 0, 500);
  float IAQ = 0.7 * TVOC_sub + 0.3 * CO2_sub;
  return IAQ;
}

//----------------------------------- UI Draw

void drawValue(float value, uint16_t x, uint16_t y, const char* unit, uint16_t color) {
    tft.setTextSize(3);
    tft.setCursor(x, y);
    tft.print(value, 1);
    tft.setTextColor(color, TFT_BLACK);
    tft.print(unit);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

//----------------------------------- SGP30 Calibration load

bool loadBaseline() {
  if (LittleFS.exists("/baseline")) {
    File file = LittleFS.open("/baseline", "r");
    uint16_t eco2, tvoc;
    file.readBytes((char*)&eco2, 2);
    file.readBytes((char*)&tvoc, 2);
    file.close();
    
    sgp.setIAQBaseline(eco2, tvoc);
    return true;
  }
  return false;
}

void logMeassage (const char* mess, bool type) {
    if (type) {
        tft.setTextColor(TFT_GREEN);
        tft.println(mess);
    } else {
        tft.setTextColor(TFT_RED);
        tft.println(mess);
    }
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

  // LittleFS init
  initLittleFS();

  // BME280
  if (bme.begin(0x76)) {
    logMeassage("BME280 OK", true);
  } else {
    logMeassage("BME280 FAIL", false);
  }

  // SGP30
  if (sgp.begin()) {
    logMeassage("SGP30 OK", true);
  } else {
    logMeassage("SGP30 FAIL", false);
  }

  // Calibration
  if (loadBaseline()) {
    logMeassage("Calibration found", true);
  } else {
    logMeassage("Calibration not found", false);
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
    logMeassage("WiFi OK", true);
  } else {
    logMeassage("WiFi FAIL", false);
  }
  
  // Time sync
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo, 3000)) {
    logMeassage("Time sync OK", true);
  } else {
    logMeassage("Time sync FAIL", false);
  }

  tft.setTextColor(TFT_BLUE);
  tft.println("Free RAM: ");
  tft.print(ESP.getFreeHeap());

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
    
    dataBME280 readingsBME280 = readBME280();
    correctHumidity(readingsBME280.temp, readingsBME280.hum);
    dataSGP30 readingsSGP30 = readSGP30();
    lastSensorRead = now;

    drawValue(readingsBME280.temp,15, 60, " C", TFT_RED);
    drawValue(readingsBME280.hum, 15, 95, " %", TFT_BLUE);
    drawValue(readingsBME280.press, 135, 60, " mmHg", TFT_GREEN);

    uint16_t aqi = calcIAQ(readingsSGP30.eco2, readingsSGP30.tvoc);
    if (aqi <= 30) {
        drawValue(aqi, 135, 95, " AQI", TFT_GREEN);
    } else if (aqi > 30 && aqi <= 50) {
        drawValue(aqi, 135, 95, " AQI", TFT_GREENYELLOW);
    } else if (aqi > 50 && aqi <= 80) {
        drawValue(aqi, 135, 95, " AQI", TFT_YELLOW);
    } else if (aqi > 80 && aqi <= 120) {
        drawValue(aqi, 135, 95, " AQI", TFT_ORANGE);
    } else {
        drawValue(aqi, 135, 95, " AQI", TFT_RED);
    }

    tft.setTextSize(2);
    tft.setCursor(155, 0);
    tft.print(readingsSGP30.eco2, 1);
    tft.print(readingsSGP30.tvoc, 1);
  }
}