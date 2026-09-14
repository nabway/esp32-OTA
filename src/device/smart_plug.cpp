#include "device/smart_plug.hpp"
#include "config/secrets.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

namespace device {

// The plug is on the LAN and speaks plain HTTP — no TLS here.
static int request(const char* path, const char* method, String& payload_out) {
    HTTPClient http;
    const String url = String("http://") + secrets::PLUG_IP + path;
    http.begin(url);
    http.addHeader("Content-Length", "0");

    int code;
    if (strcmp(method, "POST") == 0) code = http.POST("");
    else                              code = http.GET();

    payload_out = (code == 200) ? http.getString() : String();
    log_i("%s %s -> %d", method, path, code);
    http.end();
    return code;
}

PlugState SmartPlug::status() {
    String payload;
    const int code = request("/switch/switch", "GET", payload);
    if (code != 200) return PlugState::Unreachable;
    // ESPHome JSON: {"id":"switch-switch","state":"ON",...}
    return payload.indexOf("\"state\":\"ON\"") != -1 ? PlugState::On : PlugState::Off;
}

bool SmartPlug::turnOn() {
    String discard;
    return request("/switch/switch/turn_on", "POST", discard) == 200;
}

bool SmartPlug::turnOff() {
    String discard;
    return request("/switch/switch/turn_off", "POST", discard) == 200;
}

bool SmartPlug::toggle() {
    String discard;
    return request("/switch/switch/toggle", "POST", discard) == 200;
}

}  // namespace device
