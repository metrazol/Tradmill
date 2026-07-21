#include "board_select.h"
#ifdef BOARD_ESP32_2432S028R
// ============================================================
// Display + UI implementation for AITRIP / Sunton ESP32-2432S028R (CYD)
// (240x320, ILI9341 via SPI + XPT2046 resistive touch via a 2nd SPI bus)
// ============================================================

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include "config.h"
#include "ui.h"

// ---- LovyanGFX board configuration -----------------------------------------
// The CYD wires the display and the touch controller to TWO different SPI
// buses: the ILI9341 on HSPI and the XPT2046 on VSPI.  LovyanGFX handles this
// by giving the touch its own SPI host (spi_host = VSPI_HOST) while the panel
// bus uses HSPI_HOST.

class LGFX_ESP32_2432S028R : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_XPT2046 _touch;
public:
    LGFX_ESP32_2432S028R() {
        {
            auto cfg        = _bus.config();
            cfg.spi_host    = HSPI_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = SCREEN_SCLK;
            cfg.pin_mosi    = SCREEN_MOSI;
            cfg.pin_miso    = SCREEN_MISO;
            cfg.pin_dc      = SCREEN_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg          = _panel.config();
            cfg.pin_cs        = SCREEN_CS;
            cfg.pin_rst       = SCREEN_RST;
            cfg.pin_busy      = -1;
            cfg.memory_width  = SCREEN_W;  cfg.panel_width  = SCREEN_W;
            cfg.memory_height = SCREEN_H;  cfg.panel_height = SCREEN_H;
            cfg.offset_x      = 0;  cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable      = true;
            // The dual-USB CYD (ILI9341 variant) is wired with reversed R/B
            // polarity and inverted colours.  These two flags fix that.
            cfg.invert        = false;
            cfg.rgb_order     = false;
            cfg.dlen_16bit    = false;
            cfg.bus_shared    = false;
            _panel.config(cfg);
        }
        {
            auto cfg        = _light.config();
            cfg.pin_bl      = SCREEN_BL;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {
            auto cfg            = _touch.config();
            cfg.x_min           = 300;  cfg.x_max = 3900;   // raw XPT2046 range
            cfg.y_min           = 200;  cfg.y_max = 3700;
            cfg.pin_int         = TOUCH_IRQ;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            cfg.spi_host        = VSPI_HOST;   // touch on its OWN SPI bus
            cfg.freq            = 1000000;
            cfg.pin_sclk        = TOUCH_SCLK;
            cfg.pin_mosi        = TOUCH_MOSI;
            cfg.pin_miso        = TOUCH_MISO;
            cfg.pin_cs          = TOUCH_CS;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

static LGFX_ESP32_2432S028R _gfx;

// ---- LVGL glue --------------------------------------------------------------

static lv_disp_draw_buf_t _draw_buf;
static lv_color_t _buf1[SCREEN_W * 10];
static lv_color_t _buf2[SCREEN_W * 10];

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colors) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    _gfx.startWrite();
    _gfx.setAddrWindow(area->x1, area->y1, w, h);
    _gfx.writePixels((lgfx::rgb565_t *)colors, w * h);
    _gfx.endWrite();
    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t x, y;
    if (_gfx.getTouch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ---- LVGL widget handles ----------------------------------------------------

static lv_obj_t *_arc;
static lv_obj_t *_lbl_speed;
static lv_obj_t *_lbl_unit;
static lv_obj_t *_lbl_time;
static lv_obj_t *_lbl_incline;
static lv_obj_t *_btn_minus;
static lv_obj_t *_btn_plus;
static lv_obj_t *_btn_inc_up;
static lv_obj_t *_btn_inc_down;
static lv_obj_t *_btn_stop;
static lv_obj_t *_lbl_stop;
static lv_obj_t *_lbl_safety;

// Arc spans 240° centred at the bottom: 150° → 30° (clockwise).
// ARC_MAX derived from MAX_SPEED_MPH so it scales with calibration changes.
#define ARC_START  150
#define ARC_END     30
#define ARC_MAX    ((int16_t)(MAX_SPEED_MPH * 10.0f))

static void (*_speed_cb)(int32_t delta)    = nullptr;
static void (*_stop_cb)()                  = nullptr;
static void (*_incline_cb)(int32_t steps)  = nullptr;

// ---- Button callbacks -------------------------------------------------------

static void btn_speed_cb(lv_event_t *e) {
    if (_speed_cb) {
        int32_t delta = (int32_t)(intptr_t)lv_event_get_user_data(e);
        _speed_cb(delta);
    }
}

static void btn_stop_cb(lv_event_t *e) {
    if (_stop_cb) _stop_cb();
}

static void btn_incline_cb(lv_event_t *e) {
    if (_incline_cb) {
        int32_t steps = (int32_t)(intptr_t)lv_event_get_user_data(e);
        _incline_cb(steps);
    }
}

// ---- Widget factory helpers -------------------------------------------------

static lv_obj_t *make_speed_btn(lv_obj_t *scr, const char *label,
                                 lv_align_t align, int32_t x_ofs,
                                 int32_t delta) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 44);
    lv_obj_align(btn, align, x_ofs, -10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E6FCC), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0A4A99), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, btn_speed_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)delta);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    return btn;
}

// Compact square incline button (e.g. "+" / "−").
static lv_obj_t *make_incline_btn(lv_obj_t *scr, const char *label,
                                   lv_align_t align, int32_t x_ofs, int32_t y_ofs,
                                   int32_t steps) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 50, 44);
    lv_obj_align(btn, align, x_ofs, y_ofs);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xCC8A1E), 0);          // amber
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x99641A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, btn_incline_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)steps);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    return btn;
}

