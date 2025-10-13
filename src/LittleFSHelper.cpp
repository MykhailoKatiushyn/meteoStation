#include "LittleFSHelper.h"
#include <Arduino.h>

bool initLittleFS(bool formatIfFailed) {
  if (LittleFS.begin(!formatIfFailed)) {  // begin(false) - не форматировать
    Serial.println("✅ LittleFS смонтирована");
    return true;
  }
  
  if (formatIfFailed) {
    Serial.println("❌ Ошибка монтирования, пробуем форматировать...");
    return formatLittleFS();  // форматируем и возвращаем результат
  }
  
  Serial.println("❌ Ошибка монтирования LittleFS");
  return false;
}

bool formatLittleFS() {
  if (LittleFS.format()) {
    Serial.println("✅ LittleFS отформатирована");
    
    // После форматирования нужно перемонтировать
    if (LittleFS.begin(false)) {
      return true;
    }
  }
  
  Serial.println("❌ Ошибка форматирования LittleFS");
  return false;
}