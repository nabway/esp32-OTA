#include "display/oled.hpp"
#include "config/pins.hpp"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Default SSD1306 size
static const int OLED_WIDTH = 128;
static const int OLED_HEIGHT = 64;
static Adafruit_SSD1306 oledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire);
static bool oled_ok = false;

namespace display {

void begin() {
    Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
    if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED: SSD1306 allocation failed");
        oled_ok = false;
        return;
    }
    oled_ok = true;
    oledDisplay.clearDisplay();
    oledDisplay.setTextSize(1);
    oledDisplay.setTextColor(SSD1306_WHITE);
    oledDisplay.display();
}

void showStatus(const char* current_version, const char* previous_version, const char* ip, const char* wifi_status) {
    if (!oled_ok) return;
    oledDisplay.clearDisplay();
    oledDisplay.setTextSize(1);
    oledDisplay.setCursor(0, 0);
    oledDisplay.print("Cur:");
    oledDisplay.setTextSize(2);
    oledDisplay.setCursor(36, 0);
    oledDisplay.print(current_version);

    oledDisplay.setTextSize(1);
    oledDisplay.setCursor(0, 24);
    oledDisplay.print("Prev:");
    oledDisplay.setCursor(36, 24);
    oledDisplay.print(previous_version);

    oledDisplay.setCursor(0, 40);
    oledDisplay.print(wifi_status);
    oledDisplay.setCursor(80, 40);
    oledDisplay.print(ip);

    oledDisplay.display();
}

} // namespace display
