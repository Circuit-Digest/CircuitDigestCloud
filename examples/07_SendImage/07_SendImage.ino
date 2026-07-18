// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 07: Capture and upload an image from ESP32-CAM
//
// Press the BOOT button (GPIO 0) or send the REMOTE_CAPTURE control command to
// capture a frame from the camera and send it to the CircuitDigest dashboard.
//
// sendImage() uses the CircuitDigest Cloud HTTP API (not MQTT), so it needs an
// API key (cd_live_...) from the dashboard — different from the Connection Key.
//
// ── Board / camera selection ────────────────────────────────────────────────
// Pick ONE camera model below AND set the matching Arduino IDE board target:
//
//   CAMERA_MODEL_AI_THINKER     → Tools → Board → "AI Thinker ESP32-CAM"
//                                 Flash mode DIO, Partition "Huge APP (3MB No OTA)"
//   CAMERA_MODEL_ESP32S3_N16R8  → Tools → Board → "ESP32S3 Dev Module"
//                                 PSRAM "OPI PSRAM", Flash 16MB, Partition with OTA/APP room
//   CAMERA_MODEL_XIAO_ESP32S3   → Tools → Board → "XIAO_ESP32S3"
//                                 PSRAM enabled
// ────────────────────────────────────────────────────────────────────────────

#include <CircuitDigestCloud.h>

// ── SELECT YOUR BOARD (uncomment exactly one) ───────────────────────────────
#define CAMERA_MODEL_AI_THINKER
// #define CAMERA_MODEL_ESP32S3_N16R8
// #define CAMERA_MODEL_XIAO_ESP32S3
#include "camera.h"     // pin map + initCamera(), selected by the #define above
// ────────────────────────────────────────────────────────────────────────────

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxxxxxx"   // dashboard API key
#define REMOTE_CAPTURE "capture-1"                  // control variable slot (boolean catalog key)
// ────────────────────────────────────────────────────────────────────────────

// BOOT button is on GPIO 0 .
#define BOOT_BTN_PIN 0
#define DEBOUNCE_MS  50

CircuitDigestCloud CDcloud;

// Doubles as both the "press pending" flag and the debounce timestamp.
volatile uint32_t lastEdgeMs = 0;

// Set by the dashboard control callback; handled in loop().
bool remoteCapture = false;

// True from the moment a capture+upload starts until it finishes
volatile bool captureBusy = false;

// Runs in ISR context — only stamp the time.
void IRAM_ATTR onBootButton() {
  lastEdgeMs = millis();
}

// Control callback for REMOTE_CAPTURE. 
void handleCapture(CDValue value) {
  if (value.asBool() && !captureBusy) remoteCapture = true;
  CDcloud.publish(REMOTE_CAPTURE, 0.0f);
}

// Grab a frame and push it to the dashboard. Shared by the button and the
// remote-capture control command. Ignores the call if one is already running.
static void captureAndUpload() {
  if (captureBusy) return;          // already capturing — drop this trigger
  captureBusy = true;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    captureBusy = false;
    return;
  }
  Serial.printf("Captured %u bytes (%ux%u) — uploading...\n",
                fb->len, fb->width, fb->height);

  bool ok = CDcloud.sendImage(fb->buf, fb->len, "image/jpeg");
  esp_camera_fb_return(fb);

  Serial.printf("Upload: %s (err=%d)\n", ok ? "OK" : "FAILED", CDcloud.lastError());
  captureBusy = false;
}

void setup() {
  Serial.begin(115200);

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BTN_PIN), onBootButton, FALLING);

  if (!initCamera()) {
    Serial.println("Camera init failed — check wiring and board selection");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");

  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("Cloud connect failed — check credentials");
    while (true) delay(1000);
  }

  // Let the dashboard trigger a capture remotely.
  CDcloud.subscribe(REMOTE_CAPTURE, handleCapture);

  Serial.println("Ready — press BOOT or send REMOTE_CAPTURE to capture and upload");
}

void loop() {
  CDcloud.loop();

  // Button trigger: act only once the line has been quiet for DEBOUNCE_MS.
  if (lastEdgeMs && millis() - lastEdgeMs >= DEBOUNCE_MS) {
    lastEdgeMs = 0;
    // If it's no longer held, it was a bounce/glitch — discard.
    if (digitalRead(BOOT_BTN_PIN) == LOW) {
      Serial.println("Button pressed — capturing image...");
      captureAndUpload();
    }
  }

  // Remote trigger: set by the REMOTE_CAPTURE control callback.
  if (remoteCapture) {
    remoteCapture = false;
    Serial.println("Remote capture requested — capturing image...");
    captureAndUpload();
  }
}
