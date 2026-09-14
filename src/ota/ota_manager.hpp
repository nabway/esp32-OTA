#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

namespace ota {

void begin();
void loop();
int performHttpUpdate(const char* url);

} // namespace ota