// ---- Arc colour: blue → green → yellow → red with speed (mph, 0–12) --------

static lv_color_t speed_colour(float mph) {
    if      (mph < 4.0f)  return lv_color_hex(0x00C8FF);  // blue-white: easy
    else if (mph < 8.0f)  return lv_color_hex(0x00E060);  // green: moderate
    else if (mph < 10.0f) return lv_color_hex(0xFFCC00);  // yellow: hard
    else                  return lv_color_hex(0xFF4444);  // red: max
}

// ---- Public interface -------------------------------------------------------

void ui_set_speed_callback(void (*cb)(int32_t delta)) {
    _speed_cb = cb;
}

void ui_set_stop_callback(void (*cb)()) {
    _stop_cb = cb;
}

void ui_set_incline_callback(void (*cb)(int32_t steps)) {
    _incline_cb = cb;
}

void ui_init() {
    _gfx.init();
    _gfx.setRotation(0);        // portrait 240x320
    _gfx.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, SCREEN_W * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Speed arc pushed toward the top so the incline row + three button rows
    // fit below it.
    _arc = lv_arc_create(scr);
    lv_obj_set_size(_arc, 180, 180);
    lv_obj_align(_arc, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_range(_arc, 0, ARC_MAX);
    lv_arc_set_bg_angles(_arc, ARC_START, ARC_END);
    lv_arc_set_value(_arc, 0);
    lv_obj_remove_style(_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x00C8FF), LV_PART_INDICATOR);

    _lbl_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_white(), 0);
    lv_label_set_text(_lbl_speed, "0.0");
    lv_obj_align_to(_lbl_speed, _arc, LV_ALIGN_CENTER, 0, -8);

    _lbl_unit = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_unit, "mph");
    lv_obj_align_to(_lbl_unit, _lbl_speed, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // Incline level readout (centre) flanked by the incline − / + buttons.
    _lbl_incline = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_incline, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_incline, lv_color_hex(0xFFCC00), 0);
    lv_label_set_text(_lbl_incline, "Incline 0");
    lv_obj_align(_lbl_incline, LV_ALIGN_BOTTOM_MID, 0, -170);

    _btn_inc_down = make_incline_btn(scr, "-", LV_ALIGN_BOTTOM_LEFT,   10, -160, -1);
    _btn_inc_up   = make_incline_btn(scr, "+", LV_ALIGN_BOTTOM_RIGHT, -10, -160,  1);

    // Elapsed time
    _lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_time, "00:00");
    lv_obj_align(_lbl_time, LV_ALIGN_BOTTOM_MID, 0, -118);

    // Stop / start toggle button (centre row)
    _btn_stop = lv_btn_create(scr);
    lv_obj_set_size(_btn_stop, 120, 44);
    lv_obj_align(_btn_stop, LV_ALIGN_BOTTOM_MID, 0, -62);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0xAA2222), 0);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x881111), LV_STATE_PRESSED);
    lv_obj_set_style_radius(_btn_stop, 10, 0);
    lv_obj_add_event_cb(_btn_stop, btn_stop_cb, LV_EVENT_CLICKED, nullptr);

    _lbl_stop = lv_label_create(_btn_stop);
    lv_obj_set_style_text_font(_lbl_stop, &lv_font_montserrat_20, 0);
    lv_label_set_text(_lbl_stop, "Start");
    lv_obj_center(_lbl_stop);

    // Speed ± buttons (bottom row)
    _btn_minus = make_speed_btn(scr, "- Slow",  LV_ALIGN_BOTTOM_LEFT,   10, -1);
    _btn_plus  = make_speed_btn(scr, "Fast +",  LV_ALIGN_BOTTOM_RIGHT, -10,  1);

    // Safety overlay (hidden until a safety event)
    _lbl_safety = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_safety, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_safety, lv_color_hex(0xFF4444), 0);
    lv_label_set_text(_lbl_safety, "!! STOPPED !!\nSet speed to 0");
    lv_obj_set_style_text_align(_lbl_safety, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lbl_safety, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
}

void ui_update(float speed_mph, int incline, uint32_t elapsed_sec,
               bool safety, bool running) {
    if (safety) {
        lv_obj_add_flag(_arc,          LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_speed,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_unit,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_incline,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_inc_up,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_inc_down, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_stop,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_minus,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_plus,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_arc,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_speed,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_unit,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_incline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_inc_up,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_inc_down,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_stop,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_minus,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_plus,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_lbl_safety,    LV_OBJ_FLAG_HIDDEN);

    if (running) {
        lv_label_set_text(_lbl_stop, "Stop");
        lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0xAA2222), 0);
    } else {
        lv_label_set_text(_lbl_stop, "Start");
        lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x226622), 0);
    }

    lv_arc_set_value(_arc, (int16_t)(speed_mph * 10));
    lv_obj_set_style_arc_color(_arc, speed_colour(speed_mph), LV_PART_INDICATOR);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", speed_mph);
    lv_label_set_text(_lbl_speed, buf);

    snprintf(buf, sizeof(buf), "Incline %d", incline);
    lv_label_set_text(_lbl_incline, buf);

    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(elapsed_sec / 60), (unsigned)(elapsed_sec % 60));
    lv_label_set_text(_lbl_time, buf);
}

#endif // BOARD_ESP32_2432S028R
