# CircuitDigestCloud

Arduino library connecting devices to **CircuitDigest Cloud** over TLS MQTT.

WiFi and TLS are managed **inside** the library. Your sketch only needs credentials and slot keys — no `WiFiClientSecure`, no `setInsecure()`, no reconnect callbacks.

---

## Supported boards

| Board | Arduino core |
|---|---|
| ESP32 | [arduino-esp32](https://github.com/espressif/arduino-esp32) |
| ESP8266 | [esp8266/Arduino](https://github.com/esp8266/Arduino) |
| Arduino UNO R4 WiFi | [arduino-renesas](https://github.com/arduino/ArduinoCore-renesas) |
| Raspberry Pi Pico W | [arduino-pico](https://github.com/earlephilhower/arduino-pico) |
| Raspberry Pi Pico 2 W | [arduino-pico](https://github.com/earlephilhower/arduino-pico) |

---

## Installation

**Arduino IDE Library Manager** — search `CircuitDigestCloud` and install. Also install the dependency:
- [PubSubClient](https://github.com/knolleary/pubsubclient) ≥ 2.8

**PlatformIO:**
```ini
lib_deps =
  CircuitDigestCloud
  knolleary/PubSubClient@>=2.8
```

**Manual:** download ZIP, unzip into `~/Documents/Arduino/libraries/`.

---

## Credentials

All credentials come from the **CircuitDigest Cloud dashboard → device setup panel**:

| `begin()` parameter | Where to find it |
|---|---|
| `ssid` / `pass` | Your WiFi network |
| `deviceId` | Physical Device ID |
| `connectionKey` | Connection Key |
| `apiKey` | API key (`cd_live_…`) — only required for `sendImage()` |
| slot key (e.g. `"temperature-1"`) | Slot key shown on each dashboard widget |

---

## Quick start

```cpp
#include <CircuitDigestCloud.h>

const char* WIFI_SSID      = "your_ssid";
const char* WIFI_PASS      = "your_password";
const char* DEVICE_ID      = "your-device-id";
const char* CONNECTION_KEY = "your-connection-key";
const char* TEMPERATURE_SLOT = "temperature-1";
const char* LIGHT_SLOT       = "light-1";

CircuitDigestCloud cd;

void onLight(const char* key, float value) {
  // Boolean dashboard toggles arrive as 1.0 (on) or 0.0 (off).
  // Value is automatically acknowledged back to the dashboard.
  digitalWrite(LED_BUILTIN, value > 0.5f ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  cd.setDebug(&Serial);
  cd.subscribe(LIGHT_SLOT, onLight);   // register before begin()
  cd.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY);
}

void loop() {
  cd.loop();
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    cd.publish(TEMPERATURE_SLOT, 24.5f);
  }
}
```

---

## API reference

### `CircuitDigestCloud()`

No constructor arguments. The library creates and owns the TLS client internally.

---

### `begin()`

```cpp
bool begin(const char* ssid, const char* pass,
           const char* deviceId, const char* connectionKey,
           const char* apiKey = nullptr,
           uint32_t wifiTimeoutSec = 0);
```

Call once in `setup()`. Blocks until WiFi connects, then starts the MQTT connection in the background.

| Parameter | Default | Notes |
|---|---|---|
| `ssid` / `pass` | — | WiFi credentials |
| `deviceId` | — | From dashboard |
| `connectionKey` | — | From dashboard |
| `apiKey` | `nullptr` | Required only for `sendImage()` |
| `wifiTimeoutSec` | `0` | `0` = block forever; non-zero returns `false` / `CD_ERR_WIFI_CONNECT` after that many seconds |

**Block forever (default):**
```cpp
cd.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY);
```

**With timeout:**
```cpp
if (!cd.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, nullptr, 30)) {
  // WiFi did not connect within 30 seconds
}
```

---

### `loop()`

```cpp
void loop();
```

Call every iteration of `loop()`. Handles WiFi reconnect, MQTT connect/backoff, inbound message dispatch, and automatic heartbeat. Never blocks.

---

### `publish()`

```cpp
bool publish(const char* key, float value, bool retain = true);
```

Sends one float value to a dashboard slot. `key` is the slot key shown on the dashboard (e.g. `"temperature-1"`). Returns `false` if not currently connected.

---

### `subscribe()`

```cpp
bool subscribe(const char* key, CDControlCallback cb);  // per-slot
void subscribe(CDControlCallback cb);                   // global fallback
```

Registers a callback for dashboard-to-device control updates. Call before `begin()`. Multiple per-slot handlers are supported. The global fallback fires for any slot that has no dedicated handler.

**Callback signature:**
```cpp
void myHandler(const char* key, float value) { }
```

- `key` — slot key that triggered the update.
- `value` — new value as `float`. Boolean dashboard controls arrive as `1.0` (on) or `0.0` (off).
- The value is automatically published back to the slot when the callback returns (auto-ack), so the dashboard widget confirms state immediately.

---

### `sendImage()`

```cpp
bool sendImage(const uint8_t* data, size_t length,
               const char* contentType = "image/jpeg",
               const char* filename    = "capture.jpg");
```

Uploads an image to CircuitDigest Cloud over HTTPS. The library opens and manages the TLS connection. Supported types: `image/jpeg`, `image/png`, `image/gif`, `image/webp`. Maximum size: **5 MB**. Requires `apiKey` to be passed to `begin()`.

**ESP32-CAM usage:**
```cpp
camera_fb_t* fb = esp_camera_fb_get();
if (fb) {
  cd.sendImage(fb->buf, fb->len, "image/jpeg");
  esp_camera_fb_return(fb);
}
```

---

### `heartbeat()`

```cpp
bool heartbeat();
```

Sends one extra heartbeat over the existing MQTT connection. The library already sends a heartbeat automatically on connect and every 60 s — call this only if you want an additional on-demand beat (e.g. after completing a long blocking task).

---

### `connected()` / `disconnect()` / `lastError()` / `setDebug()`

```cpp
bool connected();              // true when MQTT session is established
void disconnect();             // cleanly close the MQTT session
CDError lastError();           // last error code — read immediately after a false return
void setDebug(Stream* stream); // debug output — prints to Serial by default
                               // pass nullptr to disable
                               // pass any other Stream to redirect (Serial1, Serial2, …)
```

Debug is **on by default** and prints `[CD]` status lines to `Serial`. To change port or disable, call `setDebug()` before `begin()`. The library never calls `.begin()` on the stream — that is always the user's responsibility.

```cpp
// Redirect to Serial1
Serial1.begin(115200);
cd.setDebug(&Serial1);
cd.begin(...);

// Disable debug entirely
cd.setDebug(nullptr);
cd.begin(...);
```

---

## Error codes

| Code | Meaning |
|---|---|
| `CD_OK` | No error |
| `CD_ERR_BAD_CREDENTIALS` | Missing or empty `deviceId` / `connectionKey` |
| `CD_ERR_WIFI_CONNECT` | WiFi did not connect within the specified timeout |
| `CD_ERR_NOT_CONNECTED` | `publish()` / `heartbeat()` called while MQTT is down |
| `CD_ERR_PUBLISH_FAILED` | PubSubClient rejected the publish |
| `CD_ERR_PAYLOAD_TOO_LONG` | Assembled JSON exceeded the internal buffer |
| `CD_ERR_TOPIC_TOO_LONG` | Topic string exceeded the internal buffer |
| `CD_ERR_INVALID_JSON` | Inbound message could not be parsed |
| `CD_ERR_NO_API_KEY` | `sendImage()` called without an API key |
| `CD_ERR_HTTP_CONNECT` | Could not open HTTPS connection |
| `CD_ERR_HTTP_STATUS` | Server returned a non-2xx status |
| `CD_ERR_BAD_ARGUMENT` | Called with a null pointer or zero-length buffer |
| `CD_ERR_OUT_OF_MEMORY` | `malloc` failed (unlikely on typical boards) |

---

## Reconnect behaviour

On WiFi or MQTT drop, `loop()` reconnects automatically with exponential backoff:
**1 s → 2 s → 4 s → … → 30 s**, then holds at 30 s until success. Backoff resets on each successful connect. `loop()` never blocks.

---

## Heartbeat

CircuitDigest Cloud tracks device liveness through heartbeats. The library sends one automatically on connect and then every **60 s** — no configuration needed for most use cases. Manual `cd.heartbeat()` is available for extra beats.

---

## TLS notes

| Board | TLS method | Certificate validation |
|---|---|---|
| ESP32 | `WiFiClientSecure::setInsecure()` | Disabled (dev convenience) |
| ESP8266 | `BearSSL::WiFiClientSecure::setInsecure()` | Disabled (dev convenience) |
| Pico W / Pico 2 W | `WiFiClientSecure::setInsecure()` | Disabled (dev convenience) |
| UNO R4 WiFi | `WiFiSSLClient` (modem CA bundle) | Enabled — modem bundle must include CircuitDigest Cloud's root CA |

For production on ESP32 / ESP8266 / Pico, use `setCACert()` to pin the CircuitDigest Cloud root CA instead of `setInsecure()` — this will be exposed as an optional setter in a future release.

---

## Examples

| Sketch | What it shows |
|---|---|
| `01_PublishSensor` | Publish a float sensor reading every 5 s — **start here** |
| `02_SubscribeControl` | Receive a boolean toggle from the dashboard and drive `LED_BUILTIN` |
| `03_SensorAndControl` | Publish sensor data **and** receive a control toggle in one sketch |
| `04_NeopixelControl` | Receive a color from the dashboard's Color Picker and drive a NeoPixel |
| `05_SliderAndServo` | Map a dashboard slider (0–100) to a servo angle |
| `06_SendImage` | Capture a frame from an ESP32-CAM and upload it over HTTPS |
| `07_HomeAutomation` | Control 4 relays from the cloud dashboard or physical buttons |

---

## Dependencies

- [PubSubClient](https://github.com/knolleary/pubsubclient) (MIT)

---

## License

MIT — Copyright (c) 2026 Jobit Joseph, Circuit Digest. See [LICENSE](LICENSE).
