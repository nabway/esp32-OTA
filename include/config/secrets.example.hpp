#pragma once
/**
 * @file secrets.example.hpp
 * @brief Template for the fields the refactored plug project expects.
 *        Copy to secrets.hpp and fill in. Do NOT commit secrets.hpp.
 */

namespace secrets {

// ── WiFi (multi-network fallback, like Menta) ──────────────────────────────
// If you only have one AP, point _2 / _3 at the same SSID/password.
inline constexpr char WIFI_SSID[]       = "MyHomeWiFi";
inline constexpr char WIFI_PASSWORD[]   = "supersecret";
inline constexpr char WIFI_SSID_2[]     = "MyHomeWiFi";
inline constexpr char WIFI_PASSWORD_2[] = "supersecret";
inline constexpr char WIFI_SSID_3[]     = "Phone-Hotspot";
inline constexpr char WIFI_PASSWORD_3[] = "hotspotpass";

// ── Telegram ───────────────────────────────────────────────────────────────
inline constexpr char TELEGRAM_TOKEN[]   = "123456:ABC-yourbottoken";
inline constexpr char TELEGRAM_CHAT_ID[] = "11111111";   // your numeric chat id, as a string

// ── ESPHome smart plug ───────────────────────────────────────────────────────
inline constexpr char PLUG_IP[] = "192.168.1.50";

}  // namespace secrets
