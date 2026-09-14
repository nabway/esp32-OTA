#pragma once
/**
 * @file pins.hpp
 * @brief Single source of truth for all GPIO assignments.
 *
 * Never hardcode pin numbers elsewhere. If a pin must change,
 * this is the only file to touch.
 */

namespace pins {

constexpr int ONBOARD_LED = 2;

// I2C pins for a typical ESP32 devkit; adjust if your board uses other pins
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

}  // namespace pins
