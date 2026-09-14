#include "cloud/command_log.hpp"
#include <Arduino.h>
#include <time.h>
#include <cstring>

namespace cloud {

namespace {

struct Entry {
    time_t      timestamp;   // 0 = empty slot
    char        command[24];
};

Entry  g_entries[CommandLog::CAPACITY] = {};
size_t g_next_slot = 0;   // next write position (circular)
size_t g_count     = 0;   // entries used so far, caps at CAPACITY

}  // namespace

void CommandLog::record(const char* command) {
    Entry& e = g_entries[g_next_slot];
    e.timestamp = time(nullptr);  // 0 if NTP hasn't synced yet
    strncpy(e.command, command, sizeof(e.command) - 1);
    e.command[sizeof(e.command) - 1] = '\0';

    g_next_slot = (g_next_slot + 1) % CAPACITY;
    if (g_count < CAPACITY) g_count++;
}

size_t CommandLog::formatHistory(char* out, size_t out_size) {
    if (g_count == 0) {
        snprintf(out, out_size, "Sin comandos registrados todavia.");
        return 0;
    }

    size_t written = 0;
    out[0] = '\0';
    size_t used = 0;  // strlen(out), tracked manually instead of recomputed

    // Walk newest -> oldest. g_next_slot is the next *write* position, so the
    // most recent entry is the one right before it.
    for (size_t i = 0; i < g_count; i++) {
        const size_t idx = (g_next_slot + CAPACITY - 1 - i) % CAPACITY;
        const Entry& e = g_entries[idx];

        char line[64];
        if (e.timestamp == 0) {
            snprintf(line, sizeof(line), "(sin hora) %s\n", e.command);
        } else {
            struct tm tm_info;
            localtime_r(&e.timestamp, &tm_info);
            char ts[24];
            strftime(ts, sizeof(ts), "%d/%m/%Y %H:%M:%S", &tm_info);
            snprintf(line, sizeof(line), "%s — %s\n", ts, e.command);
        }

        const size_t line_len = strlen(line);
        // out_size - 1 reserves room for the trailing '\0'. Guard against
        // used ever reaching/exceeding that (must never happen, but a
        // size_t underflow here previously caused heap corruption).
        if (used + 1 >= out_size) break;
        const size_t remaining = out_size - 1 - used;
        if (line_len >= remaining) break;  // out of room, stop here

        memcpy(out + used, line, line_len + 1);  // includes '\0'
        used += line_len;
        written++;
    }

    return written;
}

}  // namespace cloud
