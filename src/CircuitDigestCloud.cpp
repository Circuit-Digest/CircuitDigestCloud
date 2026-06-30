// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#include "CircuitDigestCloud.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static const uint16_t CD_MQTT_PORT_TLS    = 8883;
static const char     CD_TOPIC_PREFIX[]   = "$anedya/device/";
static const char     CD_DEFAULT_HOST[]   = "mqtt." CD_DEFAULT_REGION ".anedya.io";
static const char     CD_API_HOST[]       = "www.circuitdigest.cloud";
static const uint16_t CD_API_PORT         = 443;

CircuitDigestCloud* CircuitDigestCloud::_instance = nullptr;

// ── Construction ─────────────────────────────────────────────────────────────

CircuitDigestCloud::CircuitDigestCloud()
    : _pubsub(_net),
      _ssid(nullptr), _pass(nullptr),
      _deviceId(nullptr), _connKey(nullptr), _apiKey(nullptr),
      _port(CD_MQTT_PORT_TLS),
      _topicBase(nullptr), _topicBaseLen(0),
      _bufferSize(CD_DEFAULT_BUFFER_SIZE),
      _heartbeatInterval(CD_DEFAULT_HEARTBEAT_S), _lastHeartbeatMs(0),
      _initialized(false), _reqSeq(0),
      _debug(&Serial), _lastError(CD_OK),
      _state(CD_STATE_DISCONNECTED),
      _backoffNextMs(0), _backoffSeconds(1),
      _controlHead(nullptr), _globalControlCb(nullptr)
{
    snprintf(_host, sizeof(_host), "%s", CD_DEFAULT_HOST);
    _instance = this;
}

CircuitDigestCloud::~CircuitDigestCloud() {
    free(_topicBase);
    CDControl* c = _controlHead;
    while (c) { CDControl* n = c->next; delete c; c = n; }
    if (_instance == this) _instance = nullptr;
}

// ── Configuration ─────────────────────────────────────────────────────────────

void CircuitDigestCloud::setDebug(Stream* stream) { _debug = stream; }
CDError CircuitDigestCloud::lastError() { return _lastError; }

// ── begin() ──────────────────────────────────────────────────────────────────

bool CircuitDigestCloud::begin(const char* ssid, const char* pass,
                                const char* deviceId, const char* connectionKey,
                                const char* apiKey, uint32_t wifiTimeoutSec) {
    _lastError = CD_OK;
    if (!ssid || !pass || !deviceId || !*deviceId || !connectionKey || !*connectionKey) {
        _lastError = CD_ERR_BAD_CREDENTIALS;
        _logf("[CD] begin: missing credentials");
        return false;
    }
    _ssid     = ssid;
    _pass     = pass;
    _deviceId = deviceId;
    _connKey  = connectionKey;
    _apiKey   = apiKey;

    // Connect WiFi — blocks until connected.
    // If wifiTimeoutSec > 0, gives up after that many seconds.
    cdWiFiBegin(_ssid, _pass);
    _logf("[CD] connecting WiFi (SSID: %s)", _ssid);
    uint32_t wStart    = millis();
    uint32_t timeoutMs = wifiTimeoutSec * 1000UL;
    bool wrongPassSeen = false;
    while (!cdWiFiConnected()) {
        if (wifiTimeoutSec > 0 && (millis() - wStart) >= timeoutMs) {
            if (_debug) _debug->println();
            _lastError = CD_ERR_WIFI_CONNECT;
            _logf("[CD] WiFi timeout after %us — check SSID/password and signal", wifiTimeoutSec);
            return false;
        }
        if (!wrongPassSeen && cdWiFiStatus() == WL_WRONG_PASSWORD) {
            wrongPassSeen = true;
            if (_debug) _debug->println();
            _logf("[CD] wrong WiFi password — fix credentials and reset the device");
        }
        if (_debug) _debug->print('.');
        delay(200);
    }
    if (_debug) _debug->println();
    _logf("[CD] WiFi connected (IP: %s)", cdWiFiLocalIP());

    free(_topicBase); _topicBase = nullptr;
    size_t blen = strlen(CD_TOPIC_PREFIX) + strlen(_deviceId) + 1;
    _topicBase = (char*)malloc(blen);
    if (!_topicBase) { _lastError = CD_ERR_OUT_OF_MEMORY; return false; }
    snprintf(_topicBase, blen, "%s%s", CD_TOPIC_PREFIX, _deviceId);
    _topicBaseLen = (uint16_t)(blen - 1);

    _pubsub.setServer(_host, _port);
    _pubsub.setBufferSize(_bufferSize);
    _pubsub.setCallback(_staticCallback);
    if (_heartbeatInterval > 0 && _heartbeatInterval <= 0xFFFF)
        _pubsub.setKeepAlive((uint16_t)_heartbeatInterval);

    _state = CD_STATE_DISCONNECTED;
    _backoffSeconds = 1;
    _backoffNextMs  = 0;
    _initialized = true;

    _logf("[CD] begin: host=%s:%u base=%s", _host, _port, _topicBase);
    return true;
}

