# OTA Architecture

Complete technical reference for the update flow from `serve_firmware.py` through firmware validation.

## The Sequence (8 Phases)

```
┌─────────────────────────────────────────┐
│ Phase 0-1: Dispatch                    │
│ PC sends /ota via Telegram             │
│ Message queued, ESP32 not yet notified │
└─────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ Phase 2-3: Poll & Authorize            │
│ ESP32 polls every 2s, finds command    │
│ Validates chat_id, parses URL          │
└─────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ Phase 4: Download                       │
│ HTTP GET firmware.bin (plain, no TLS)  │
│ Entire loop() blocks (~30s)            │
│ Stream written to OTA inactive part.   │
└─────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ Phase 5-6: Reboot & Setup              │
│ ESP.restart() → boot new partition     │
│ WiFi connect + NTP + mark PENDING_VER. │
└─────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ Phase 7: Validation                    │
│ +8 seconds: ota::loop() validates      │
│ IMG_VALID → rollback cancelled         │
│ Or if crash/reboot: auto rollback      │
└─────────────────────────────────────────┘
```

## Phase Breakdown

### Phase 0: PC listening

```python
# serve_firmware.py
socketserver.TCPServer(('', 8000), Handler).serve_forever()
```

Passive. Handler responds to any path via `SimpleHTTPRequestHandler`. Must run in directory containing `firmware.bin`.

### Phase 1: Dispatch from PC

```bash
# send_telegram_ota.sh
curl POST api.telegram.org/bot<TOKEN>/sendMessage \
  -d text="/ota http://<IP>:8000/firmware.bin"
```

Script exits immediately. Message sits in Telegram queue. ESP32 hasn't been notified yet.

### Phase 2: ESP32 polls Telegram

```cpp
// TelegramBot::poll() — every 2000 ms (throttled)
WiFiClientSecure client;
client.setInsecure();  // TLS handshake, no cert validation
http.begin(client, "https://api.telegram.org/...");
http.GET();
parseJson() → extract message.text
```

New TLS handshake each poll. High overhead. Returns on first `getUpdates` with offset.

### Phase 3: Authorization & Dispatch

```cpp
// handleCommand()
if (chat_id != TELEGRAM_CHAT_ID) return;  // silent discard
if (strncmp(text, "/ota", 4) == 0) {
    sendMessage("Starting OTA — downloading firmware");
    ota::performHttpUpdate(url);
}
```

Blocks until OTA completes (up to 30s for a large image).

### Phase 4: Download & Flash (Critical)

```cpp
// ota::performHttpUpdate(url)
WiFiClient client;  // plain HTTP, no TLS
int result = http.begin(client, url);           // -2 on failure
result = http.GET();                            // -3 if unreachable
size_t len = http.getSize();
result = Update.begin(len);                     // -4 if too large
result = Update.writeStream(*http.getStream()); // blocks loop()
result = Update.end();                          // -5 if validation fails
```

**The entire `loop()` blocks here.** Telegram polling stops. OTA validation loop doesn't run. WiFi keepalive may time out on very slow connections.

No TLS here — `http://` only. If passed `https://`, `http.begin()` fails with `-2`.

### Phase 5: Reboot

```cpp
if (r == 1) {
    sendMessage("OTA written successfully — rebooting");
    delay(1000);
    ESP.restart();
}
```

Boot partition already switched by `Update.end()`.

### Phase 6: Boot new image

```cpp
void setup() {
    Serial.begin(115200);
    
    // Up to 30s if WiFi fails
    if (!WiFiManager::begin(30000)) {
        ESP.restart();  // retry
    }
    
    // Up to 10s for NTP
    configTime(GMT_OFFSET, 0, "pool.ntp.org");
    while (now < 1970+8years && timeout < 10s) delay(250);
    
    // First sign of life
    TelegramBot::send("🔌 Smart Plug Bot online");
    
    // Mark image as PENDING_VERIFY
    ota::begin();  // esp_ota_get_state_partition()
    
    // Display state
    display::showStatus(...);  // Cur: 1.3 / Prev: 1.2
}
```

Worst case: 50s before validation window opens (30s WiFi + 10s NTP + buffer).

If image crashes during `setup()`, bootloader finds partition in `PENDING_VERIFY` and rolls back automatically.

### Phase 7: Validation (Final)

```cpp
void ota::loop() {
    if (millis() - pending_since >= VERIFY_DELAY_MS) {  // 8000 ms
        int err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            Preferences prefs;
            prefs.putString("last_good_ver", APP_VERSION);
            is_validated = true;
        }
    }
}
```

