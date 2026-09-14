#pragma once
#include <cstdint>
/**
 * @file telegram_bot.hpp
 * @brief Telegram command bot for the smart plug.
 *
 * Same architecture as the Menta bot (raw HTTP + ArduinoJson, no external bot
 * library) so both projects share one consistent, fully-controllable pattern.
 * Commands are dispatched to device::SmartPlug.
 */

namespace cloud {

class TelegramBot {
public:
    // Send a message to the primary authorized chat (e.g. boot notice).
    static bool send(const char* message);

    // Poll Telegram for incoming commands and dispatch them.
    // Self-throttled (POLL_INTERVAL_MS) — safe to call every loop().
    static void poll();
};

}  // namespace cloud