// ── loop() ───────────────────────────────────────────────────────────────────

void CircuitDigestCloud::loop() {
    if (!_initialized) return;

    // Reconnect WiFi if dropped.
    if (!cdWiFiConnected()) {
        if (_state == CD_STATE_CONNECTED) {
            _logf("[CD] WiFi lost → reconnecting");
            _pubsub.disconnect();
            _state = CD_STATE_DISCONNECTED;
        }
        cdWiFiBegin(_ssid, _pass);
        delay(200);
        return;
    }

    if (_state == CD_STATE_CONNECTED && !_pubsub.connected()) {
        _logf("[CD] MQTT lost → DISCONNECTED");
        _pubsub.disconnect();
        _resetTransport();
        delay(250);
        _state = CD_STATE_DISCONNECTED;
    }

    if (_state == CD_STATE_DISCONNECTED) {
        _state = CD_STATE_CONNECTING;
        if (!_attemptConnect()) {
            _scheduleBackoff();
            _state = CD_STATE_BACKOFF;
        }
    } else if (_state == CD_STATE_BACKOFF) {
        if ((uint32_t)(millis() - _backoffNextMs) < 0x80000000UL) {
            _state = CD_STATE_CONNECTING;
            if (!_attemptConnect()) {
                _scheduleBackoff();
                _state = CD_STATE_BACKOFF;
            }
        }
    } else if (_state == CD_STATE_CONNECTED) {
        _pubsub.loop();
        if (_heartbeatInterval > 0) {
            uint32_t periodMs = (_heartbeatInterval < 5 ? 5 : _heartbeatInterval) * 1000UL;
            if ((uint32_t)(millis() - _lastHeartbeatMs) >= periodMs) {
                _publishHeartbeat();
                _lastHeartbeatMs = millis();
            }
        }
    }
}

// ── Connection ───────────────────────────────────────────────────────────────

bool CircuitDigestCloud::connected() {
    return _state == CD_STATE_CONNECTED && _pubsub.connected();
}

void CircuitDigestCloud::disconnect() {
    if (_pubsub.connected()) _pubsub.disconnect();
    _resetTransport();
    _state = CD_STATE_DISCONNECTED;
    _backoffSeconds = 1;
    _backoffNextMs  = 0;
    _logf("[CD] disconnect: session closed");
}

void CircuitDigestCloud::_resetTransport() {
    _net.stop();
    cdApplyTLS(_net);
}

bool CircuitDigestCloud::_attemptConnect() {
    _pubsub.disconnect();
    _resetTransport();
    delay(250);
    _logf("[CD] connecting MQTT %s:%u as %s ...", _host, _port, _deviceId);
    bool ok = _pubsub.connect(_deviceId, _deviceId, _connKey);
    if (ok) {
        _state = CD_STATE_CONNECTED;
        _onConnected();
        _logf("[CD] connected");
    } else {
        _logf("[CD] connect failed (state=%d)", _pubsub.state());
    }
    return ok;
}

void CircuitDigestCloud::_onConnected() {
    snprintf(_topicScratch, sizeof(_topicScratch), "%s/valuestore/updates/json", _topicBase);
    _pubsub.subscribe(_topicScratch, 1);

    snprintf(_topicScratch, sizeof(_topicScratch), "%s/errors", _topicBase);
    _pubsub.subscribe(_topicScratch, 1);
    snprintf(_topicScratch, sizeof(_topicScratch), "%s/response", _topicBase);
    _pubsub.subscribe(_topicScratch, 1);

    _publishHeartbeat();
    _lastHeartbeatMs = millis();
    _backoffSeconds = 1;
}

