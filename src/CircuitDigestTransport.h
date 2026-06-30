// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once

// Board-specific WiFi + TLS client selection.
// This header is the only place that knows about board differences.
// Supported: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  typedef WiFiClientSecure CDSecureClient;
  #define CD_WIFI_CLASS        WiFiClass
  #define CD_HAS_SETINSECURE   1

#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
  typedef BearSSL::WiFiClientSecure CDSecureClient;
  #define CD_WIFI_CLASS        ESP8266WiFiClass
  #define CD_HAS_SETINSECURE   1

#elif defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
  // WiFiSSLClient uses the modem's built-in CA bundle — no setInsecure().
  // The CircuitDigest Cloud root CA (Amazon Root CA 1 / ISRG Root X1) must be in the modem bundle.
  typedef WiFiSSLClient CDSecureClient;
  #define CD_WIFI_CLASS        WiFiClass
  #define CD_HAS_SETINSECURE   0

#elif defined(ARDUINO_ARCH_RP2040)
  // Covers both Pico W and Pico 2 W (arduino-pico core).
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  typedef WiFiClientSecure CDSecureClient;
  #define CD_WIFI_CLASS        WiFiClass
  #define CD_HAS_SETINSECURE   1

#else
  #error "CircuitDigestCloud: unsupported board. Supported: ESP32, ESP8266, UNO R4 WiFi, Pico W, Pico 2 W."
#endif

// WL_WRONG_PASSWORD is missing from some WiFi libraries (e.g. ESP8266).
// Value 6 is consistent across all supported SDK versions.
#ifndef WL_WRONG_PASSWORD
  #define WL_WRONG_PASSWORD 6
#endif

// Apply insecure TLS (skip cert validation) on boards that support it.
// UNO R4 WiFi relies on the modem CA bundle — setInsecure() is not available.
inline void cdApplyTLS(CDSecureClient& client) {
#if CD_HAS_SETINSECURE
    client.setInsecure();
#else
    (void)client;
#endif
}

// Thin wrappers so CircuitDigestCloud.cpp can stay board-agnostic.
inline void cdWiFiBegin(const char* ssid, const char* pass) {
    WiFi.begin(ssid, pass);
}

inline bool cdWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

inline int cdWiFiStatus() {
    return (int)WiFi.status();
}

// Returns the local IP as a dotted-decimal string in a static buffer.
inline const char* cdWiFiLocalIP() {
    static char buf[16];
    IPAddress ip = WiFi.localIP();
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return buf;
}
