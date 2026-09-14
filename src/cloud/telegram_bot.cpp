#include "cloud/telegram_bot.hpp"
#include "device/smart_plug.hpp"
#include "config/secrets.hpp"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ota/ota_manager.hpp"

namespace cloud {

// ── Internal state ────────────────────────────────────────────────────────────

static constexpr char     TG_API[]         = "https://api.telegram.org";
static int32_t            last_update      = 0;     // Telegram update_id offset
static uint32_t           last_poll_ms     = 0;

// Telegram polls run over TLS. We open a fresh secure client per poll (same as
// the Menta bot). A full handshake every second is wasteful, so we poll a bit
// slower here. For snappier 1 s polling, promote `client` to a static/persistent
// WiFiClientSecure and reuse the connection instead of recreating it.
static constexpr uint32_t POLL_INTERVAL_MS = 2000;

// Inline keyboard with the 3 plug commands, sent under every bot reply.
// callback_data is what comes back in the callback_query — keep it short,
// Telegram caps it at 64 bytes.
static constexpr char KEYBOARD_JSON[] =
    "{\"inline_keyboard\":[["
        "{\"text\":\"🟢 ON\",\"callback_data\":\"/on\"},"
        "{\"text\":\"🔴 OFF\",\"callback_data\":\"/off\"},"
        "{\"text\":\"🔁 TOGGLE\",\"callback_data\":\"/toggle\"}"
    "]]}";

// ── HTTP helpers ──────────────────────────────────────────────────────────────

// NOTE: text is injected raw into JSON. Keep messages free of " and \ , or add
// proper escaping if you ever forward user-supplied strings back to Telegram.
static bool postMessage(const char* chat_id, const char* text) {
    WiFiClientSecure client;
    client.setInsecure();  // skip cert validation (matches Menta; swap for
                           // client.setCACert(TELEGRAM_CERTIFICATE_ROOT) to verify)

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "%s/bot%s/sendMessage", TG_API, secrets::TELEGRAM_TOKEN);

    char body[512];
    snprintf(body, sizeof(body),
             "{\"chat_id\":\"%s\",\"text\":\"%s\"}", chat_id, text);

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body);

    if (code == 200) { http.end(); return true; }
    log_w("Telegram sendMessage failed, HTTP %d — body: %s", code, http.getString().c_str());
    http.end();
    return false;
}

// Same as postMessage but attaches the inline keyboard (reply_markup).
static bool postMessageWithKeyboard(const char* chat_id, const char* text) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "%s/bot%s/sendMessage", TG_API, secrets::TELEGRAM_TOKEN);

    char body[768];
    snprintf(body, sizeof(body),
             "{\"chat_id\":\"%s\",\"text\":\"%s\",\"reply_markup\":%s}",
             chat_id, text, KEYBOARD_JSON);

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body);

    if (code == 200) { http.end(); return true; }
    log_w("Telegram sendMessageWithKeyboard failed, HTTP %d — body: %s", code, http.getString().c_str());
    http.end();
    return false;
}

// Acknowledges a button tap so Telegram stops showing the little loading
// spinner on the button. Required by the API even if you have nothing to say;
// passing nullptr skips the toast text shown to the user.
static void answerCallbackQuery(const char* callback_query_id, const char* toast_text) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[160];
    snprintf(url, sizeof(url), "%s/bot%s/answerCallbackQuery", TG_API, secrets::TELEGRAM_TOKEN);

    char body[256];
    if (toast_text) {
        snprintf(body, sizeof(body),
                 "{\"callback_query_id\":\"%s\",\"text\":\"%s\"}",
                 callback_query_id, toast_text);
    } else {
        snprintf(body, sizeof(body),
                 "{\"callback_query_id\":\"%s\"}", callback_query_id);
    }

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body);
    if (code != 200) {
        log_w("answerCallbackQuery failed, HTTP %d — body: %s", code, http.getString().c_str());
    }
    http.end();
}

// ── Command dispatch ──────────────────────────────────────────────────────────
// Add new commands here — this if/else chain is the single place to extend.
// Shared by both typed commands (/on) and button taps (callback_data "/on").

