// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 07: Home Automation (4 Relays + 4 Buttons)
//
// Each relay can be toggled from the cloud dashboard OR from its physical
// button. Either way the dashboard stays in sync.
//
// Wiring:
//   Relay 1–4 : RELAY1_PIN … RELAY4_PIN  → relay module IN1–IN4
//   Button 1–4: BTN1_PIN … BTN4_PIN, other leg → GND (internal pull-up used)
//
// ── Porting to other boards ───────────────────────────────────────────────────
// Pin numbers and IRAM_ATTR below are for ESP32 DevKit. If you use a different
// board, make these changes:
//
// ESP8266 / other ESP32 variants:
//   Update pin numbers to match your board's GPIO layout.
//
// Arduino UNO R4 WiFi / Raspberry Pi Pico W / Pico 2 W:
//   1. Remove IRAM_ATTR from every btnISR line — it does not exist on these boards.
//      Change:  void IRAM_ATTR btnISR0() { ... }
//      To:      void btnISR0() { ... }
//   2. Update pin numbers to match your board's GPIO layout.
//      UNO R4 : not all pins support interrupts — check the UNO R4 pinout.
//      Pico W : all GP pins (0–28) support interrupts — use GP pin numbers.
// ────────────────────────────────────────────────────────────────────────────

#include <CircuitDigestCloud.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

// Variable keys — one Toggle widget per relay on the dashboard.
#define MyRelay1  "relay-1"
#define MyRelay2  "relay-2"
#define MyRelay3  "relay-3"
#define MyRelay4  "relay-4"
// ────────────────────────────────────────────────────────────────────────────

// ── Pin assignments — adjust for your board (see notes above) ────────────────
#define RELAY1_PIN  16
#define RELAY2_PIN  17
#define RELAY3_PIN  18
#define RELAY4_PIN  19

#define BTN1_PIN    25
#define BTN2_PIN    26
#define BTN3_PIN    27
#define BTN4_PIN    14
// ────────────────────────────────────────────────────────────────────────────

CircuitDigestCloud CDcloud;

const uint8_t RELAY_PINS[4] = { RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN };
const uint8_t BTN_PINS[4]   = { BTN1_PIN,   BTN2_PIN,   BTN3_PIN,   BTN4_PIN   };
const char*   SLOTS[4]      = { MyRelay1,   MyRelay2,   MyRelay3,   MyRelay4   };

bool relayState[4] = { false, false, false, false };

// ── Interrupt-based button detection ─────────────────────────────────────────
// IRAM_ATTR: ESP32/ESP8266 only — remove for UNO R4 / Pico W (see notes above).
volatile bool     btnFlag[4]         = { false, false, false, false };
volatile uint32_t lastInterruptMs[4] = { 0, 0, 0, 0 };

void IRAM_ATTR btnISR0() { uint32_t n = millis(); if (n - lastInterruptMs[0] > 50) { btnFlag[0] = true; lastInterruptMs[0] = n; } }
void IRAM_ATTR btnISR1() { uint32_t n = millis(); if (n - lastInterruptMs[1] > 50) { btnFlag[1] = true; lastInterruptMs[1] = n; } }
void IRAM_ATTR btnISR2() { uint32_t n = millis(); if (n - lastInterruptMs[2] > 50) { btnFlag[2] = true; lastInterruptMs[2] = n; } }
void IRAM_ATTR btnISR3() { uint32_t n = millis(); if (n - lastInterruptMs[3] > 50) { btnFlag[3] = true; lastInterruptMs[3] = n; } }
// ─────────────────────────────────────────────────────────────────────────────

// Single source of truth — drives the relay pin and updates the stored state.
void applyRelay(uint8_t i, bool on) {
  relayState[i] = on;
  digitalWrite(RELAY_PINS[i], on ? HIGH : LOW);
}

// ── Cloud → device callbacks ──────────────────────────────────────────────────
void onRelay1(float v) { applyRelay(0, (bool)v); }
void onRelay2(float v) { applyRelay(1, (bool)v); }
void onRelay3(float v) { applyRelay(2, (bool)v); }
void onRelay4(float v) { applyRelay(3, (bool)v); }
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    applyRelay(i, false);
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  attachInterrupt(digitalPinToInterrupt(BTN1_PIN), btnISR0, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN2_PIN), btnISR1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN3_PIN), btnISR2, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN4_PIN), btnISR3, FALLING);

  CDcloud.subscribe(MyRelay1, onRelay1);
  CDcloud.subscribe(MyRelay2, onRelay2);
  CDcloud.subscribe(MyRelay3, onRelay3);
  CDcloud.subscribe(MyRelay4, onRelay4);

  CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY);
}

void loop() {
  CDcloud.loop();

  // ── Physical buttons ──────────────────────────────────────────────────────
  for (uint8_t i = 0; i < 4; i++) {
    if (btnFlag[i]) {
      btnFlag[i] = false;
      applyRelay(i, !relayState[i]);
      CDcloud.publish(SLOTS[i], relayState[i] ? 1.0f : 0.0f);
      Serial.printf("Button %u → Relay %u %s\n", i + 1, i + 1, relayState[i] ? "ON" : "OFF");
    }
  }
}
