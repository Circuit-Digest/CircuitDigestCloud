// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 06: Capture and upload an image from ESP32-CAM
//
// Press the BOOT button (GPIO 0) to capture a frame from the camera and
// send it to the CircuitDigest Cloud dashboard.
//
// sendImage() uses the CircuitDigest Cloud HTTP API (not MQTT), so it needs an
// API key (cd_live_...) from the dashboard — different from the Connection Key.
//
// Board target : AI-Thinker ESP32-CAM (or any ESP32-CAM variant).
// In Arduino IDE  → Tools → Board → ESP32 Arduino → AI Thinker ESP32-CAM
// Flash mode     → DIO,  Partition scheme → Huge APP (3MB No OTA).
//
// Pin mapping below matches the AI-Thinker module.
// Change the #define block if you use a different ESP32-CAM board.

#include <CircuitDigestCloud.h>
#include "esp_camera.h"

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxxxxxx"   // dashboard API key
// ────────────────────────────────────────────────────────────────────────────

// ── Camera pin definitions (AI-Thinker ESP32-CAM) ───────────────────────────
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1   // not connected on most AI-Thinker boards
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22
// ────────────────────────────────────────────────────────────────────────────

// BOOT button is on GPIO 0 (active LOW).
#define BOOT_BTN_PIN 0

CircuitDigestCloud CDcloud;

volatile bool captureRequested = false;

// Runs in ISR context — only set a flag, do nothing heavy.
void IRAM_ATTR onBootButton() {
  captureRequested = true;
}

static bool initCamera() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_D0;
  cfg.pin_d1       = CAM_PIN_D1;
  cfg.pin_d2       = CAM_PIN_D2;
  cfg.pin_d3       = CAM_PIN_D3;
  cfg.pin_d4       = CAM_PIN_D4;
  cfg.pin_d5       = CAM_PIN_D5;
  cfg.pin_d6       = CAM_PIN_D6;
  cfg.pin_d7       = CAM_PIN_D7;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  // Use UXGA (max res) if PSRAM is available, otherwise fall back to VGA.
  if (psramFound()) {
    cfg.frame_size   = FRAMESIZE_UXGA;   // 1600×1200 — OV2640 maximum
    cfg.jpeg_quality = 4;               // 0–63, lower = better quality
    cfg.fb_count     = 1;               // 1 at UXGA for stability
  } else {
    cfg.frame_size   = FRAMESIZE_VGA;    // 640×480 — max without PSRAM
    cfg.jpeg_quality = 4;
    cfg.fb_count     = 1;
  }

  return esp_camera_init(&cfg) == ESP_OK;
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

  Serial.println("Ready — press BOOT to capture and upload");
}

void loop() {
  CDcloud.loop();

  if (captureRequested) {
    captureRequested = false;

    // Simple software debounce — button must still be held after 50 ms.
    delay(50);
    if (digitalRead(BOOT_BTN_PIN) != LOW) return;

    Serial.println("Button pressed — capturing image...");

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Capture failed");
    } else {
      Serial.printf("Captured %u bytes (%ux%u) — uploading...\n",
                    fb->len, fb->width, fb->height);

      bool ok = CDcloud.sendImage(fb->buf, fb->len, "image/jpeg");
      esp_camera_fb_return(fb);

      Serial.printf("Upload: %s (err=%d)\n", ok ? "OK" : "FAILED", CDcloud.lastError());
    }
  }
}
