#include "board_select.h"
#ifdef BOARD_ESP32_2432S028R
// ============================================================
// Display + UI implementation for AITRIP / Sunton ESP32-2432S028R (CYD)
// (240x320 PORTRAIT, ILI9341 via SPI + XPT2046 resistive touch via VSPI)
//
// Rotation: VERIFIED this panel's GRAM is natively LANDSCAPE 320x240.  Configured
//   as memory/panel 320x240 with offset_rotation=0.  The board is MOUNTED with
//   the USB-C port on TOP; LCD_ROTATION=5 renders an upright 240x320 portrait.
//   (Rotation 4 was the upright landscape with USB on the right; rotation 5 is
//   one 90° step further, compensating the physical rotation — see config header.)
// Touch:  XPT2046 calibration below; touch offset_rotation=4 keeps the effective
//   touch rotation at 0 so convertRawXY applies the stored affine, then its
//   rotation-1 step remaps the affine output (u,v) → (v, 319-u) to the portrait frame.
// Build with -D TEST_MARKERS=1 to re-run the diagnostic sweep / tag confirmation.
// ============================================================

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include "config.h"
#include "ui.h"

// Diagnostic screen for picking rotation/colour.  Set via build flag:
//   pio run -e esp32_2432s028r -D TEST_MARKERS=1
#ifndef TEST_MARKERS
#define TEST_MARKERS 0
#endif

// ---- LovyanGFX board configuration -----------------------------------------
// The CYD wires the display and the touch controller to TWO different SPI
// buses: ILI9341 on HSPI, XPT2046 on VSPI.

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
            cfg.freq_write  = 40000000;  // 40 MHz — matches reference; drop if artifacts
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
            // VERIFIED on this board (clone): the GRAM is natively LANDSCAPE
            // 320x240.  Phase-B sweep confirmed setRotation(4) gave a full-screen,
            // upright landscape with USB on the right.  The board is now mounted
            // rotated 90° CCW (USB on top), so LCD_ROTATION=5 shows an upright
            // 240x320 portrait read from the same 320x240 GRAM.
            cfg.memory_width  = 320;  cfg.panel_width  = 320;
            cfg.memory_height = 240;  cfg.panel_height = 240;
            cfg.offset_x      = 0;   cfg.offset_y     = 0;
            cfg.offset_rotation   = 0;
            cfg.bus_shared        = false;
            cfg.dummy_read_pixel  = 8;
            cfg.dummy_read_bits   = 1;
            cfg.readable          = true;
            cfg.invert            = false;
            cfg.rgb_order         = false;
            cfg.dlen_16bit        = false;
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
            // Coarse defaults — overridden at boot by the baked full-affine
            // calibration below. offset_rotation=4 cancels the display's
            // rotation-4 axis flip, so convertRawXY applies only the affine.
            cfg.x_min           = 300;  cfg.x_max = 3900;
            cfg.y_min           = 200;  cfg.y_max = 3700;
            cfg.pin_int         = TOUCH_IRQ;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 4;    // neutralize display rotation-4 axis flip
            cfg.spi_host        = -1;   // let LovyanGFX assign the SPI host
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

    // Read the panel/driver ID over the SPI bus (MIPI RDDID / 0x04).
    // ILI9341 reports 0x00 0x93 0x41; ST7789 typically 0x00 0x85 0x85.
    // `_panel.readCommand` is public on Panel_LCD so this compiles from here.
    uint32_t readPanelID() {
        return _panel.readCommand(0x04, 0, 3);
    }
};

static LGFX_ESP32_2432S028R _gfx;

// ---- LVGL glue --------------------------------------------------------------

static lv_disp_draw_buf_t _draw_buf;
// Buffers sized for portrait width (240) × 10 rows.
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
        static uint32_t tdbg = 0;
        uint32_t now = millis();
        if (now - tdbg > 250) { tdbg = now; Serial.printf("[APP] touch=(%u,%u)\n", (unsigned)x, (unsigned)y); }
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
static lv_obj_t *_btn_fast;
static lv_obj_t *_btn_slow;
static lv_obj_t *_btn_inc_up;
static lv_obj_t *_btn_inc_down;
static lv_obj_t *_btn_stop;
static lv_obj_t *_lbl_stop;
static lv_obj_t *_lbl_safety;

// Arc spans 240° centred at the bottom: 150° → 30° clockwise.
#define ARC_START  150
#define ARC_END     30
#define ARC_MAX    ((int16_t)(MAX_SPEED_MPH * 10.0f))

