#include <Arduino.h>

namespace app {

// GPIO pin for LED (adjust to your board)
static constexpr uint8_t LED_PIN = 2;

void setup() {
    // User code initialization
    Serial.println("Hello World — user setup() running");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void loop() {
    // User code main loop — runs after framework components
    static uint32_t last_toggle = 0;
    uint32_t now = millis();

    if (now - last_toggle >= 1000) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        Serial.println("LED toggle");
        last_toggle = now;
    }
}

}  // namespace app
