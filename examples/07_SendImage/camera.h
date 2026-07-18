// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// camera.h — Board-selectable pin maps + camera init for the CircuitDigestCloud
//            ESP32-CAM example.
//
// Select ONE board in the main sketch BEFORE including this file, e.g.:
//
//     #define CAMERA_MODEL_AI_THINKER
//     #include "camera.h"
//
// Supported boards:
//   CAMERA_MODEL_AI_THINKER      AI-Thinker ESP32-CAM        (OV2640)
//   CAMERA_MODEL_ESP32S3_N16R8   ESP32-S3-N16R8 CAM board    (OV3660)
//   CAMERA_MODEL_XIAO_ESP32S3    Seeed XIAO ESP32S3 Sense    (OV2640)

#pragma once

#include <Arduino.h>
#include "esp_camera.h"

// ── Pin maps ────────────────────────────────────────────────────────────────
#if defined(CAMERA_MODEL_AI_THINKER)
// AI-Thinker ESP32-CAM (OV2640)
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1   // not connected on most AI-Thinker boards
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26   // SDA
#define CAM_PIN_SIOC    27   // SCL
#define CAM_PIN_D7      35   // Y9
#define CAM_PIN_D6      34   // Y8
#define CAM_PIN_D5      39   // Y7
#define CAM_PIN_D4      36   // Y6
#define CAM_PIN_D3      21   // Y5
#define CAM_PIN_D2      19   // Y4
#define CAM_PIN_D1      18   // Y3
#define CAM_PIN_D0       5   // Y2
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#elif defined(CAMERA_MODEL_ESP32S3_N16R8)
// ESP32-S3-N16R8 CAM board (OV3660)
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD     4   // SDA
#define CAM_PIN_SIOC     5   // SCL
#define CAM_PIN_D7      16   // Y9
#define CAM_PIN_D6      17   // Y8
#define CAM_PIN_D5      18   // Y7
#define CAM_PIN_D4      12   // Y6
#define CAM_PIN_D3      10   // Y5
#define CAM_PIN_D2       8   // Y4
#define CAM_PIN_D1       9   // Y3
#define CAM_PIN_D0      11   // Y2
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

#elif defined(CAMERA_MODEL_XIAO_ESP32S3)
// Seeed XIAO ESP32S3 Sense (OV2640)
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40   // SDA
#define CAM_PIN_SIOC    39   // SCL
#define CAM_PIN_D7      48   // Y9
#define CAM_PIN_D6      11   // Y8
#define CAM_PIN_D5      12   // Y7
#define CAM_PIN_D4      14   // Y6
#define CAM_PIN_D3      16   // Y5
#define CAM_PIN_D2      18   // Y4
#define CAM_PIN_D1      17   // Y3
#define CAM_PIN_D0      15   // Y2
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

#else
#error "No camera model selected — #define one CAMERA_MODEL_* before including camera.h"
#endif

// ── Init ────────────────────────────────────────────────────────────────────
// inline so this definition can live in a header without ODR violations.
// Returns true on success. Selects max resolution when PSRAM is present.
inline bool initCamera() {
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

  // Use max res if PSRAM is available, otherwise fall back to VGA.
  // UXGA is valid for both OV2640 and OV3660; the OV3660 can go higher (QXGA)
  // if you want to bump it on the S3-N16R8 board.
  if (psramFound()) {
    cfg.frame_size   = FRAMESIZE_SVGA;   // 800 × 600 pixels
    cfg.jpeg_quality = 4;                // 0–63, lower = better quality
    cfg.fb_count     = 1;                // 1 at UXGA for stability
  } else {
    cfg.frame_size   = FRAMESIZE_VGA;    // 640×480 — max without PSRAM
    cfg.jpeg_quality = 4;
    cfg.fb_count     = 1;
  }

  return esp_camera_init(&cfg) == ESP_OK;
}
