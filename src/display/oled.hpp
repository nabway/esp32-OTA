#pragma once

#include <Arduino.h>

namespace display {
void begin();
void showStatus(const char* current_version, const char* previous_version, const char* ip, const char* wifi_status);
}
