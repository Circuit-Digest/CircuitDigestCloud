// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 01: Publish float sensor data
//
// Publishes a temperature reading every 5 seconds to a dashboard slot.
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.
// No WiFi or TLS setup needed — the library handles everything.

#include <CircuitDigestCloud.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"        // Physical Device ID (dashboard)
#define CONNECTION_KEY "your-connection-key"    // Connection Key    (dashboard)
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key           (dashboard)

// Variable key shown next to your variable on the dashboard, e.g. "temperature-1"
#define MyTemperature  "temperature-1"
// ────────────────────────────────────────────────────────────────────────────

CircuitDigestCloud CDcloud;

void setup() {
  Serial.begin(115200);
  // begin() connects WiFi then starts the MQTT state machine.
  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();   // drives WiFi reconnect + MQTT + auto heartbeat

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    float temperature = random(200, 300) / 10.0f;   // replace with real sensor
    CDcloud.publish(MyTemperature, temperature);
  }
}
