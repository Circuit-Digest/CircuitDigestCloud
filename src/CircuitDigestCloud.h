// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include "CircuitDigestTransport.h"
#include "CircuitDigestTypes.h"

// Control callback: invoked when the dashboard writes to a registered slot.
//   value — the new value as a float (booleans arrive as 1.0 / 0.0)
// Auto-ack: the value is published back to the slot automatically.
typedef void (*CDControlCallback)(float value);

struct CDControl {
    const char*       key;
    CDControlCallback callback;
    CDControl*        next;
};

class CircuitDigestCloud {
public:
    CircuitDigestCloud();
    ~CircuitDigestCloud();

    // Mandatory: call in setup() before loop().
    // Blocks until WiFi connects, then arms the MQTT state machine.
    // wifiTimeoutSec: 0 (default) = wait forever; any other value = give up after that many seconds.
    bool begin(const char* ssid,           const char* pass,
               const char* deviceId,       const char* connectionKey,
               const char* apiKey          = nullptr,
               uint32_t    wifiTimeoutSec  = 0);

    // Call every iteration of loop() — drives WiFi reconnect + MQTT + heartbeat.
    void loop();

    // Publish one float telemetry reading to a dashboard slot.
    bool publish(const char* key, float value, bool retain = true);

    // Register a callback for a specific control slot (dashboard → device).
    // Call before begin(). Multiple slots supported; each with its own callback.
    bool subscribe(const char* key, CDControlCallback cb);

    // Global fallback — fires for any control update with no per-slot handler.
    void subscribe(CDControlCallback cb);

    // Upload an image (e.g. ESP32-CAM frame) to CircuitDigest Cloud over HTTPS.
    // Uses an internally managed TLS client. Requires apiKey passed to begin().
    bool sendImage(const uint8_t* data, size_t length,
                   const char* contentType = "image/jpeg",
                   const char* filename    = "capture.jpg");

    // Send one extra heartbeat over the existing MQTT connection.
    bool heartbeat();

    bool    connected();
    void    disconnect();
    CDError lastError();
    void    setDebug(Stream* stream);

private:
    CDSecureClient _net;
    PubSubClient   _pubsub;

    const char* _ssid;
    const char* _pass;
    const char* _deviceId;
    const char* _connKey;
    const char* _apiKey;

    char     _host[64];
    uint16_t _port;

    char*    _topicBase;
    uint16_t _topicBaseLen;

    uint16_t _bufferSize;
    uint32_t _heartbeatInterval;
    uint32_t _lastHeartbeatMs;
    bool     _initialized;
    uint32_t _reqSeq;

    Stream*  _debug;
    CDError  _lastError;

    enum State : uint8_t {
        CD_STATE_DISCONNECTED,
        CD_STATE_BACKOFF,
        CD_STATE_CONNECTING,
        CD_STATE_CONNECTED
    } _state;

    uint32_t _backoffNextMs;
    uint16_t _backoffSeconds;

    CDControl*        _controlHead;
    CDControlCallback _globalControlCb;

    char _topicScratch  [CD_TOPIC_BUFFER_SIZE];
    char _submitScratch [CD_SUBMIT_BUFFER_SIZE];
    char _keyScratch    [CD_INBOUND_STRING_BUFFER];

    static CircuitDigestCloud* _instance;
    static void _staticCallback(char* topic, uint8_t* payload, unsigned int len);

    void _onMqttMessage(char* topic, uint8_t* payload, unsigned int len);
    bool _attemptConnect();
    void _onConnected();
    void _scheduleBackoff();
    void _resetTransport();

    bool _publishSubmit (const char* key, float value, bool retain);
    bool _publishHeartbeat();
    int  _readHttpStatus(CDSecureClient& c);

    void _logf(const char* fmt, ...);
};
