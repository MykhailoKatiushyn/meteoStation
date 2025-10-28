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

//----------------------------------- Button and screen logic

#define BUTTON 16
int currentScreen = 0;
bool lastButtonState = HIGH;

//----------------------------------- Global constants etc

const char* ssid = "myNET";
const char* password = "Li#lu~2014";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;  // GMT+2
const int daylightOffset_sec = 0;

bool firstTimeRead = true; // flags for fitst void loop iteration
bool firstSensorsRead = true;

const int sensorInterval = 5000;
const int timeInterval = 30000;

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
    float iaq;
};

struct SensorData {
  dataBME280 bmeData;
  dataSGP30 sgpData;
  struct tm timeinfo;
};

SensorData sensors;



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

void updateSensors() {
  sensors.bmeData.temp = bme.readTemperature();
  sensors.bmeData.hum = bme.readHumidity();
  sensors.bmeData.press = bme.readPressure() / 133.322F;
  
  correctHumidity(sensors.bmeData.temp, sensors.bmeData.hum);

  if (sgp.IAQmeasure()) {
    sensors.sgpData.eco2 = sgp.eCO2;
    sensors.sgpData.tvoc = sgp.TVOC;
  }

  sensors.sgpData.iaq = calcIAQ(sensors.sgpData.eco2, sensors.sgpData.tvoc);
}

void updateTime() {
  getLocalTime(&sensors.timeinfo);
}

//----------------------------------- UI Draw

void drawValue(float value, uint16_t x, uint16_t y, const char* unit, uint16_t color) {
    tft.setCursor(x, y);
    tft.print(value, 1);
    tft.setTextColor(color, TFT_BLACK);
    tft.print(unit);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void drawDashboard() {
    //Time draw

    tft.setTextSize(5);
    tft.setCursor(80, 10);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print(&sensors.timeinfo, "%H:%M");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    //Values draw
    

    tft.setTextSize(3);
    drawValue(sensors.bmeData.temp, 15, 60, " C", TFT_RED);
    drawValue(sensors.bmeData.hum, 15, 95, " %", TFT_BLUE);
    drawValue(sensors.bmeData.press, 135, 60, " mmHg", TFT_GREEN);

    float iaq = sensors.sgpData.iaq;
    uint16_t color;

    if (iaq <= 30) color = TFT_GREEN;
    else if (iaq <= 50) color = TFT_GREENYELLOW;
    else if (iaq <= 80) color = TFT_YELLOW;
    else if (iaq <= 120) color = TFT_ORANGE;
    else color = TFT_RED;

    drawValue(iaq, 135, 95, " IAQ  ", color);
}

void drawIAQScreen() {
  tft.setTextSize(3);
  drawValue(sensors.sgpData.eco2, 15, 60, " eCO2", TFT_BLUE);
  drawValue(sensors.sgpData.tvoc, 15, 95, " TVOC", TFT_COLMOD);
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

//----------------------------------- Log

void logMeassage (const char* mess, bool type) {
    if (type) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println(mess);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println(mess);
    }
}


//----------------------------------- Setup



void setup() {
  Serial.begin(115200);

  pinMode(BUTTON, INPUT_PULLUP);

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
  unsigned long now = millis();
  bool shouldRedraw = true;
  bool shouldClear = false;

  bool buttonState = digitalRead(BUTTON);

  if (lastButtonState == HIGH && buttonState == LOW) {
      currentScreen = !currentScreen;
      shouldClear = true;
      Serial.print("buttonState: ");
      Serial.println(buttonState);
      Serial.print("currentScreen: ");
      Serial.println(currentScreen);
      Serial.print("lastButtonState: ");
      Serial.println(lastButtonState);
  }

  lastButtonState = buttonState;

  
  //------------------- Time update
  
  static unsigned long lastUpdate = 0;

  if(firstTimeRead || now - lastUpdate >= timeInterval) {
    updateTime();

    firstTimeRead = false;
    lastUpdate = now;
    shouldRedraw = true;
  }

  //------------------- Values update

  static unsigned long lastSensorRead = 0;

  if (firstSensorsRead || now - lastSensorRead >= sensorInterval) {
    updateSensors();

    firstSensorsRead = false;
    lastSensorRead = now;
    shouldRedraw = true;
  }

  if (shouldClear) {
    tft.fillScreen(TFT_BLACK);

    if (!currentScreen) drawDashboard();
    else drawIAQScreen();

    shouldClear = false;
  } else if (shouldRedraw) {
    if (!currentScreen) drawDashboard();
    else drawIAQScreen();

    shouldRedraw = false; 
  }
}