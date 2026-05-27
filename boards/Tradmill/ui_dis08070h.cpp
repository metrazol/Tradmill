#include "board_select.h"
#ifdef BOARD_DIS08070H
// ============================================================
// Display + UI implementation for Elecrow CrowPanel DIS08070H
// (800x480, 16-bit RGB parallel, GT911 capacitive touch)
// ============================================================

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
// Panel_RGB and Bus_RGB are platform-specific and not auto-included by LovyanGFX.hpp
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include "config.h"
#include "ui.h"

// ---- LovyanGFX board configuration -----------------------------------------

class LGFX_DIS08070H : public lgfx::LGFX_Device {
    lgfx::Panel_RGB   _panel;
    lgfx::Bus_RGB     _bus;
    lgfx::Light_PWM   _light;
    lgfx::Touch_GT911 _touch;
public:
    LGFX_DIS08070H() {
        {
            auto cfg = _bus.config();
            cfg.freq_write        = 14000000;
            cfg.pin_hsync         = SCREEN_HSYNC;
            cfg.pin_vsync         = SCREEN_VSYNC;
            cfg.pin_pclk          = SCREEN_PCLK;
            cfg.pin_henable       = SCREEN_DE;
            cfg.panel             = &_panel;  // Bus_RGB requires back-pointer to panel
            cfg.hsync_front_porch = 10;
            cfg.hsync_pulse_width =  8;
            cfg.hsync_back_porch  = 50;
            cfg.vsync_front_porch = 10;
            cfg.vsync_pulse_width =  8;
            cfg.vsync_back_porch  = 20;
            // pin_data[]: B[0-4], G[0-5], R[0-4]
            cfg.pin_data[0]  = SCREEN_B0; cfg.pin_data[1]  = SCREEN_B1;
            cfg.pin_data[2]  = SCREEN_B2; cfg.pin_data[3]  = SCREEN_B3;
            cfg.pin_data[4]  = SCREEN_B4;
            cfg.pin_data[5]  = SCREEN_G0; cfg.pin_data[6]  = SCREEN_G1;
            cfg.pin_data[7]  = SCREEN_G2; cfg.pin_data[8]  = SCREEN_G3;
            cfg.pin_data[9]  = SCREEN_G4; cfg.pin_data[10] = SCREEN_G5;
            cfg.pin_data[11] = SCREEN_R0; cfg.pin_data[12] = SCREEN_R1;
            cfg.pin_data[13] = SCREEN_R2; cfg.pin_data[14] = SCREEN_R3;
            cfg.pin_data[15] = SCREEN_R4;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg           = _panel.config();
            cfg.memory_width   = SCREEN_W; cfg.panel_width  = SCREEN_W;
            cfg.memory_height  = SCREEN_H; cfg.panel_height = SCREEN_H;
            cfg.offset_x       = 0;        cfg.offset_y     = 0;
            cfg.offset_rotation = 0;
            _panel.config(cfg);
        }
        {
            auto cfg        = _light.config();
            cfg.pin_bl      = SCREEN_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {
            auto cfg            = _touch.config();
            cfg.x_min           = 0;  cfg.x_max = SCREEN_W - 1;
            cfg.y_min           = 0;  cfg.y_max = SCREEN_H - 1;
            cfg.pin_int         = TOUCH_INT;
            cfg.pin_rst         = TOUCH_RST;
            cfg.pin_sda         = I2C_SDA;
            cfg.pin_scl         = I2C_SCL;
            cfg.i2c_port        = 0;
            cfg.freq            = 400000;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

static LGFX_DIS08070H _gfx;

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
static lv_obj_t *_btn_minus;
static lv_obj_t *_btn_plus;
static lv_obj_t *_btn_stop;
static lv_obj_t *_lbl_stop;
static lv_obj_t *_lbl_safety;

#define ARC_START  150
#define ARC_END     30
#define ARC_MAX    ((int16_t)(MAX_SPEED_MPH * 10.0f))

static void (*_speed_cb)(int32_t delta) = nullptr;
static void (*_stop_cb)()               = nullptr;

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

// ---- Widget factory helpers -------------------------------------------------

static lv_obj_t *make_speed_btn(lv_obj_t *scr, const char *label,
                                 lv_align_t align, int32_t x_ofs,
                                 int32_t delta) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 200, 56);
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

// ---- Arc colour: blue → green → yellow → red with speed --------------------

static lv_color_t speed_colour(float mph) {
    if      (mph < 4.0f)  return lv_color_hex(0x00C8FF);
    else if (mph < 8.0f)  return lv_color_hex(0x00E060);
    else if (mph < 10.0f) return lv_color_hex(0xFFCC00);
    else                  return lv_color_hex(0xFF4444);
}

// ---- Public interface -------------------------------------------------------

void ui_set_speed_callback(void (*cb)(int32_t delta)) {
    _speed_cb = cb;
}

void ui_set_stop_callback(void (*cb)()) {
    _stop_cb = cb;
}

void ui_init() {
    _gfx.init();
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

    // Layout (800x480 landscape):
    //   Arc (300x300) centred in upper portion
    //   Speed number inside arc
    //   Time row  y≈330
    //   Stop button (200x56) centred  y≈400
    //   Slow / Fast buttons (200x56) bottom corners

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    _arc = lv_arc_create(scr);
    lv_obj_set_size(_arc, 300, 300);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, -70);
    lv_arc_set_range(_arc, 0, ARC_MAX);
    lv_arc_set_bg_angles(_arc, ARC_START, ARC_END);
    lv_arc_set_value(_arc, 0);
    lv_obj_remove_style(_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x00C8FF), LV_PART_INDICATOR);

    _lbl_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_white(), 0);
    lv_label_set_text(_lbl_speed, "0.0");
    lv_obj_align(_lbl_speed, LV_ALIGN_CENTER, 0, -95);

    _lbl_unit = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_unit, "mph");
    lv_obj_align_to(_lbl_unit, _lbl_speed, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    _lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_time, "00:00");
    lv_obj_align(_lbl_time, LV_ALIGN_BOTTOM_MID, 0, -148);

    _btn_stop = lv_btn_create(scr);
    lv_obj_set_size(_btn_stop, 200, 56);
    lv_obj_align(_btn_stop, LV_ALIGN_BOTTOM_MID, 0, -78);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0xAA2222), 0);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x881111), LV_STATE_PRESSED);
    lv_obj_set_style_radius(_btn_stop, 10, 0);
    lv_obj_add_event_cb(_btn_stop, btn_stop_cb, LV_EVENT_CLICKED, nullptr);

    _lbl_stop = lv_label_create(_btn_stop);
    lv_obj_set_style_text_font(_lbl_stop, &lv_font_montserrat_20, 0);
    lv_label_set_text(_lbl_stop, "Start");
    lv_obj_center(_lbl_stop);

    _btn_minus = make_speed_btn(scr, "- Slow",  LV_ALIGN_BOTTOM_LEFT,   20, -1);
    _btn_plus  = make_speed_btn(scr, "Fast +",  LV_ALIGN_BOTTOM_RIGHT, -20,  1);

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
    (void)incline;

    if (safety) {
        lv_obj_add_flag(_arc,          LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_speed,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_unit,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_stop,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_minus,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_plus,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_arc,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_speed,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_unit,    LV_OBJ_FLAG_HIDDEN);
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

    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(elapsed_sec / 60), (unsigned)(elapsed_sec % 60));
    lv_label_set_text(_lbl_time, buf);
}

#endif // BOARD_DIS08070H
