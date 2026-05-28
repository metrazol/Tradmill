// ============================================================
// Tradmill — shared Arduino sketch
// Board is selected in board_select.h.  This file contains no
// board-specific code; all display and hardware differences live
// in config.h, ui_gc9a01.cpp, and ui_zx2d80ce02s.cpp.
// ============================================================

#define FIRMWARE_VERSION "1.0.16"

#include <Arduino.h>
#include <lvgl.h>
#include "esp_log.h"
#include "config.h"
#include "treadmill.h"
#include "ui.h"

static const char *TAG = "Tradmill";

// ---- Rotary encoder ISR ----------------------------------------------------
// Quadrature decoding via 4-bit state table.  Runs on both CLK and DT edges.

static volatile int32_t encoderCount = 0;
static volatile uint8_t lastEncoded  = 0;

void IRAM_ATTR encoderISR() {
    uint8_t a       = digitalRead(ENCODER_CLK);
    uint8_t b       = digitalRead(ENCODER_DT);
    uint8_t encoded = (a << 1) | b;
    uint8_t sum     = (lastEncoded << 2) | encoded;

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderCount++;
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderCount--;

    lastEncoded = encoded;
}

// ---- Objects ---------------------------------------------------------------

static Treadmill treadmill;

static void onSpeedAdjust(int32_t delta) {
    treadmill.adjustSpeed(delta);
}

static void onStopButton() {
    treadmill.toggleRunning();
}

// ---- Setup -----------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000);                 // Give the Serial Monitor time to connect!
    Serial.setDebugOutput(true); // Force ESP_LOG messages to show up over USB
    ESP_LOGI(TAG, "setup start  PSRAM=%s size=%u heap=%u",
        psramFound() ? "found" : "MISSING",
        (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreeHeap());

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT,  INPUT_PULLUP);
    pinMode(ENCODER_SW,  INPUT_PULLUP);
    lastEncoded = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);
    
    // 1. INITIALIZE DISPLAY FIRST so it gets the LCD_CAM interrupts
    ui_init();
    ESP_LOGI(TAG, "ui_init OK");
    ui_set_speed_callback(onSpeedAdjust);
    ui_set_stop_callback(onStopButton);

    // 2. INITIALIZE TREADMILL (PWM/LEDC) SECOND
    treadmill.begin();
    ESP_LOGI(TAG, "treadmill OK");

    // 3. ATTACH ENCODER INTERRUPTS LAST
    attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_DT),  encoderISR, CHANGE);

    ESP_LOGI(TAG, "Tradmill v" FIRMWARE_VERSION " ready");
}

// ---- Loop ------------------------------------------------------------------

static uint32_t lastLvglTick      = 0;
static uint32_t lastDisplayUpdate = 0;
static uint32_t lastSwCheck       = 0;
static int32_t  lastEncoderCount  = 0;
static bool     lastSwPressed     = false;

void loop() {
    uint32_t now = millis();

    // Keep LVGL's internal clock in sync
    lv_tick_inc(now - lastLvglTick);
    lastLvglTick = now;

    // Encoder rotation → speed
    int32_t enc   = encoderCount;
    int32_t delta = enc - lastEncoderCount;
    if (delta != 0) {
        treadmill.adjustSpeed(delta);
        lastEncoderCount = enc;
    }

    // Encoder click → start / stop (edge-detected, 20 ms poll for debounce)
    if (now - lastSwCheck >= 20) {
        bool pressed = (digitalRead(ENCODER_SW) == LOW);
        if (pressed && !lastSwPressed) {
            treadmill.toggleRunning();
        }
        lastSwPressed = pressed;
        lastSwCheck   = now;
    }

    treadmill.update();

    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
        ui_update(
            treadmill.getSpeedMph(),
            treadmill.getInclineLevel(),
            treadmill.getElapsedSeconds(),
            treadmill.isSafetyTriggered(),
            treadmill.isRunning()
        );
        lastDisplayUpdate = now;
    }

    if (DEBUG) {
        Serial.printf("spd:%.1fmph inc:%d t:%us running:%d safety:%d\n",
            treadmill.getSpeedMph(),
            treadmill.getInclineLevel(),
            (unsigned)treadmill.getElapsedSeconds(),
            (int)treadmill.isRunning(),
            (int)treadmill.isSafetyTriggered());
    }

    lv_timer_handler();
    delay(5);
}
