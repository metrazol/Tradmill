#pragma once
// ============================================================
// Board: CrowPanel 1.28" 240x240 round (GC9A01 driver, SPI)
// MCU:   ESP32-S3
// ============================================================

// ---- Display (GC9A01 via SPI) ----
#define SCREEN_SCLK  10
#define SCREEN_MOSI  11
#define SCREEN_CS     9
#define SCREEN_DC     3
#define SCREEN_RST   14
#define SCREEN_BL    46   // backlight — drive HIGH for full brightness
#define LCD_EN_PIN    1   // LCD 3.3V power enable — drive HIGH
#define SCREEN_W    240
#define SCREEN_H    240

// ---- Rotary encoder (built-in) ----
#define ENCODER_CLK  45
#define ENCODER_DT   42
#define ENCODER_SW   41   // click button, active-low (INPUT_PULLUP)

// ---- Treadmill motor control ----
// Speed PWM stays on a native LEDC GPIO (the MCP23017 cannot generate a clean
// 20 Hz signal).  Incline relays, incline buttons, and the safety key all move
// onto an MCP23017 I2C expander (see below).
// GPIO 15-21 are safe general-purpose pins on the ESP32-S3 WROOM module.
#define SPEED_PIN         15  // PWM output → MC2100 speed input

// ---- MCP23017 I2C GPIO expander (incline relays + buttons + safety key) ----
// Dedicated I2C bus (Wire1), separate from any display/touch bus.
// GPIO 20/21 are free now that the incline/safety signals moved to the expander.
#define MCP_I2C_SDA          21
#define MCP_I2C_SCL          20
#define MCP_I2C_ADDR         0x20  // A0-A2 tied to GND
#define MCP_INCLINE_UP_PIN    0    // GPA0 → relay: incline up
#define MCP_INCLINE_DOWN_PIN  1    // GPA1 → relay: incline down
#define MCP_INCLINE_UP_BTN    8    // GPB0 ← active-low button (internal pull-up)
#define MCP_INCLINE_DOWN_BTN  9    // GPB1 ← active-low button (internal pull-up)
#define MCP_SAFETY_KEY_PIN   -1    // GPB2 ← safety key; set to 10 to enable, -1 to disable

// ---- Speed / PWM ----
// 8-bit resolution gives div_num = 80M / (20 * 256) = 15625, which fits
// the ESP32-S3 LEDC prescaler range.
#define PWM_FREQ_HZ      20       // Hz — MC2100 expects 20 Hz
#define PWM_RESOLUTION    8       // bits → duty range 0–255
#define MAX_DUTY_PCT      0.55f   // 55% duty = full speed on MC2100
#define MAX_SPEED_MPH    12.0f
#define SPEED_STEP_MPH    0.5f   // belt speed change per encoder tick

// ---- Incline ----
#define MAX_INCLINE_LEVEL   15
#define MIN_INCLINE_LEVEL    0
#define INCLINE_DEBOUNCE_MS 300

// ---- Application ----
#define DISPLAY_UPDATE_MS  250
#define DEBUG              true