void CircuitDigestCloud::_scheduleBackoff() {
    _backoffNextMs = millis() + (uint32_t)_backoffSeconds * 1000UL;
    _logf("[CD] backoff %us", _backoffSeconds);
    _backoffSeconds = (_backoffSeconds * 2 > CD_BACKOFF_MAX_S)
                      ? CD_BACKOFF_MAX_S : _backoffSeconds * 2;
}

// ── Publish ───────────────────────────────────────────────────────────────────

bool CircuitDigestCloud::_publishSubmit(const char* key, float value, bool retain) {
    if (!_pubsub.connected()) { _lastError = CD_ERR_NOT_CONNECTED; return false; }
    if (!key || !*key)        { _lastError = CD_ERR_BAD_ARGUMENT;  return false; }

    int n = snprintf(_topicScratch, sizeof(_topicScratch),
                     "%s/submitdata/json", _topicBase);
    if (n <= 0 || (size_t)n >= sizeof(_topicScratch)) {
        _lastError = CD_ERR_TOPIC_TOO_LONG; return false;
    }

    int p = snprintf(_submitScratch, sizeof(_submitScratch),
        "{\"reqID\":\"%lu\",\"data\":[{\"variable\":\"%s\",\"value\":%g,\"timestamp\":0}]}",
        (unsigned long)(++_reqSeq), key, value);
    if (p <= 0 || (size_t)p >= sizeof(_submitScratch)) {
        _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false;
    }

    bool ok = _pubsub.publish(_topicScratch, (const uint8_t*)_submitScratch,
                               (unsigned int)p, retain);
    if (!ok) _lastError = CD_ERR_PUBLISH_FAILED;
    _logf("[CD] submit %s=%g [%s]", key, value, ok ? "ok" : "fail");
    return ok;
}

bool CircuitDigestCloud::publish(const char* key, float value, bool retain) {
    _lastError = CD_OK;
    return _publishSubmit(key, value, retain);
}

bool CircuitDigestCloud::_publishHeartbeat() {
    if (!_pubsub.connected()) { _lastError = CD_ERR_NOT_CONNECTED; return false; }
    int n = snprintf(_topicScratch, sizeof(_topicScratch), "%s/heartbeat/json", _topicBase);
    if (n <= 0 || (size_t)n >= sizeof(_topicScratch)) {
        _lastError = CD_ERR_TOPIC_TOO_LONG; return false;
    }
    bool ok = _pubsub.publish(_topicScratch, (const uint8_t*)"{}", 2, false);
    if (!ok) _lastError = CD_ERR_PUBLISH_FAILED;
    _logf("[CD] heartbeat [%s]", ok ? "ok" : "fail");
    return ok;
}

bool CircuitDigestCloud::heartbeat() {
    _lastError = CD_OK;
    return _publishHeartbeat();
}

// ── Controls / subscribe ──────────────────────────────────────────────────────

bool CircuitDigestCloud::subscribe(const char* key, CDControlCallback cb) {
    _lastError = CD_OK;
    if (!key || !cb) { _lastError = CD_ERR_BAD_ARGUMENT; return false; }
    // Update existing entry if key already registered.
    for (CDControl* c = _controlHead; c; c = c->next) {
        if (strcmp(c->key, key) == 0) { c->callback = cb; return true; }
    }
    CDControl* c = new CDControl();
    if (!c) { _lastError = CD_ERR_OUT_OF_MEMORY; return false; }
    c->key      = key;
    c->callback = cb;
    c->next     = _controlHead;
    _controlHead = c;
    return true;
}

void CircuitDigestCloud::subscribe(CDControlCallback cb) {
    _globalControlCb = cb;
}

// ── Inbound MQTT ─────────────────────────────────────────────────────────────

void CircuitDigestCloud::_staticCallback(char* topic, uint8_t* payload, unsigned int len) {
    if (_instance) _instance->_onMqttMessage(topic, payload, len);
}

