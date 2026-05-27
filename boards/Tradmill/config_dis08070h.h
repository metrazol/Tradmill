#pragma once
// ============================================================
// Board: Elecrow CrowPanel 7" DIS08070H (800x480 RGB + GT911 touch)
// MCU:   ESP32-S3-WROOM-1-N4R8  (4 MB flash, 8 MB Octal PSRAM)
//
// Arduino IDE settings required:
//   Board:      ESP32S3 Dev Module
//   PSRAM:      OPI PSRAM
//   Flash size: 4MB (or 16MB if your unit has it)
//   Upload speed: 921600
// ============================================================

// ---- Display (16-bit RGB parallel, DE mode) ----
#define SCREEN_W  800
#define SCREEN_H  480

// Blue[0-4], Green[0-5], Red[0-4] data pins
#define SCREEN_B0  15
#define SCREEN_B1   7
#define SCREEN_B2   6
#define SCREEN_B3   5
#define SCREEN_B4   4
#define SCREEN_G0   9
#define SCREEN_G1  46
#define SCREEN_G2   3
#define SCREEN_G3   8
#define SCREEN_G4  16
#define SCREEN_G5   1
#define SCREEN_R0  14
#define SCREEN_R1  21
#define SCREEN_R2  47
#define SCREEN_R3  48
#define SCREEN_R4  45

// Sync / control
#define SCREEN_HSYNC  39
#define SCREEN_VSYNC  40
#define SCREEN_PCLK    0
#define SCREEN_DE     41
#define SCREEN_BL      2   // PWM backlight

// ---- Touch controller (GT911 capacitive, via I2C port 0) ----
#define I2C_SDA   19
#define I2C_SCL   20
#define TOUCH_INT  38   // verify against board schematic — may be NC on some revisions
#define TOUCH_RST  -1

// ---- Rotary encoder ----
// GPIO 10-13 (SD card) and 17-18, 42 (I2S audio) are the only free pins
// when display + touch occupy the rest.  Adjust to match your wiring.
#define ENCODER_CLK  42
#define ENCODER_DT   13
#define ENCODER_SW   12

// ---- Treadmill motor control ----
#define SPEED_PIN         10  // PWM output → MC2100 speed input
#define INCLINE_UP_PIN    11  // Relay: incline up
#define INCLINE_DOWN_PIN  17  // Relay: incline down
#define INCLINE_UP_BTN    18  // Active-low button
#define INCLINE_DOWN_BTN  -1  // Not enough free pins — incline-down button not wired
#define SAFETY_KEY_PIN    -1

// ---- Speed / PWM ----
// Same ESP32-S3 LEDC peripheral as ZX2D80CE02S — identical timing.
#define PWM_FREQ_HZ      20
#define PWM_RESOLUTION   13
#define MAX_DUTY_PCT      0.55f
#define MAX_SPEED_MPH     6.0f
#define SPEED_STEP_MPH    0.25f

// ---- Incline ----
#define MAX_INCLINE_LEVEL   15
#define MIN_INCLINE_LEVEL    0
#define INCLINE_DEBOUNCE_MS 300

// ---- Application ----
#define DISPLAY_UPDATE_MS  250
#define DEBUG              false