// Layout constants (portrait 240×320, all in pixels).
// Top:  shrunken speed arc + readouts (speed, unit, time, incline level).
// Mid:  speed buttons (Fast+/Slow-) and incline buttons (Incline+/Incline-)
//       as two full-width 2-up rows — the largest the portrait panel allows.
// Bottom: start/stop strip across the full width.
#define MARGIN        6
#define GAP           4
#define BTN_W         ((SCREEN_W - 2 * MARGIN - GAP) / 2)  // 112
#define BTN_X_L       MARGIN                               // 6
#define BTN_X_R       (BTN_X_L + BTN_W + GAP)              // 122
#define BTN_H         58
#define ARC_SIZE      140   // shrunken speed gauge (was 190)
#define ARC_X         ((SCREEN_W - ARC_SIZE) / 2)          // 50
#define ARC_Y         2
#define SPEED_BTN_Y   156
#define INC_BTN_Y     220
#define STOP_X        MARGIN
#define STOP_Y        284
#define STOP_W        (SCREEN_W - 2 * MARGIN)              // 228
#define STOP_H        32

static void (*_speed_cb)(int32_t delta)    = nullptr;
static void (*_stop_cb)()                  = nullptr;
static void (*_incline_cb)(int32_t steps)  = nullptr;

// Track running state to avoid unconditional redraws every 250 ms.
static bool _last_running = true;   // inverted so first call always writes

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

// Full-width-row speed button (blue).
static lv_obj_t *make_speed_btn(lv_obj_t *scr, const char *label,
                                 int32_t x, int32_t y, int32_t delta) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E6FCC), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0A4A99), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, btn_speed_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)delta);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    return btn;
}

// Full-width-row incline button (amber).
static lv_obj_t *make_incline_btn(lv_obj_t *scr, const char *label,
                                   int32_t x, int32_t y, int32_t steps) {
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xCC8A1E), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x99641A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, btn_incline_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)steps);
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

void ui_set_speed_callback(void (*cb)(int32_t delta)) { _speed_cb = cb; }
void ui_set_stop_callback(void (*cb)())               { _stop_cb  = cb; }
void ui_set_incline_callback(void (*cb)(int32_t steps)) { _incline_cb = cb; }

// ---- Diagnostic probe (TEST_MARKERS) -----------------------------------------

#if TEST_MARKERS
// Pure-hardware probe — no LVGL at all.  Geometry is now FIXED in the class
// constructor (320x240 native-landscape GRAM, offset_rotation=0, verified via
// the earlier A/B sweep).  This just confirms the locked LCD_ROTATION and prints
// the panel RDDID so a fresh board can be sanity-checked.
// Build:
//   pio run -e esp32_2432s028r -D TEST_MARKERS=1 -t upload
// On screen: TOP edge 20px colour ruler (width), LEFT 20px grey ruler (height),
//   a crosshair, and the rotation digit LCD_ROTATION drawn huge in the centre.
static uint16_t ruler_top_color(int i) {
    static const uint16_t pal[8] = {
        0x0000, 0x001F, 0x07E0, 0x07FF, 0xF800, 0xF81F, 0xFFE0, 0xFFFF
    };
    return pal[i & 7];
}
static uint16_t ruler_left_color(int i) {
    static const uint16_t grey[8] = {
        0x0000, 0x2104, 0x4208, 0x630C, 0x8410, 0xA534, 0xC618, 0xFFFF
    };
    return grey[i & 7];
}
static void draw_rulers(int w, int h) {
    for (int i = 0, x = 0; x < w; i++, x += 20)
        _gfx.fillRect(x, 0, 20, 4, ruler_top_color(i));
    for (int i = 0, y = 0; y < h; i++, y += 20)
        _gfx.fillRect(0, y, 4, 20, ruler_left_color(i));
}
static void confirm_frame(uint8_t r) {
    _gfx.setRotation(r);
    int w = _gfx.width();
    int h = _gfx.height();

    _gfx.fillScreen(lgfx::rgb565_t(24 + r * 16, 255u - r * 8u, r * 40));
    draw_rulers(w, h);

    _gfx.drawLine(0, h / 2, w, h / 2, lgfx::rgb565_t(255, 255, 255));
    _gfx.drawLine(w / 2, 0, w / 2, h, lgfx::rgb565_t(255, 255, 255));

    _gfx.setFont(&lgfx::fonts::Font0);
    _gfx.setTextColor(0x0000u);   // black
    _gfx.setTextDatum(lgfx::middle_center);
    _gfx.setTextSize(6);
    _gfx.drawNumber(r, w / 2, h / 2);
}

