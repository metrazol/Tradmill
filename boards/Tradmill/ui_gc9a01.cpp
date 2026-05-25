#include "board_select.h"
#ifdef BOARD_GC9A01
// ============================================================
// Display + UI implementation for CrowPanel 1.28" GC9A01
// (240x240 round, SPI, no touch screen)
// ============================================================

#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "config.h"
#include "ui.h"

// ---- Display hardware -------------------------------------------------------

static Arduino_DataBus *_spi;
static Arduino_GFX     *_gfx;

static lv_disp_draw_buf_t _draw_buf;
static lv_color_t _buf1[SCREEN_W * 10];
static lv_color_t _buf2[SCREEN_W * 10];

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colors) {
    _gfx->draw16bitRGBBitmap(
        area->x1, area->y1,
        (uint16_t *)colors,
        area->x2 - area->x1 + 1,
        area->y2 - area->y1 + 1
    );
    lv_disp_flush_ready(drv);
}

// ---- LVGL widget handles ----------------------------------------------------

static lv_obj_t *_arc;
static lv_obj_t *_lbl_speed;
static lv_obj_t *_lbl_unit;
static lv_obj_t *_lbl_incline;
static lv_obj_t *_lbl_time;
static lv_obj_t *_lbl_safety;

// Arc spans 240° centred at the bottom: 150° → 30° (clockwise)
#define ARC_START  150
#define ARC_END     30
#define ARC_MAX    120   // represents 12.0 mph (value = mph * 10)

// ---- Arc indicator colour: blue → green → yellow → red with speed ----------

static lv_color_t speed_colour(float mph) {
    if      (mph < 4.0f)  return lv_color_hex(0x00C8FF);  // blue-white: easy
    else if (mph < 8.0f)  return lv_color_hex(0x00E060);  // green: moderate
    else if (mph < 10.0f) return lv_color_hex(0xFFCC00);  // yellow: hard
    else                  return lv_color_hex(0xFF4444);  // red: max
}

// ---- Public interface -------------------------------------------------------

void ui_set_speed_callback(void (*cb)(int32_t delta)) {
    (void)cb;  // no touch screen — speed is encoder-only on this board
}

void ui_init() {
    // Power on display
    pinMode(LCD_EN_PIN, OUTPUT); digitalWrite(LCD_EN_PIN, HIGH);
    pinMode(SCREEN_BL,  OUTPUT); digitalWrite(SCREEN_BL,  HIGH);

    // Init GC9A01 via SPI
    _spi = new Arduino_ESP32SPI(SCREEN_DC, SCREEN_CS, SCREEN_SCLK, SCREEN_MOSI);
    _gfx = new Arduino_GC9A01(_spi, SCREEN_RST, 0 /* rotation */, true /* IPS */);
    _gfx->begin();
    _gfx->fillScreen(0x0000);

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

    // Build widgets
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Speed arc (fills most of the 240x240 circle)
    _arc = lv_arc_create(scr);
    lv_obj_set_size(_arc, 210, 210);
    lv_obj_center(_arc);
    lv_arc_set_range(_arc, 0, ARC_MAX);
    lv_arc_set_bg_angles(_arc, ARC_START, ARC_END);
    lv_arc_set_value(_arc, 0);
    lv_obj_remove_style(_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x00C8FF), LV_PART_INDICATOR);

    // Large speed value in the centre
    _lbl_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_white(), 0);
    lv_label_set_text(_lbl_speed, "0.0");
    lv_obj_align(_lbl_speed, LV_ALIGN_CENTER, 0, -20);

    _lbl_unit = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_unit, "mph");
    lv_obj_align_to(_lbl_unit, _lbl_speed, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    // Incline level near the top of the circle
    _lbl_incline = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_incline, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_incline, lv_color_hex(0xFFCC00), 0);
    lv_label_set_text(_lbl_incline, "Incline: 0");
    lv_obj_align(_lbl_incline, LV_ALIGN_CENTER, 0, -60);

    // Elapsed time near the bottom of the circle
    _lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_time, "00:00");
    lv_obj_align(_lbl_time, LV_ALIGN_CENTER, 0, 60);

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
        lv_obj_clear_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_arc,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_speed,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_unit,    LV_OBJ_FLAG_HIDDEN);
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

#endif // BOARD_GC9A01
