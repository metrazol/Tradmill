#pragma once
#include <lvgl.h>

// Initialises display hardware, registers the LVGL driver, and builds all
// on-screen widgets.  Must be called once from setup() before the main loop.
void ui_init();

// Refreshes all on-screen values.  Call at DISPLAY_UPDATE_MS cadence.
// `running` controls the stop/start button label on boards that have one.
void ui_update(float speed_mph, int incline, uint32_t elapsed_sec,
               bool safety, bool running);

// Registers a callback invoked when the on-screen ±speed buttons are tapped
// (ZX2D80CE02S touch panel only).  Pass a function like:
//   void onSpeed(int32_t delta) { treadmill.adjustSpeed(delta); }
// On boards without a touch screen this is a no-op.
void ui_set_speed_callback(void (*cb)(int32_t delta));

// Registers a callback invoked when the on-screen stop/start button is tapped
// (ZX2D80CE02S touch panel only).  Pass a function like:
//   void onStop() { treadmill.toggleRunning(); }
// On boards without a touch screen this is a no-op.
void ui_set_stop_callback(void (*cb)());