static void probe_screen() {
    Serial.begin(115200);  // may run before setup(); idempotent
    Serial.println();
    Serial.println("[PROBE] Physical-corner touch calibration.");
    Serial.println("[PROBE] A crosshair appears at each corner; tap+hold it precisely.");
    Serial.println("[PROBE] Order: TL -> BL -> TR -> BR. Raw is 16-sample averaged.");

    _gfx.setRotation(LCD_ROTATION);
    int w = _gfx.width();
    int h = _gfx.height();

    const int cx[4] = { 4,  4, w - 5, w - 5 };
    const int cy[4] = { 4,  h - 5, 4,   h - 5 };
    const char *nm[4] = { "TL", "BL", "TR", "BR" };
    static const int _RAWERR = 20;

    for (int i = 0; i < 4; ++i) {
        _gfx.fillScreen(lgfx::rgb565_t(40, 40, 40));
        _gfx.setFont(&lgfx::fonts::Font0);
        _gfx.setTextColor(0xFFFFu);
        _gfx.setTextDatum(lgfx::middle_center);
        _gfx.setTextSize(2);
        _gfx.drawString(nm[i], cx[i], cy[i] - 40);
        _gfx.drawLine(cx[i] - 30, cy[i], cx[i] + 30, cy[i], 0xF800u);
        _gfx.drawLine(cx[i], cy[i] - 40, cx[i], cy[i] + 30, 0xF800u);

        int32_t ax = 0, ay = 0;
        bool captured = false;
        while (!captured) {
            lgfx::v1::touch_point_t tp, tp2;
            do { delay(2); } while (!_gfx.getTouchRaw(&tp));
            delay(12);
            if (!_gfx.getTouchRaw(&tp2)) continue;
            if (abs(tp.x - tp2.x) > _RAWERR || abs(tp.y - tp2.y) > _RAWERR) continue;

            long sx = tp.x + tp2.x, sy = tp.y + tp2.y;
            bool ok = true;
            for (int j = 0; j < 7; ++j) {
                int32_t a, b, c, d;
                if (!_gfx.getTouchRaw(&a, &b)) { ok = false; break; }
                delay(12);
                if (!_gfx.getTouchRaw(&c, &d)) { ok = false; break; }
                if (abs(a - c) > _RAWERR || abs(b - d) > _RAWERR) { ok = false; break; }
                sx += a + c; sy += b + d;
            }
            if (!ok) { delay(40); continue; }
            ax = (int32_t)(sx >> 4);
            ay = (int32_t)(sy >> 4);
            captured = true;
            Serial.printf("[PHY] %s raw=(%4d,%4d)\n", nm[i], (int)ax, (int)ay);
            do { delay(4); } while (_gfx.getTouchRaw(&tp));
        }
    }
    Serial.println("[PHY] Capture done. Paste the four raw= values into s_touch_cal.");

    while (true) { delay(10000); }
}
#endif // TEST_MARKERS

// ---- Public interface -------------------------------------------------------