static void handleCommand(const char* chat_id, const char* text) {
    // Authorization: only the configured chat may control the plug.
    if (strcmp(chat_id, secrets::TELEGRAM_CHAT_ID) != 0) {
        log_w("Ignoring command from unauthorized chat %s", chat_id);
        return;
    }

    if (strcmp(text, "/status") == 0) {
        switch (device::SmartPlug::status()) {
            case device::PlugState::On:  postMessageWithKeyboard(chat_id, "Relay is ON");  break;
            case device::PlugState::Off: postMessageWithKeyboard(chat_id, "Relay is OFF"); break;
            default:                     postMessageWithKeyboard(chat_id, "Couldn't reach the plug"); break;
        }
    } else if (strcmp(text, "/on") == 0) {
        postMessageWithKeyboard(chat_id, device::SmartPlug::turnOn()  ? "Turning ON"  : "Failed to turn ON");
    } else if (strcmp(text, "/off") == 0) {
        postMessageWithKeyboard(chat_id, device::SmartPlug::turnOff() ? "Turning OFF" : "Failed to turn OFF");
    } else if (strcmp(text, "/toggle") == 0) {
        postMessageWithKeyboard(chat_id, device::SmartPlug::toggle()  ? "Toggled"     : "Failed to toggle");
    } else if (strncmp(text, "/ota", 4) == 0) {
        // /ota http://host/firmware.bin
        const char* p = text + 4;
        while (*p == ' ') ++p;
        if (*p == '\0') {
            postMessageWithKeyboard(chat_id, "Usage: /ota http://host/firmware.bin");
        } else {
            postMessageWithKeyboard(chat_id, "Starting OTA — downloading firmware");
            int r = ota::performHttpUpdate(p);
            if (r == 1) {
                postMessageWithKeyboard(chat_id, "OTA written successfully — rebooting");
                delay(1000);
                ESP.restart();
            } else {
                char buf[128];
                snprintf(buf, sizeof(buf), "OTA failed (code %d)", r);
                postMessageWithKeyboard(chat_id, buf);
            }
        }
    } else {
        postMessageWithKeyboard(chat_id, "Commands: /status /on /off /toggle");
    }
}

// ── Public ────────────────────────────────────────────────────────────────────

bool TelegramBot::send(const char* message) {
    return postMessageWithKeyboard(secrets::TELEGRAM_CHAT_ID, message);
}

void TelegramBot::poll() {
    const uint32_t now = millis();
    if (now - last_poll_ms < POLL_INTERVAL_MS) return;
    last_poll_ms = now;

    if (WiFi.status() != WL_CONNECTED) return;  // nothing to do without a link

    log_i("Telegram: polling for updates...");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[224];
    snprintf(url, sizeof(url),
             "%s/bot%s/getUpdates?offset=%ld&timeout=0",
             TG_API, secrets::TELEGRAM_TOKEN, (long)(last_update + 1));

    http.begin(client, url);
    const int code = http.GET();
    if (code != 200) {
        log_w("Telegram: getUpdates failed, HTTP %d", code);
        http.end();
        return;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) { log_w("Telegram JSON parse failed: %s", err.c_str()); return; }

    JsonArray results = doc["result"].as<JsonArray>();
    log_i("Telegram: got %d updates", results.size());
    if (results.isNull() || results.size() == 0) return;

    for (JsonObject update : results) {
        // Advance the offset for every update so we never reprocess it,
        // even ones we end up ignoring.
        last_update = update["update_id"].as<int32_t>();

        // ── Button tap (callback_query) ──────────────────────────────────────
        if (!update["callback_query"].isNull()) {
            JsonObject cq = update["callback_query"];

            const long long chat_id_num = cq["message"]["chat"]["id"] | 0LL;
            char chat_id[24];
            snprintf(chat_id, sizeof(chat_id), "%lld", chat_id_num);

            const char* callback_query_id = cq["id"] | "";
            const char* data = cq["data"] | "";

            log_i("Telegram button tap from %s: %s", chat_id, data);

            // Must ack every callback_query, even unauthorized ones, or the
            // button spinner hangs forever on the user's end.
            answerCallbackQuery(callback_query_id, nullptr);

            if (chat_id_num != 0 && data[0] != '\0') {
                handleCommand(chat_id, data);
            }
            continue;
        }

        // ── Typed text command (message) ─────────────────────────────────────
        // chat.id is a number in the Telegram API — render it as a decimal
        // string so it matches secrets::TELEGRAM_CHAT_ID.
        const long long chat_id_num = update["message"]["chat"]["id"] | 0LL;
        if (chat_id_num == 0) continue;
        char chat_id[24];
        snprintf(chat_id, sizeof(chat_id), "%lld", chat_id_num);

        const char* text = update["message"]["text"] | "";
        if (text[0] == '\0') continue;  // skip non-text messages

        log_i("Telegram command from %s: %s", chat_id, text);
        handleCommand(chat_id, text);
    }
}

}  // namespace cloud
