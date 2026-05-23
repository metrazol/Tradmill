#pragma once
#include <stdint.h>

// Connects to WiFi and starts the ArduinoOTA service.
// Call once from setup() after Serial.begin().
// Safe to call when wifi_config.h is absent — becomes a no-op.
void connectivity_init();

// Pumps the OTA handler and posts a status update to Home Assistant
// every 5 seconds.  Call every loop iteration.
// Silently skips the POST if WiFi is not connected.
void connectivity_update(float speed_mph, uint32_t elapsed_sec, bool running);
