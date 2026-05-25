#include "board_select.h"
#ifdef BOARD_ZX2D80CE02S
// ============================================================
// Display + UI implementation for Smartpanel ZX2D80CE02S
// (240x320, ST7789 via 8080 8-bit parallel, FT5x06 touch)
// ============================================================

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include "config.h"
#include "ui.h"

// ---- LovyanGFX board configuration -----------------------------------------

class LGFX_ZX2D80CE02S : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_Parallel8 _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_FT5x06  _touch;
public:
    LGFX_ZX2D80CE02S() {
        {
            auto cfg       = _bus.config();
            cfg.port       = 0;
            cfg.freq_write = 20000000;
            cfg.pin_wr = SCREEN_WR; cfg.pin_rd = SCREEN_RD; cfg.pin_rs = SCREEN_DC;
            cfg.pin_d0 = SCREEN_D0; cfg.pin_d1 = SCREEN_D1;
            cfg.pin_d2 = SCREEN_D2; cfg.pin_d3 = SCREEN_D3;
            cfg.pin_d4 = SCREEN_D4; cfg.pin_d5 = SCREEN_D5;
            cfg.pin_d6 = SCREEN_D6; cfg.pin_d7 = SCREEN_D7;
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
            cfg.offset_rotation = 2;
            cfg.invert        = true;
            cfg.rgb_order     = true;
            cfg.dlen_16bit    = false;
            cfg.bus_shared    = false;
            _panel.config(cfg);
        }
        {
            auto cfg        = _light.config();
            cfg.pin_bl      = SCREEN_BL;
            cfg.invert      = false;
            cfg.freq        = 21111;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {
            auto cfg            = _touch.config();
            cfg.x_min           = 0;  cfg.x_max = SCREEN_W - 1;
            cfg.y_min           = 0;  cfg.y_max = SCREEN_H - 1;
            cfg.pin_int         = TOUCH_INT;
            cfg.pin_sda         = I2C_SDA;
            cfg.pin_scl         = I2C_SCL;
            cfg.i2c_port        = 1;
            cfg.freq            = 400000;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

static LGFX_ZX2D80CE02S _gfx;

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
static lv_obj_t *_lbl_incline;
static lv_obj_t *_lbl_time;
static lv_obj_t *_btn_minus;
static lv_obj_t *_btn_plus;
static lv_obj_t *_lbl_safety;

// Arc spans 240° centred at the bottom: 150° → 30° (clockwise)
#define ARC_START  150
#define ARC_END     30
#define ARC_MAX    120   // represents 12.0 mph (value = mph * 10)

static void (*_speed_cb)(int32_t delta) = nullptr;

// ---- Speed button callback --------------------------------------------------

static void btn_speed_cb(lv_event_t *e) {
    if (_speed_cb) {
        int32_t delta = (int32_t)(intptr_t)lv_event_get_user_data(e);
        _speed_cb(delta);
    }
}

static lv_obj_t *make_speed_btn(lv_obj_t *scr, const char *label,
                                 lv_align_t align, int32_t x_ofs,
                                 int32_t delta) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 50);
    lv_obj_align(btn, align, x_ofs, -8);
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

void ui_init() {
    // Init display via LovyanGFX
    _gfx.init();
    _gfx.fillScreen(TFT_BLACK);

    // Init LVGL and register display driver
    lv_init();
    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, SCREEN_W * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &_draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch input device
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    // Build widgets
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Speed arc (centred in the upper portion of the 240x320 screen)
    _arc = lv_arc_create(scr);
    lv_obj_set_size(_arc, 210, 210);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, -30);
    lv_arc_set_range(_arc, 0, ARC_MAX);
    lv_arc_set_bg_angles(_arc, ARC_START, ARC_END);
    lv_arc_set_value(_arc, 0);
    lv_obj_remove_style(_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x00C8FF), LV_PART_INDICATOR);

    // Large speed value in the centre of the arc
    _lbl_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_white(), 0);
    lv_label_set_text(_lbl_speed, "0.0");
    lv_obj_align(_lbl_speed, LV_ALIGN_CENTER, 0, -50);

    _lbl_unit = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_unit, "mph");
    lv_obj_align_to(_lbl_unit, _lbl_speed, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // Incline level at the top of the screen
    _lbl_incline = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_incline, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_incline, lv_color_hex(0xFFCC00), 0);
    lv_label_set_text(_lbl_incline, "Incline: 0");
    lv_obj_align(_lbl_incline, LV_ALIGN_TOP_MID, 0, 10);

    // Elapsed time between arc and touch buttons
    _lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_time, "00:00");
    lv_obj_align(_lbl_time, LV_ALIGN_BOTTOM_MID, 0, -68);

    // Touch speed buttons at the bottom
    _btn_minus = make_speed_btn(scr, "-  Slow",  LV_ALIGN_BOTTOM_LEFT,   10, -1);
    _btn_plus  = make_speed_btn(scr, "Fast  +",  LV_ALIGN_BOTTOM_RIGHT, -10,  1);

    // Safety overlay (hidden until a safety event)
    _lbl_safety = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_safety, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_safety, lv_color_hex(0xFF4444), 0);
    lv_label_set_text(_lbl_safety, "!! STOPPED !!\nSet speed to 0");
    lv_obj_set_style_text_align(_lbl_safety, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lbl_safety, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
}

void ui_update(float speed_mph, int incline, uint32_t elapsed_sec, bool safety) {
    if (safety) {
        lv_obj_add_flag(_arc,          LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_speed,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_unit,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_minus,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_plus,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_arc,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_speed,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_unit,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_minus,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_plus,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_lbl_safety,    LV_OBJ_FLAG_HIDDEN);

    lv_arc_set_value(_arc, (int16_t)(speed_mph * 10));
    lv_obj_set_style_arc_color(_arc, speed_colour(speed_mph), LV_PART_INDICATOR);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", speed_mph);
    lv_label_set_text(_lbl_speed, buf);

    snprintf(buf, sizeof(buf), "Incline: %d", incline);
    lv_label_set_text(_lbl_incline, buf);

    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(elapsed_sec / 60), (unsigned)(elapsed_sec % 60));
    lv_label_set_text(_lbl_time, buf);
}

#endif // BOARD_ZX2D80CE02S
