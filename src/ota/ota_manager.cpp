#include "ota/ota_manager.hpp"
#include <esp_ota_ops.h>
#include "version.hpp"
#include <Preferences.h>

namespace ota {

static bool pending_verify = false;
static uint32_t pending_since = 0;
static const uint32_t VERIFY_DELAY_MS = 8000; // wait this long before confirming

void begin() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_err_t st = esp_ota_get_state_partition(running, &state);
    if (st == ESP_OK) {
        Serial.printf("OTA: running partition %s state=%d\n", running->label, (int)state);
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            pending_verify = true;
            pending_since = millis();
            Serial.println("OTA: pending verification — will confirm after delay if stable");
        }
    } else {
        Serial.printf("OTA: could not read partition state: %d\n", st);
    }
}

void loop() {
    if (pending_verify && (uint32_t)(millis() - pending_since) > VERIFY_DELAY_MS) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            Serial.println("OTA: app marked valid — rollback cancelled");
            // Store this firmware version as the last good one so the device
            // can display the previous working version later on.
            Preferences prefs;
            prefs.begin("ota", false);
            prefs.putString("last_good_ver", APP_VERSION);
            prefs.end();
        } else {
            Serial.printf("OTA: esp_ota_mark_app_valid_cancel_rollback failed: %d\n", err);
        }
        pending_verify = false;
    }
}

int performHttpUpdate(const char* url) {
    if ((WiFi.status() != WL_CONNECTED)) {
        Serial.println("OTA: WiFi not connected");
        return -1;
    }

    HTTPClient http;
    WiFiClient client;
    Serial.printf("OTA: fetching %s\n", url);
    if (!http.begin(client, url)) {
        Serial.println("OTA: http.begin failed");
        return -2;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("OTA: HTTP GET failed, code=%d\n", httpCode);
        http.end();
        return -3;
    }

    int len = http.getSize();
    bool canBegin = Update.begin(len > 0 ? len : 0x400000); // try with size hint
    if (!canBegin) {
        Serial.println("OTA: Not enough space to begin update");
        http.end();
        return -4;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (!Update.end()) {
        Serial.printf("OTA: Update failed. Error #: %d\n", Update.getError());
        http.end();
        return -5;
    }

    if (Update.isFinished()) {
        Serial.printf("OTA: Update complete — %u bytes written\n", (unsigned)written);
        http.end();
        return 1;
    } else {
        Serial.println("OTA: Update not finished");
        http.end();
        return -6;
    }
}

} // namespace ota
