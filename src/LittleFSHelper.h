#pragma once
#include <LittleFS.h>

bool initLittleFS(bool formatIfFailed = false);  // параметр по умолчанию
bool formatLittleFS();