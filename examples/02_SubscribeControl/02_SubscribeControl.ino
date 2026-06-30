// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 02: Receive a control from the dashboard
//
// Listens for a boolean toggle on the dashboard and drives LED_BUILTIN.
// The library auto-acknowledges each change so the dashboard widget confirms.
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#include <CircuitDigestCloud.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

// Variable key of the control variable on the dashboard (direction: output, type: boolean)
#define MyLight  "light-1"
// ────────────────────────────────────────────────────────────────────────────

CircuitDigestCloud CDcloud;

// Called whenever the dashboard writes to MyLight.
// value is 1.0 for ON (true) and 0.0 for OFF (false).
void onLight(float value) {
  bool on = (bool)value;
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
  Serial.print("Light → "); Serial.println(on ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  // Register control handler before begin().
  CDcloud.subscribe(MyLight, onLight);

  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();
}
