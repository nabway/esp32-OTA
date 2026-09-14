#pragma once
#include <cstdint>
/**
 * @file smart_plug.hpp
 * @brief Control of a local ESPHome smart plug over plain HTTP (LAN).
 *
 * Keeps all the plug-specific HTTP details out of the Telegram layer, the same
 * way the Menta project keeps the sensor logic in its own module. The bot only
 * talks to this small, intent-based API.
 */

namespace device {

enum class PlugState { On, Off, Unreachable };

class SmartPlug {
public:
    // GET /switch/switch  — current relay state.
    static PlugState status();

    // POST control endpoints. Return true on HTTP 200.
    static bool turnOn();
    static bool turnOff();
    static bool toggle();
};

}  // namespace device
