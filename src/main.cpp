#include <Arduino.h>
#include <time.h>
#include "net/wifi_manager.hpp"
#include "cloud/telegram_bot.hpp"
#include "ota/ota_manager.hpp"
#include "version.hpp"
#include "display/oled.hpp"
#include "features/led_example.hpp"
#include <Preferences.h>

// Buenos Aires (UTC-3, sin horario de verano). Ajustá si tu zona es otra.
static constexpr long GMT_OFFSET_SEC      = -3 * 3600;
static constexpr int  DAYLIGHT_OFFSET_SEC = 0;

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}

    log_i("=== Smart Plug Bot boot ===");

    // Multi-network connect with timeout. On total failure we reboot, same as
    // the Menta device, rather than spin forever.
    if (!net::WiFiManager::begin(30000)) {
        log_e("WiFi failed — rebooting in 5 s");
        delay(5000);
        ESP.restart();
    }

    // NTP sync — needed so cloud::CommandLog can stamp /historial entries
    // with real dates instead of the 1970 epoch. Requires WiFi to be up.
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    log_i("Waiting for NTP sync...");
    time_t now = time(nullptr);
    uint32_t ntp_start = millis();
    while (now < 8 * 3600 * 2 && millis() - ntp_start < 10000) {  // sanity: year > 1970+
        delay(250);
        now = time(nullptr);
    }
    if (now < 8 * 3600 * 2) {
        log_w("NTP sync timed out — /historial timestamps will be wrong until it syncs later");
    } else {
        log_i("NTP synced");
    }

    // Boot notification so you know the device came online.
    cloud::TelegramBot::send("🔌 Smart Plug Bot online");

    // OTA manager: check if we booted into a pending image and verify later
    ota::begin();

    // Show firmware version (also useful if you have a small display attached)
    Serial.printf("Firmware version: %s\n", APP_VERSION);
    display::begin();

    // Read last known good version from NVS (written by OTA manager on
    // successful confirmation). If none, show "-".
    Preferences prefs;
    prefs.begin("ota", true);
    String last_good = prefs.getString("last_good_ver", "-");
    prefs.end();

    // WiFi state and IP
    const char* wifi_state = (WiFi.status() == WL_CONNECTED) ? "WiFi:OK" : "WiFi:DOWN";
    String ip = WiFi.localIP().toString();
    display::showStatus(APP_VERSION, last_good.c_str(), ip.c_str(), wifi_state);
    Serial.println("To perform HTTP OTA: send line starting with 'U' followed by URL on Serial (e.g. Uhttp://host/firmware.bin)");

    // === USER CODE INITIALIZATION ===
    // Add your project setup here (features/led_example.hpp is an example)
    app::setup();

    log_i("Setup complete");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    // Silently reconnect if WiFi dropped (throttled internally to 15 s).
    net::WiFiManager::loop();

    // Poll Telegram for commands (self-throttled to POLL_INTERVAL_MS).
    cloud::TelegramBot::poll();

    // OTA manager loop (confirms pending images after being stable)
    ota::loop();

    // Simple Serial-triggered OTA for testing: send a line starting with 'U' + URL
    // or send 'X' to force a hard crash and exercise OTA rollback.
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) return;
        if (line.charAt(0) == 'X') {
            Serial.println("FORCE ROLLBACK TEST: hard crash in 100 ms");
            delay(100);
            *((volatile uint32_t*)0) = 0;
        } else if (line.charAt(0) == 'U') {
            String url = line.substring(1);
            url.trim();
            if (url.length() > 0) {
                Serial.printf("Starting OTA from: %s\n", url.c_str());
                int r = ota::performHttpUpdate(url.c_str());
                Serial.printf("OTA result: %d\n", r);
                if (r == 1) {
                    Serial.println("OTA successful — rebooting in 2s");
                    delay(2000);
                    ESP.restart();
                }
            } else {
                Serial.println("Usage: Uhttp://server/firmware.bin");
            }
        }
    }

    // === USER CODE EXECUTION ===
    // Add your project loop here (features/led_example.hpp is an example)
    app::loop();
}
