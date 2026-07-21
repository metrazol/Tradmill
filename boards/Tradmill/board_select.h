#pragma once
// ============================================================
// HARDWARE SELECTION
//
// Arduino IDE: uncomment exactly one line below, then re-compile and re-flash.
//
// PlatformIO: the board is selected per-env via a -D build flag in
// platformio.ini (e.g. -D BOARD_DIS08070H), which takes precedence over the
// lines below.  Each #define is guarded so the build flag wins.
// ============================================================

#if !defined(BOARD_GC9A01) && !defined(BOARD_ZX2D80CE02S) && !defined(BOARD_DIS08070H) && !defined(BOARD_ESP32_2432S028R)
// #define BOARD_GC9A01      // CrowPanel 1.28" 240x240 round (GC9A01 via SPI)
#define BOARD_ZX2D80CE02S // Smartpanel ZX2D80CE02S 240x320 (ST7789 via 8080 parallel + touch)
//#define BOARD_DIS08070H      // Elecrow CrowPanel 7" 800x480 (RGB parallel + GT911 touch)
//#define BOARD_ESP32_2432S028R // AITRIP/Sunton CYD 2.8" 240x320 (ILI9341 SPI + XPT2046 resistive touch)
#endif