// Minimal float + key extractor — avoids CDJson/CDValue dependency.
// Finds a JSON string field by name, writes it into out (null-terminated).
static bool extractString(const uint8_t* d, unsigned int len, const char* field,
                           char* out, size_t cap) {
    char needle[40];
    int nn = snprintf(needle, sizeof(needle), "\"%s\"", field);
    if (nn <= 0 || (size_t)nn >= sizeof(needle)) return false;
    size_t needleLen = (size_t)nn;
    for (unsigned int i = 0; i + needleLen <= len; i++) {
        if (memcmp(d + i, needle, needleLen) != 0) continue;
        unsigned int j = i + needleLen;
        while (j < len && (d[j] == ' ' || d[j] == '\t')) j++;
        if (j >= len || d[j] != ':') continue;
        j++;
        while (j < len && (d[j] == ' ' || d[j] == '\t')) j++;
        if (j >= len || d[j] != '"') continue;
        j++;
        size_t n = 0;
        while (j < len && d[j] != '"') {
            if (n + 1 < cap) out[n++] = (char)d[j];
            j++;
        }
        out[n] = 0;
        return true;
    }
    return false;
}

// Finds a JSON number field by name, parses it as float.
static bool extractFloat(const uint8_t* d, unsigned int len, const char* field, float* out) {
    char needle[40];
    int nn = snprintf(needle, sizeof(needle), "\"%s\"", field);
    if (nn <= 0 || (size_t)nn >= sizeof(needle)) return false;
    size_t needleLen = (size_t)nn;
    for (unsigned int i = 0; i + needleLen <= len; i++) {
        if (memcmp(d + i, needle, needleLen) != 0) continue;
        unsigned int j = i + needleLen;
        while (j < len && (d[j] == ' ' || d[j] == '\t')) j++;
        if (j >= len || d[j] != ':') continue;
        j++;
        while (j < len && (d[j] == ' ' || d[j] == '\t')) j++;
        // accept number, true, false
        if (j >= len) return false;
        char nbuf[32]; size_t n = 0;
        while (j < len && n < sizeof(nbuf) - 1 &&
               (d[j] == '-' || d[j] == '.' || d[j] == 'e' || d[j] == 'E' ||
                d[j] == '+' || (d[j] >= '0' && d[j] <= '9'))) {
            nbuf[n++] = (char)d[j++];
        }
        // boolean literals
        if (n == 0) {
            if (j + 4 <= len && memcmp(d + j, "true",  4) == 0) { *out = 1.0f; return true; }
            if (j + 5 <= len && memcmp(d + j, "false", 5) == 0) { *out = 0.0f; return true; }
            return false;
        }
        nbuf[n] = 0;
        *out = (float)atof(nbuf);
        return true;
    }
    return false;
}

void CircuitDigestCloud::_onMqttMessage(char* topic, uint8_t* payload, unsigned int len) {
    if (strncmp(topic, _topicBase, _topicBaseLen) != 0) return;
    const char* rest = topic + _topicBaseLen;

    if (strncmp(rest, "/errors", 7) == 0 || strncmp(rest, "/response", 9) == 0) {
        _logf("[CD] %s: %.*s", rest + 1, (int)len, (const char*)payload);
        return;
    }
    if (strncmp(rest, "/valuestore/updates/json", 24) != 0) return;

    _logf("[CD] valuestore: %.*s", (int)len, (const char*)payload);

    if (!extractString(payload, len, "key",   _keyScratch, sizeof(_keyScratch))) {
        _lastError = CD_ERR_INVALID_JSON;
        _logf("[CD] valuestore: 'key' missing");
        return;
    }
    float value = 0.0f;
    if (!extractFloat(payload, len, "value", &value)) {
        _lastError = CD_ERR_INVALID_JSON;
        _logf("[CD] valuestore: 'value' missing or not numeric");
        return;
    }

    _logf("[CD] control key=%s value=%g", _keyScratch, value);

    // Find per-slot handler first, then fall back to global.
    CDControlCallback cb = nullptr;
    for (CDControl* c = _controlHead; c; c = c->next) {
        if (strcmp(c->key, _keyScratch) == 0) { cb = c->callback; break; }
    }
    if (!cb) cb = _globalControlCb;

    if (cb) {
        cb(value);
        // Auto-ack: publish the same value back so the dashboard widget confirms.
        _publishSubmit(_keyScratch, value, true);
    } else {
        _logf("[CD] no handler for control %s", _keyScratch);
    }
}

// ── Image upload (HTTPS) ──────────────────────────────────────────────────────

