// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 05: Slider and Servo
//
// Reads a slider value (0–100) from the dashboard, maps it to a servo angle
// (0–180°), moves the servo, and publishes the actual angle back to the
// dashboard so a gauge widget can display the current position.
//
// Dashboard setup:
//   analog-input-1 (direction: output, type: float) — bind a Slider widget (0–100)
//   analog-input-2 (direction: input,  type: float) — bind a Gauge widget (0–180)
//
// Requires: ESP32Servo library for ESP32, or Servo.h for other boards.
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#include <CircuitDigestCloud.h>
#if defined(ESP32)
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

#define MySlider  "analog-input-1"   // dashboard slider (0–100)  → device
#define MyAngle   "analog-input-2"   // servo angle     (0–180°)  → dashboard
// ────────────────────────────────────────────────────────────────────────────

// ── Servo config ─────────────────────────────────────────────────────────────
#define SERVO_PIN  13   // PWM-capable pin connected to servo signal wire
// ────────────────────────────────────────────────────────────────────────────

Servo servo;
CircuitDigestCloud CDcloud;

bool pendingPublish = false;
int  pendingAngle   = 0;

// Called when the dashboard slider changes (value 0–100).
void onSlider(float value) {
  // Clamp to valid range then map 0–100 → 0–180.
  float clamped = constrain(value, 0.0f, 100.0f);
  int angle = (int)(clamped * 1.8f);   // 100 * 1.8 = 180

  servo.write(angle);
  Serial.printf("Slider %.0f → servo %d°\n", clamped, angle);

  // Schedule publish for loop() — avoids calling publish() inside a callback.
  pendingAngle   = angle;
  pendingPublish = true;
}

void setup() {
  Serial.begin(115200);

  servo.attach(SERVO_PIN);
  servo.write(0);   // start at 0°

  CDcloud.subscribe(MySlider, onSlider);

  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();

  // Publish the new servo angle back to the dashboard.
  if (pendingPublish) {
    pendingPublish = false;
    CDcloud.publish(MyAngle, (float)pendingAngle);
  }
}
