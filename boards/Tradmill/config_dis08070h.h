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
// Speed PWM stays on a native LEDC GPIO (the MCP23017 cannot generate a clean
// 20 Hz signal).  Incline relays, incline buttons, and the safety key all move
// onto an MCP23017 I2C expander (see below).
#define SPEED_PIN         10  // PWM output → MC2100 speed input

// ---- MCP23017 I2C GPIO expander (incline relays + buttons + safety key) ----
// Dedicated I2C bus (Wire1), separate from the GT911 touch bus (SDA 19 / SCL 20).
// GPIO 17/18 are free now that the incline signals moved to the expander.
// The expander also restores the incline-down button this board previously
// lacked due to GPIO scarcity.
#define MCP_I2C_SDA          17
#define MCP_I2C_SCL          18
#define MCP_I2C_ADDR         0x20  // A0-A2 tied to GND
#define MCP_INCLINE_UP_PIN    0    // GPA0 → relay: incline up
#define MCP_INCLINE_DOWN_PIN  1    // GPA1 → relay: incline down
#define MCP_INCLINE_UP_BTN    8    // GPB0 ← active-low button (internal pull-up)
#define MCP_INCLINE_DOWN_BTN  9    // GPB1 ← active-low button (internal pull-up)
#define MCP_SAFETY_KEY_PIN   -1    // GPB2 ← safety key; set to 10 to enable, -1 to disable

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
