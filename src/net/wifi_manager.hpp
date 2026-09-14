#pragma once
#include <cstdint>
/**
 * @file wifi_manager.hpp
 * @brief WiFi connection with multi-network fallback and automatic reconnect.
 *
 * Reused as-is from the Menta project. Tries each known network (see secrets)
 * in order, and reconnects silently from loop() if the link drops.
 */

namespace net {

class WiFiManager {
public:
    // Connect on first call. Blocks until connected or timeout (ms).
    static bool begin(uint32_t timeout_ms = 15000);

    // Call from loop() — reconnects silently if connection dropped.
    static void loop();

    static bool isConnected();
};

}  // namespace net
