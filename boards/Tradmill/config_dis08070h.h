#pragma once
// ============================================================
// Board: Elecrow CrowPanel 7" DIS08070H (800x480 RGB + GT911 touch)
// MCU:   ESP32-S3-WROOM-1-N4R8  (4 MB flash, 8 MB Octal PSRAM)
//
// Arduino IDE settings required:
//   Board:           ESP32S3 Dev Module
//   PSRAM:           OPI PSRAM
//   Flash size:      4MB (N4R8 module — 16MB causes a boot loop)
//   Upload speed:    921600
//   USB CDC On Boot: Disabled  <-- REQUIRED.  Touch shares GPIO 19/20 with the
//                    USB PHY, which the firmware disables so the GT911 I2C bus
//                    works.  Logging then runs over UART0 (GPIO 43/44).
// ============================================================

// ---- Display (16-bit RGB parallel, DE mode) ----
#define SCREEN_W 800
#define SCREEN_H 480

// Blue[0-4], Green[0-5], Red[0-4] data pins
#define SCREEN_B0 15
#define SCREEN_B1 7
#define SCREEN_B2 6
#define SCREEN_B3 5
#define SCREEN_B4 4
#define SCREEN_G0 9
#define SCREEN_G1 46
#define SCREEN_G2 3
#define SCREEN_G3 8
#define SCREEN_G4 16
#define SCREEN_G5 1
#define SCREEN_R0 14
#define SCREEN_R1 21
#define SCREEN_R2 47
#define SCREEN_R3 48
#define SCREEN_R4 45

// Sync / control
#define SCREEN_HSYNC 39
#define SCREEN_VSYNC 40
#define SCREEN_PCLK 0
#define SCREEN_DE 41
#define SCREEN_BL 2  // PWM backlight

// ---- Touch controller (GT911 capacitive, via I2C port 0) ----
// The touch I2C bus shares GPIO 19/20 with the ESP32-S3 USB_SERIAL_JTAG PHY.
// The PHY must be disabled at boot (see ui_dis08070h.cpp) or the bus hangs with
// continuous I2C timeouts and touch never responds.
#define I2C_SDA 19
#define I2C_SCL 20
// INT/RST are wired through the on-board PCA9557 (I2C 0x18); a board pull-up
// releases the GT911 from reset on its own.  Leave both at -1 — driving GPIO 38
// (the INT line) is also the GT911 address strap and breaks touch.
#define TOUCH_INT -1
#define TOUCH_RST -1
// The GT911 I2C address is latched from the INT pin level on the rising edge of
// RST at power-on.  Both INT and RST are wired through the on-board PCA9557
// (I2C 0x18); ui_dis08070h.cpp performs a deterministic reset sequence that
// drives INT low while releasing RST, which selects 0x5D.  Keep this at 0x5D to
// match that sequence.  (The boot log prints an I2C scan so you can confirm.)
#define TOUCH_I2C_ADDR 0x5D

// ---- PCA9557 I/O expander (GT911 INT/RST control) ----
// On-board expander on the GT911 touch I2C bus (SDA 19 / SCL 20).
//   PCA9557 P0  → GT911 RST   (low = held in reset)
//   PCA9557 P1  → GT911 INT    (level latched as the I2C address on RST release)
#define PCA9557_I2C_ADDR 0x18
#define PCA9557_GT911_RST_BIT 0
#define PCA9557_GT911_INT_BIT 1

// ---- Rotary encoder ----
// Removed on this board: the UI is touch-only.  (Leaving ENCODER_* undefined
// compiles the encoder ISR/polling out of Tradmill.ino entirely.)

// ---- Treadmill motor control ----
// Speed PWM stays on a native LEDC GPIO (the MCP23017 cannot generate a clean
// 20 Hz signal).  Incline relays, incline buttons, and the safety key all move
// onto an MCP23017 I2C expander (see below).
#define SPEED_PIN 10  // PWM output → MC2100 speed input

// ---- MCP23017 I2C GPIO expander (incline relays + buttons + safety key) ----
// Dedicated I2C bus (Wire1), separate from the GT911 touch bus (SDA 19 / SCL 20).
// GPIO 17/18 are free now that the incline signals moved to the expander.
// The expander also restores the incline-down button this board previously
// lacked due to GPIO scarcity.
#define MCP_I2C_SDA 17
#define MCP_I2C_SCL 18
#define MCP_I2C_ADDR 0x20       // A0-A2 tied to GND
#define MCP_INCLINE_UP_PIN 0    // GPA0 → relay: incline up
#define MCP_INCLINE_DOWN_PIN 1  // GPA1 → relay: incline down
#define MCP_INCLINE_UP_BTN 8    // GPB0 ← active-low button (internal pull-up)
#define MCP_INCLINE_DOWN_BTN 9  // GPB1 ← active-low button (internal pull-up)
#define MCP_SAFETY_KEY_PIN -1   // GPB2 ← safety key; set to 10 to enable, -1 to disable

// ---- Speed / PWM ----
// Same ESP32-S3 LEDC peripheral as ZX2D80CE02S — identical timing.
#define PWM_FREQ_HZ 20
#define PWM_RESOLUTION 13
#define MAX_DUTY_PCT 0.55f
#define MAX_SPEED_MPH 6.0f
#define SPEED_STEP_MPH 0.25f

// ---- Incline ----
#define MAX_INCLINE_LEVEL 15
#define MIN_INCLINE_LEVEL 0
#define INCLINE_DEBOUNCE_MS 300

// ---- Application ----
#define DISPLAY_UPDATE_MS 250
#define DEBUG false