bool CircuitDigestCloud::sendImage(const uint8_t* data, size_t length,
                                    const char* contentType, const char* filename) {
    _lastError = CD_OK;
    if (!_deviceId) {
        _lastError = CD_ERR_BAD_CREDENTIALS;
        _logf("[CD] sendImage: credentials missing");
        return false;
    }
    if (!_apiKey || !*_apiKey) {
        _lastError = CD_ERR_NO_API_KEY;
        _logf("[CD] sendImage: apiKey not provided to begin()");
        return false;
    }
    if (!data || length == 0) {
        _lastError = CD_ERR_BAD_ARGUMENT;
        _logf("[CD] sendImage: empty image");
        return false;
    }
    if (!contentType || !*contentType) contentType = "image/jpeg";
    if (!filename    || !*filename)    filename    = "capture.jpg";

    // Multipart body parts.
    static const char BOUNDARY[] = "----CDImageBoundary";
    char head[256];
    int headLen = snprintf(head, sizeof(head),
        "--%s\r\nContent-Disposition: form-data; name=\"image\"; filename=\"%s\"\r\n"
        "Content-Type: %s\r\n\r\n",
        BOUNDARY, filename, contentType);
    char tail[48];
    int tailLen = snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", BOUNDARY);
    if (headLen <= 0 || headLen >= (int)sizeof(head) || tailLen <= 0) {
        _lastError = CD_ERR_BAD_ARGUMENT; return false;
    }

    size_t contentLength = (size_t)headLen + length + (size_t)tailLen;

    // Build a local HTTPS client for this upload only (avoids keeping a second
    // TLS session open permanently).
    CDSecureClient https;
    cdApplyTLS(https);

    if (!https.connect(CD_API_HOST, CD_API_PORT)) {
        _lastError = CD_ERR_HTTP_CONNECT;
        _logf("[CD] sendImage: connect %s failed", CD_API_HOST);
        return false;
    }

    char line[160];
    https.print("POST /api/v1/devices/");
    https.print(_deviceId);
    https.print("/image HTTP/1.1\r\n");
    snprintf(line, sizeof(line), "Host: %s\r\n", CD_API_HOST); https.print(line);
    https.print("Authorization: "); https.print(_apiKey); https.print("\r\n");
    snprintf(line, sizeof(line),
             "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
    https.print(line);
    snprintf(line, sizeof(line), "Content-Length: %lu\r\n",
             (unsigned long)contentLength);
    https.print(line);
    https.print("Connection: close\r\n\r\n");
    https.write((const uint8_t*)head, (size_t)headLen);

    const size_t CHUNK = 1024;
    size_t sent = 0;
    while (sent < length) {
        size_t want  = (length - sent < CHUNK) ? (length - sent) : CHUNK;
        size_t wrote = https.write(data + sent, want);
        if (wrote == 0) {
            https.stop();
            _lastError = CD_ERR_PUBLISH_FAILED;
            _logf("[CD] sendImage: write stalled at %u/%u",
                  (unsigned)sent, (unsigned)length);
            return false;
        }
        sent += wrote;
    }
    https.write((const uint8_t*)tail, (size_t)tailLen);
    https.flush();

    int status = _readHttpStatus(https);
    https.stop();

    if (status >= 200 && status < 300) {
        _logf("[CD] sendImage: OK (HTTP %d, %u bytes)", status, (unsigned)length);
        return true;
    }
    _lastError = CD_ERR_HTTP_STATUS;
    _logf("[CD] sendImage: HTTP %d", status);
    return false;
}

int CircuitDigestCloud::_readHttpStatus(CDSecureClient& c) {
    char buf[48]; size_t i = 0;
    uint32_t start = millis(); bool sawData = false;
    while (millis() - start < 10000) {
        int ch = c.read();
        if (ch < 0) {
            if (sawData && !c.connected() && !c.available()) break;
            delay(2); continue;
        }
        sawData = true;
        if (ch == '\n') break;
        if (ch != '\r' && i < sizeof(buf) - 1) buf[i++] = (char)ch;
    }
    buf[i] = 0;
    const char* sp = strchr(buf, ' ');
    return sp ? atoi(sp + 1) : -1;
}

// ── Debug ─────────────────────────────────────────────────────────────────────

void CircuitDigestCloud::_logf(const char* fmt, ...) {
    if (!_debug) return;
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _debug->println(buf);
}
