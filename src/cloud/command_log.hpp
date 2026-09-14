#pragma once
#include <cstdint>
#include <cstddef>
/**
 * @file command_log.hpp
 * @brief Ring buffer of the last N Telegram commands, with wall-clock time.
 *
 * Requires NTP to be synced (configTime in main.cpp) for timestamps to be
 * meaningful — before sync, entries show the epoch (1970-01-01).
 */

namespace cloud {

class CommandLog {
public:
    static constexpr size_t CAPACITY = 5;

    // Record a command as just executed (e.g. "/on", "/toggle").
    static void record(const char* command);

    // Render the whole log into a human-readable, Telegram-ready string
    // ("dd/mm/yyyy HH:MM:SS — /command", newest first). Returns the number
    // of entries written.
    static size_t formatHistory(char* out, size_t out_size);
};

}  // namespace cloud
