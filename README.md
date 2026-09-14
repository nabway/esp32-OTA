# ESP32 OTA Manager

HTTP-based firmware updates for ESP32 with automatic rollback protection and Telegram bot control.

## Features

- **OTA over HTTP** — download and flash firmware without USB cable
- **Automatic rollback** — if new image crashes within 8s, bootloader reverts automatically
- **Telegram bot** — trigger updates from your phone (`/ota http://...`)
- **Dual partitions** — safe concurrent OTA (app0 ↔ app1)
- **Multi-network WiFi** — connect to up to 3 SSIDs with 30s timeout
- **OLED display** — shows current version, last known good, WiFi state and IP
- **NTP sync** — accurate timestamps for logging
- **Serial fallback** — trigger OTA via USB if needed

## Project Structure

```
esp32-OTA/
├── src/
│   ├── main.cpp              ← entry point + loop orchestration
│   ├── ota/                  ← OTA manager (HTTP download + flash)
│   │   └── ota_manager.cpp
│   ├── cloud/                ← Telegram bot
│   │   └── telegram_bot.cpp
│   ├── net/                  ← WiFi manager
│   │   └── wifi_manager.cpp
│   ├── display/              ← OLED screen
│   │   └── oled.cpp
│   └── features/             ← ADD YOUR CODE HERE
│       ├── led_example.cpp   ← Example
│       └── led_example.hpp
│
├── include/
│   ├── config/
│   │   ├── secrets.hpp       ← WiFi + Telegram credentials
│   │   └── pins.hpp          ← GPIO
│   └── features/
│       └── led_example.hpp
│
└── scripts/
    ├── serve_firmware.py     ← HTTP server for OTA
    ├── build_and_publish.sh  ← build + copy firmware update
    └── send_telegram_ota.sh  ← shortcut to trigger OTA service
```

## Execution Flow

```
SETUP PHASE
├─ Serial.begin()
├─ WiFiManager::begin()       [auto]
├─ configTime() + NTP sync    [auto]
├─ TelegramBot::begin()       [auto]
├─ OTA::begin()               [auto]
├─ Display::begin()           [auto]
└─ app::setup()               [YOUR CODE RUNS HERE]

LOOP PHASE (continuous)
├─ WiFiManager::loop()        [auto]
├─ TelegramBot::poll()        [auto] → handles /status, /ota, etc.
├─ OTA::loop()                [auto] → validates firmware after 8s
├─ Serial input handler       [auto] → U<url> or X for testing
└─ app::loop()                [YOUR CODE RUNS HERE]
```

## Quick Start

### 1. Set up Telegram bot

```bash
# In Telegram, message @BotFather
/newbot
# Save the TOKEN

# Send any message to your bot, then:
curl -s "https://api.telegram.org/bot<TOKEN>/getUpdates" | jq '.result[0].message.chat.id'
# Save the CHAT_ID
```

### 2. Configure credentials

Edit `include/config/secrets.hpp`:

```cpp
static const char TELEGRAM_TOKEN[]   = "<YOUR_TOKEN>";
static const char TELEGRAM_CHAT_ID[] = "<YOUR_CHAT_ID>";
```

### 3. Flash initial firmware

First board flash requires USB cable:

```bash
cp include/config/secrets.example.hpp include/config/secrets.hpp
# Edit secrets.hpp with your credentials

pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-*
```

All future updates go over OTA.

### 4. Update firmware over OTA

```bash
# Terminal 1: Build and serve
./scripts/build_and_publish.sh
cd scripts/serve_dir && python3 ../serve_firmware.py

# Terminal 2: Get your LAN IP
ipconfig getifaddr en0

# Terminal 3: Trigger update
./scripts/send_telegram_ota.sh http://192.168.x.x:8000/firmware.bin
```

Bot will reply:
- `Starting OTA — downloading firmware`
- `OTA written successfully — rebooting`
- `🔌 Smart Plug Bot online` (after reboot)

After 8 seconds of stable boot, firmware auto-validates and rollback is cancelled.

## Adding Your Code

1. Create new files in `src/features/` (e.g., `relay.cpp`)
2. Add header in `include/features/` (e.g., `relay.hpp`)
3. Edit `src/main.cpp` includes:

```cpp
#include "features/relay.hpp"
```

4. Call your functions:

```cpp
void setup() {
    // ... framework init ...
    relay::setup();   // YOUR CODE
}

void loop() {
    // ... framework loops ...
    relay::loop();    // YOUR CODE
}
```

Reference: `src/features/led_example.cpp` shows a minimal hello-world with LED blink.

## OTA Error Codes

| Code | Cause |
|------|-------|
| `1` | Success (rebooting) |
| `-1` | No WiFi connection |
| `-2` | Invalid URL (use `http://`, not `https://`) |
| `-3` | Server unreachable (wrong IP, different network) |
| `-4` | Binary too large for OTA partition |
| `-5` | Corrupted image (failed validation) |
| `-6` | Incomplete download |

## Serial Commands

If connected via USB:

- `Uhttp://host:8000/firmware.bin` — trigger OTA via serial
- `X` — force crash to test rollback mechanism

## Telegram Commands

Customize in `src/cloud/telegram_bot.cpp`:

- `/ota http://host:8000/firmware.bin` — trigger firmware update
- `/status` — show current state
- `/on`, `/off`, `/toggle` — smart plug control (example)

## Technical Deep-Dive

See [docs/OTA_ARCHITECTURE.md](docs/OTA_ARCHITECTURE.md) for:
- Complete sequence diagram (8 phases)
- Partition state machine
- Timing constraints
- What happens during rollback

## Hardware

**Tested on:** ESP32-DevKit-V1 (30-pin)

**Display:** SSD1306 128x64 OLED on I2C (GPIO 21/22, address 0x3C)

**LED:** GPIO 2 (built-in blue LED) — configurable in `include/features/led_example.hpp`

## License

—
