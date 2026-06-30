// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 03: Sensor + Control
//
// Publishes a temperature reading every 5 seconds AND receives a boolean
// control from the dashboard to toggle an output pin.
//
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#include <CircuitDigestCloud.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

// Sensor variable key (direction: input, type: float) — shows on dashboard as a gauge/chart.
#define MyTemperature  "temperature-1"

// Control variable key (direction: output, type: boolean) — driven by a Toggle widget.
#define MyLight        "light-1"
// ────────────────────────────────────────────────────────────────────────────

CircuitDigestCloud CDcloud;

// Called when the dashboard toggles the light.
// value is 1.0 for ON, 0.0 for OFF. Auto-acked back to the dashboard.
void onLight(float value) {
  bool on = (bool)value;
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
  Serial.print("Light → "); Serial.println(on ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  CDcloud.subscribe(MyLight, onLight);   // register control handler before begin()

  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    float temperature = random(200, 300) / 10.0f;   // replace with real sensor
    CDcloud.publish(MyTemperature, temperature);
  }
}
