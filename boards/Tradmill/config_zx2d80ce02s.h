#pragma once
// ============================================================
// Board: Smartpanel ZX2D80CE02S 240x320 (ST7789, 8080 8-bit parallel + FT5x06 touch)
// MCU:   ESP32-S3
// ============================================================

// ---- Display (ST7789 via 8080 8-bit parallel bus) ----
#define SCREEN_D0    16
#define SCREEN_D1    40
#define SCREEN_D2    15
#define SCREEN_D3     7
#define SCREEN_D4    41
#define SCREEN_D5    42
#define SCREEN_D6     2
#define SCREEN_D7     1
#define SCREEN_WR    17
#define SCREEN_RD    -1
#define SCREEN_DC    18
#define SCREEN_CS    -1
#define SCREEN_RST    3
#define SCREEN_BL    47   // PWM backlight — drive HIGH for full brightness
#define SCREEN_W    240
#define SCREEN_H    320

// ---- Touch controller (FT5x06 capacitive, via I2C) ----
#define I2C_SDA   8
#define I2C_SCL   9
#define TOUCH_INT 48

// ---- Rotary encoder (external, wired to expansion header) ----
#define ENCODER_CLK  13
#define ENCODER_DT   14
#define ENCODER_SW   21   // click button, active-low (INPUT_PULLUP)

// ---- Treadmill motor control ----
// Speed PWM stays on a native LEDC GPIO (the MCP23017 cannot generate a clean
// 20 Hz signal).  Incline relays, incline buttons, and the safety key all move
// onto an MCP23017 I2C expander (see below).
// Expansion header: GPIO 10-14, 21 (21 already used by ENCODER_SW above).
#define SPEED_PIN         10  // PWM output → MC2100 speed input

// ---- MCP23017 I2C GPIO expander (incline relays + buttons + safety key) ----
// Dedicated I2C bus (Wire1), separate from the FT5x06 touch bus (SDA 8 / SCL 9).
// Reuses the RS485 RXD/RTS pins (GPIO 4/5) for the expander's I2C bus — RS485
// is unavailable while the expander is in use.  Relay pins 11/12 are now free.
#define MCP_I2C_SDA           4    // RS485 RXD repurposed
#define MCP_I2C_SCL           5    // RS485 RTS repurposed
#define MCP_I2C_ADDR         0x20  // A0-A2 tied to GND
#define MCP_INCLINE_UP_PIN    0    // GPA0 → relay: incline up
#define MCP_INCLINE_DOWN_PIN  1    // GPA1 → relay: incline down
#define MCP_INCLINE_UP_BTN    8    // GPB0 ← active-low button (internal pull-up)
#define MCP_INCLINE_DOWN_BTN  9    // GPB1 ← active-low button (internal pull-up)
#define MCP_SAFETY_KEY_PIN   -1    // GPB2 ← safety key; set to 10 to enable, -1 to disable

// ---- Speed / PWM ----
// At 80 MHz APB, 8-bit resolution overflows the LEDC prescaler (div = 15625 > 1023).
// 13-bit gives div = 488, which fits.  The MC2100 only cares about frequency,
// not resolution, so extra bits cause no harm.
#define PWM_FREQ_HZ      20       // Hz — MC2100 expects 20 Hz
#define PWM_RESOLUTION   13       // bits → duty range 0–8191
#define MAX_DUTY_PCT      0.55f   // 55% duty = full speed on MC2100
// Speed is in km/h on this board (display reads "km/h").  Max belt speed 10 km/h.
#define MAX_SPEED_MPH    10.0f   // km/h — name kept for cross-board compatibility
#define SPEED_STEP_MPH    0.5f   // km/h change per encoder tick / touch button

// ---- Incline ----
#define MAX_INCLINE_LEVEL   15
#define MIN_INCLINE_LEVEL    0
#define INCLINE_DEBOUNCE_MS 300

// ---- Application ----
#define DISPLAY_UPDATE_MS  250
#define DEBUG              false
