#include "connectivity.h"
#include <Arduino.h>

// The entire implementation is conditional on wifi_config.h existing.
// If the file is absent the two public functions compile as no-ops so
// the sketch builds and runs offline without any code changes.
#if __has_include("wifi_config.h")

#include "wifi_config.h"
#include "config.h"    // for DEBUG
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>

static uint32_t _last_post_ms      = 0;
static uint32_t _last_reconnect_ms = 0;

// ---- WiFi -------------------------------------------------------------------

static void wifi_connect() {
    Serial.printf("WiFi: connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(250);
        Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi: connected  IP %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi: timed out — running offline");
    }
}

// Non-blocking reconnect check called each loop.  Retries every 30 s.
static void ensure_wifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    uint32_t now = millis();
    if (now - _last_reconnect_ms < 30000) return;
    _last_reconnect_ms = now;

    Serial.println("WiFi: reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ---- Home Assistant webhook -------------------------------------------------

static void post_to_ha(float speed_mph, uint32_t elapsed_sec, bool running) {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClient  client;
    HTTPClient  http;

    if (!http.begin(client, HA_WEBHOOK_URL)) return;
    http.addHeader("Content-Type", "application/json");

    char body[96];
    snprintf(body, sizeof(body),
        "{\"speed_mph\":%.2f,\"elapsed_sec\":%u,\"running\":%s}",
        speed_mph, (unsigned)elapsed_sec, running ? "true" : "false");

    int code = http.POST((uint8_t *)body, strlen(body));
    http.end();

    if (DEBUG) Serial.printf("HA webhook: HTTP %d\n", code);
}

// ---- Public interface -------------------------------------------------------

void connectivity_init() {
    wifi_connect();

    ArduinoOTA.setHostname(OTA_HOSTNAME);
#ifdef OTA_PASSWORD
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.println("OTA: starting " + type + " update");
    });
    ArduinoOTA.onEnd([]()  { Serial.println("\nOTA: done — rebooting"); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        if (DEBUG) Serial.printf("OTA: %u%%\r", done * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("OTA error [%u]\n", err);
    });

    ArduinoOTA.begin();
    Serial.println("OTA: ready  hostname=" OTA_HOSTNAME);
}

void connectivity_update(float speed_mph, uint32_t elapsed_sec, bool running) {
    ensure_wifi();
    ArduinoOTA.handle();

    uint32_t now = millis();
    if (now - _last_post_ms >= 5000) {
        post_to_ha(speed_mph, elapsed_sec, running);
        _last_post_ms = now;
    }
}

// ---- No-ops when wifi_config.h is absent ------------------------------------
#else
void connectivity_init() {}
void connectivity_update(float, uint32_t, bool) {}
#endif
