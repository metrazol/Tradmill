#pragma once
// ============================================================
// Board: AITRIP / Sunton ESP32-2432S028R "Cheap Yellow Display" (CYD)
//        2.8" 240x320 ILI9341 (SPI) + XPT2046 resistive touch (SPI)
// MCU:   ESP32-WROOM-32  (4 MB flash, no PSRAM)
//
// Arduino IDE settings required:
//   Board:        ESP32 Dev Module
//   Flash size:   4MB
//   PSRAM:        Disabled (stock board has none)
//   Upload speed: 921600
//   USB chip:     CH340 — hold BOOT while plugging in if not detected
//
// The display and touch controller live on TWO separate SPI buses:
//   - ILI9341 display on HSPI  (SCLK 14 / MOSI 13 / MISO 12)
//   - XPT2046 touch   on VSPI  (CLK 25 / MOSI 32 / MISO 39)
// LovyanGFX drives both from a single LGFX device (see ui_esp32_2432s028r.cpp).
// ============================================================

// ---- Display (ILI9341 via SPI / HSPI) ----
#define SCREEN_SCLK  14
#define SCREEN_MOSI  13
#define SCREEN_MISO  12
#define SCREEN_CS    15
#define SCREEN_DC     2
#define SCREEN_RST   -1   // tied to the board reset line
#define SCREEN_BL    21   // PWM backlight — drive HIGH for full brightness
#define SCREEN_W    320   // landscape
#define SCREEN_H    240   // landscape

// ---- Touch controller (XPT2046 resistive, via SPI / VSPI) ----
#define TOUCH_SCLK   25
#define TOUCH_MOSI   32
#define TOUCH_MISO   39   // input-only GPIO
#define TOUCH_CS     33
#define TOUCH_IRQ    36   // input-only GPIO

// ---- Rotary encoder ----
// Removed on this board: the UI is touch-only (like DIS08070H).  The CYD breaks
// out very few free GPIOs, so leaving ENCODER_* undefined compiles the encoder
// ISR/polling out of Tradmill.ino entirely.

// ---- Treadmill motor control ----
// Speed PWM stays on a native LEDC GPIO (the MCP23017 cannot generate a clean
// 20 Hz signal).  Incline relays, incline buttons, and the safety key all move
// onto an MCP23017 I2C expander (see below).
// GPIO 27 is a free, PWM-capable pin broken out on the CN1 / P3 headers.
#define SPEED_PIN         27  // PWM output → MC2100 speed input

// ---- MCP23017 I2C GPIO expander (incline relays + buttons + safety key) ----
// Dedicated I2C bus (Wire1), separate from both SPI buses above.
// GPIO 22 is free on the CN1 (I2C) header; GPIO 5 is the SD-card CS pin, free
// whenever the on-board microSD slot is unused (this firmware never touches it).
#define MCP_I2C_SDA          22   // free on CN1 header
#define MCP_I2C_SCL           5    // SD_CS repurposed — SD card unused by firmware
#define MCP_I2C_ADDR         0x20  // A0-A2 tied to GND
#define MCP_INCLINE_UP_PIN    0    // GPA0 → relay: incline up
#define MCP_INCLINE_DOWN_PIN  1    // GPA1 → relay: incline down
#define MCP_INCLINE_UP_BTN    8    // GPB0 ← active-low button (internal pull-up)
#define MCP_INCLINE_DOWN_BTN  9    // GPB1 ← active-low button (internal pull-up)
#define MCP_SAFETY_KEY_PIN   -1    // GPB2 ← safety key; set to 10 to enable, -1 to disable

// ---- Speed / PWM ----
// Classic ESP32 (WROOM-32) LEDC.  At the 80 MHz APB clock, 8-bit resolution
// overflows the prescaler (div = 15625 > 1023); 13-bit gives div = 488, which
// fits.  The MC2100 only cares about frequency, not resolution.
#define PWM_FREQ_HZ      20       // Hz — MC2100 expects 20 Hz
#define PWM_RESOLUTION   13       // bits → duty range 0–8191
#define MAX_DUTY_PCT      0.55f   // 55% duty = full speed on MC2100
#define MAX_SPEED_MPH    12.0f
#define SPEED_STEP_MPH    0.5f   // belt speed change per on-screen button tap

// ---- Incline ----
#define MAX_INCLINE_LEVEL   15
#define MIN_INCLINE_LEVEL    0
#define INCLINE_DEBOUNCE_MS 300

// ---- Application ----
#define DISPLAY_UPDATE_MS  250
#define DEBUG              false