If image restarts or crashes before 8s elapses, bootloader detects `PENDING_VERIFY` and reverts. This is exercised by sending `X` on serial.

## Partition State Machine

```
IMG_NEW
  (set by Update.end())
    │
    ├─── boot ──→ PENDING_VERIFY
    │            (esp_ota_get_state_partition)
    │                  │
    │                  ├─ +8s, no reboot → IMG_VALID (rollback cancelled)
    │                  │
    │                  └─ crash or reset → rollback to previous
    │                     (bootloader auto-detects PENDING_VERIFY)
```

The 8-second window is the **only guarantee** that an image is stable. Nothing auto-validates.

## Critical Timings

| Event | Timeout | Notes |
|-------|---------|-------|
| `WiFiManager::begin()` | 30 s | Tries up to 3 SSIDs, reboots if all fail |
| `configTime()` + NTP | 10 s | Soft failure (continues with wrong time) |
| Telegram poll interval | 2 s | Throttled; overhead is TLS handshake |
| OTA download | ∞ | Entire `loop()` blocks; WiFi may timeout on very slow links |
| Validation window | 8 s | Countdown starts in `ota::begin()`, **not on boot** |

Total time to validation in worst case: ~50 seconds.

## Two Different HTTP Channels

**Telegram → API:** `WiFiClientSecure` + `setInsecure()`
- TLS handshake every poll
- Certificate validation disabled
- High latency per message

**Firmware download:** `WiFiClient` plain
- No TLS, no encryption
- Direct binary stream
- URL must be `http://`, never `https://`

They don't share connection state. Telegram staying online doesn't guarantee the download succeeds.

## Error Codes

From `performHttpUpdate()`:

```cpp
#define UPDATE_OK                           1
#define UPDATE_FAIL_BEGIN                  -4
#define UPDATE_FAIL_END                    -5
#define UPDATE_FAIL_STREAM                 -6

int result = http.GET();
// -1: no WiFi (WiFi.status())
// -2: bad URL (http.begin)
// -3: unreachable (http.GET)
// (then Update.* calls for -4, -5, -6)
```

### Most Common Failures

| Code | Frequency | Fix |
|------|-----------|-----|
| -3 | Very high | Check IP (must be on same subnet). Verify Python server is running. Ping ESP32 from your PC. |
| -1 | Medium | WiFi disconnected or AP hidden. Check SSID list in secrets.hpp. |
| -4 | Low | Binary > 1.25 MB. Check firmware size in build output. |
| -5 | Low | Corrupted during download. Re-run `build_and_publish.sh`. |
| -2 | Rare | Passing `https://` URL when setup requires `http://`. |

## Rollback Mechanism

Triggered if the board resets or hard-crashes **before** `ota::loop()` validates.

```cpp
// In bootloader (Espressif esp_ota_ops.c)
if (otadata.ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    // Use previous partition instead
    select_previous_partition();
}
```

User-triggered rollback test:
1. Upload new firmware
2. Send `X` via serial within 8 seconds
3. Board crashes (undefined pointer dereference)
4. Bootloader detects `PENDING_VERIFY` and reverts
5. Previous firmware boots

Output:
```
[boot message from OLD firmware]
Rolled back to previous image
```

## Display Behavior

`display::showStatus()` runs once in `setup()`. When `ota::loop()` writes to NVS, screen doesn't update until next boot.

To show validation in real-time, call `display::showStatus()` from `ota::loop()` after validation succeeds.

## Testing OTA Without Rebooting

Send `U<url>` via serial monitor to trigger `performHttpUpdate()` directly:

```
Uhttp://192.168.1.100:8000/firmware.bin
```

No boot cycle, no WiFi reconnect. Useful for fast iteration.

## Constraints for Custom Code

- `loop()` runs continuously. Blocking calls will delay Telegram polling and OTA validation.
- WiFi callback handlers (`onConnect`, `onDisconnect`) may interrupt `loop()`.
- Allocate large buffers (>50 KB) carefully; heap is limited (~100 KB free during OTA).
- NVS (Preferences) is safe for custom data; NVS partition is separate from OTA.
- Telegram updates are rate-limited; if you flood `sendMessage()`, API will throttle.

## Deployment Notes

Before deploying to production:

- **Keep firmware under 1.25 MB** — OTA partition limit. Check size in build output.
- **Test rollback locally** — Send `X` via serial to verify the board recovers on crash.
- **Use HTTPS for images** — `http://` is fine for development; production should encrypt the link.
- **Guard the Telegram token** — Treat `secrets.hpp` as a credential. If exposed, regenerate in BotFather.
- **Pin app version** — Don't auto-increment; set `APP_VERSION` explicitly before each build.
