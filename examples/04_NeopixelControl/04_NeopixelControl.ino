// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 04: NeoPixel Color Control
//
// Receives a color from the dashboard's Color Picker widget and drives a
// NeoPixel (WS2812B) LED strip. The widget sends a packed 24-bit RGB integer:
// (r << 16) | (g << 8) | b  i.e. 0xRRGGBB, range 0–16777215.
// It arrives as a float — cast to long to recover the integer, then unpack.
//
// Requires: Adafruit NeoPixel library (install via Library Manager)
//
// Dashboard setup:
//   Add one variable (direction: output, type: float):
//     key "color-1"
//   Add a Color Picker widget and bind it to "color-1".
//
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#include <CircuitDigestCloud.h>
#include <Adafruit_NeoPixel.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

#define MyColor  "color-1"   // Color Picker variable key on the dashboard
// ────────────────────────────────────────────────────────────────────────────

// ── NeoPixel config — adjust for your wiring ────────────────────────────────
#define NEO_PIN    5    // Data pin connected to NeoPixel DIN
#define NEO_COUNT  8    // Number of LEDs in the strip
// ────────────────────────────────────────────────────────────────────────────

Adafruit_NeoPixel strip(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
CircuitDigestCloud CDcloud;

// Dashboard sends packed 24-bit RGB as a float (0xRRGGBB → 0–16777215).
// A 32-bit float has 24 bits of mantissa, so the cast to long is exact.
void onColor(float value) {
  long c    = (long)value;
  uint8_t r = (c >> 16) & 0xFF;
  uint8_t g = (c >>  8) & 0xFF;
  uint8_t b =  c        & 0xFF;

  strip.fill(strip.Color(r, g, b));
  strip.show();
  Serial.printf("NeoPixel → #%02X%02X%02X (R%u G%u B%u)\n", r, g, b, r, g, b);
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.fill(0);   // all LEDs off
  strip.show();

  CDcloud.subscribe(MyColor, onColor);

  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();
}