void ui_init() {
    _gfx.init();
    _gfx.setRotation(LCD_ROTATION);
    _gfx.fillScreen(TFT_BLACK);

#if TEST_MARKERS
    probe_screen();
    return;
#endif

    // ---- Touch calibration (stored affine) ----
    // Raw XPT2046 readings measured at the four PHYSICAL display corners with a
    // precise on-screen crosshair (rotation-4 landscape frame). The sensor is
    // mounted rotated ~90 deg (raw_x ~ vertical, raw_y ~ horizontal), so the
    // full 4-corner affine is required — an axis-aligned min/max range cannot
    // represent it. With touch offset_rotation=4 the display's rotation-5 axis
    // flip is cancelled (r -> 1 in convertRawXY) so the stored affine is applied
    // in the rotation-0 frame, then the r=1 step remaps it into the current
    // portrait frame: (u,v) -> (v, 319-u).  This stays valid for the USB-on-top
    // mounting because the panel and the touch sensor rotate together.
    static const uint16_t s_touch_cal[8] = {
        524, 488,    // TL (0,0)
        3302, 412,   // BL (0,h-1)
        627, 3650,   // TR (w-1,0)
        3473, 3630,  // BR (w-1,h-1)
    };
    _gfx.setTouchCalibrate((uint16_t *)s_touch_cal);

    lv_init();
    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, SCREEN_W * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;   // 240
    disp_drv.ver_res  = SCREEN_H;   // 320
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

    // ---- Top: shrunken speed arc + readouts ----------------------------------
    _arc = lv_arc_create(scr);
    lv_obj_set_size(_arc, ARC_SIZE, ARC_SIZE);
    lv_obj_set_pos(_arc, ARC_X, ARC_Y);
    lv_arc_set_range(_arc, 0, ARC_MAX);
    lv_arc_set_bg_angles(_arc, ARC_START, ARC_END);
    lv_arc_set_value(_arc, 0);
    lv_obj_remove_style(_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x00C8FF), LV_PART_INDICATOR);

    // Speed value centred inside arc.
    _lbl_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_white(), 0);
    lv_label_set_text(_lbl_speed, "0.0");
    lv_obj_align_to(_lbl_speed, _arc, LV_ALIGN_CENTER, 0, -6);

    // Unit label below speed value.
    _lbl_unit = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_unit, "mph");
    lv_obj_align_to(_lbl_unit, _lbl_speed, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // Elapsed time in the arc's open bottom gap.
    _lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lbl_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(_lbl_time, "00:00");
    lv_obj_align_to(_lbl_time, _arc, LV_ALIGN_CENTER, 0, 36);

    // Incline level readout, same open bottom gap.
    _lbl_incline = lv_label_create(scr);
    lv_obj_set_style_text_font(_lbl_incline, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lbl_incline, lv_color_hex(0xFFCC00), 0);
    lv_obj_set_style_text_align(_lbl_incline, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_lbl_incline, "Incline 0");
    lv_obj_align_to(_lbl_incline, _arc, LV_ALIGN_CENTER, 0, 58);

    // ---- Speed buttons: Fast+/Slow- side by side ----------------------------
    _btn_fast = make_speed_btn(scr, "Fast +", BTN_X_L, SPEED_BTN_Y,  1);
    _btn_slow = make_speed_btn(scr, "Slow -", BTN_X_R, SPEED_BTN_Y, -1);

    // ---- Incline buttons: Incline+/Incline- side by side --------------------
    _btn_inc_up   = make_incline_btn(scr, "Incline +", BTN_X_L, INC_BTN_Y,  1);
    _btn_inc_down = make_incline_btn(scr, "Incline -", BTN_X_R, INC_BTN_Y, -1);

    // ---- Bottom strip: start/stop -------------------------------------------
    _btn_stop = lv_btn_create(scr);
    lv_obj_set_size(_btn_stop, STOP_W, STOP_H);
    lv_obj_set_pos(_btn_stop, STOP_X, STOP_Y);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x226622), 0);
    lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x154415), LV_STATE_PRESSED);
    lv_obj_set_style_radius(_btn_stop, 6, 0);
    lv_obj_add_event_cb(_btn_stop, btn_stop_cb, LV_EVENT_CLICKED, nullptr);

    _lbl_stop = lv_label_create(_btn_stop);
    lv_obj_set_style_text_font(_lbl_stop, &lv_font_montserrat_20, 0);
    lv_label_set_text(_lbl_stop, "Start");
    lv_obj_center(_lbl_stop);

    // ---- Safety overlay (hidden until triggered) ----------------------------
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
    // Skip widget updates when the marker screen is active.
#ifdef TEST_MARKERS
    if (TEST_MARKERS) return;
#endif
    if (safety) {
        lv_obj_add_flag(_arc,          LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_speed,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_unit,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_time,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_incline,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_inc_up,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_inc_down, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_stop,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_fast,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_btn_slow,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lbl_safety, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_arc,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_speed,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_unit,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_time,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_lbl_incline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_inc_up,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_inc_down,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_stop,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_fast,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_btn_slow,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_lbl_safety,    LV_OBJ_FLAG_HIDDEN);

    // Only repaint the stop button when running state actually changes —
    // prevents the unconditional style write from triggering full redraws
    // every 250ms and causing nearby buttons to flash.
    if (running != _last_running) {
        if (running) {
            lv_label_set_text(_lbl_stop, "Stop");
            lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0xAA2222), 0);
            lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x881111), LV_STATE_PRESSED);
        } else {
            lv_label_set_text(_lbl_stop, "Start");
            lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x226622), 0);
            lv_obj_set_style_bg_color(_btn_stop, lv_color_hex(0x154415), LV_STATE_PRESSED);
        }
        _last_running = running;
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
