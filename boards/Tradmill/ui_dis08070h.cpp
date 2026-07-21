#include "board_select.h"
#ifdef BOARD_DIS08070H
// ============================================================
// Display + UI implementation for Elecrow CrowPanel DIS08070H
// (800x480, 16-bit RGB parallel, GT911 capacitive touch)
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
// Panel_RGB and Bus_RGB are platform-specific and not auto-included by LovyanGFX.hpp
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "soc/usb_serial_jtag_reg.h"
#include "soc/io_mux_reg.h"
#include "hal/usb_serial_jtag_ll.h"
#include "config.h"
#include "ui.h"

static const char *TAG_UI = "ui_dis";

// ---- Free GPIO 19/20 for the GT911 touch I2C bus ---------------------------
// On this board the GT911 lives on GPIO 19/20, which are also the ESP32-S3
// USB_SERIAL_JTAG D-/D+ pins.  The USB PHY pad keeps driving those lines even
// after the I2C driver claims them, so the touch bus hangs with continuous
// "I2C software timeout" errors and touch never works.  Disabling the USB PHY
// before app startup hands the pins back to the I2C peripheral.
//
// NOTE: this kills USB-CDC serial.  Keep Arduino "USB CDC On Boot: Disabled" so
// logging continues over UART0 (GPIO 43/44).
static __attribute__((constructor(101))) void disable_usb_serial_jtag_phy() {
    // The HAL helper was renamed across ESP-IDF versions:
    //   IDF 4.4 (Arduino core 2.x): usb_serial_jtag_ll_enable_pad(bool)
    //   IDF 5.x:                    usb_serial_jtag_ll_phy_enable_pad(bool)
#if defined(USB_SERIAL_JTAG_LL_PHY_ENABLE_PAD) || ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    usb_serial_jtag_ll_phy_enable_pad(false);
#else
    usb_serial_jtag_ll_enable_pad(false);
#endif
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    PIN_FUNC_SELECT(IO_MUX_GPIO19_REG, PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(IO_MUX_GPIO20_REG, PIN_FUNC_GPIO);
    PIN_INPUT_ENABLE(IO_MUX_GPIO19_REG);
    PIN_INPUT_ENABLE(IO_MUX_GPIO20_REG);
    REG_SET_BIT(IO_MUX_GPIO19_REG, FUN_PU);
    REG_SET_BIT(IO_MUX_GPIO20_REG, FUN_PU);
    REG_CLR_BIT(IO_MUX_GPIO19_REG, FUN_PD);
    REG_CLR_BIT(IO_MUX_GPIO20_REG, FUN_PD);
}

// ---- GT911 power-on: PCA9557-driven reset + address strap ------------------
// The GT911's RST and INT lines are not wired to the ESP32 directly — they run
// through the on-board PCA9557 I/O expander (I2C 0x18) on the same bus as the
// touch controller (SDA 19 / SCL 20).
//
// The GT911 latches its I2C address from the INT pin level on the *rising* edge
// of RST:  INT low  -> 0x5D,  INT high -> 0x14.  Until RST is released the GT911
// stays in reset and ACKs at no address, which is why simply changing
// TOUCH_I2C_ADDR never fixed touch.  This routine drives a deterministic
// sequence (INT low, hold RST low, then release RST) so the GT911 boots and
// answers at 0x5D every time.
//
// PCA9557 registers: 0x00 input, 0x01 output, 0x02 polarity, 0x03 config
// (config bit = 1 -> input, 0 -> output).

static bool i2c_present(uint8_t addr);   // defined below

static bool pca9557_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(PCA9557_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

// Reproduces Elecrow's official CrowPanel 7" V3.0 reset timing exactly:
//   Wire.begin(19,20); Out.reset(); setMode(OUTPUT);
//   IO0=LOW, IO1=LOW; delay(20); IO0=HIGH; delay(100); IO1->INPUT;
// IO0 = GT911 RST, IO1 = GT911 INT.  Holding INT low while RST rises selects
// I2C address 0x5D; INT is then switched back to an input so the GT911 can use
// it as its interrupt output.
//
// PCA9557 registers: 0x00 input, 0x01 output, 0x02 polarity, 0x03 config
// (config bit 1 = input, 0 = output).
static void gt911_reset_sequence() {
    const uint8_t io0 = (1 << PCA9557_GT911_RST_BIT);   // RST
    const uint8_t io1 = (1 << PCA9557_GT911_INT_BIT);   // INT

    // Wire is already begun by ui_init(); don't re-init.
    if (!i2c_present(PCA9557_I2C_ADDR)) {
        Serial.printf("[TOUCH] PCA9557 not at 0x%02X — reset skipped\n", PCA9557_I2C_ADDR);
        return;
    }

    // "Out.reset()": clear polarity + drive outputs low to a known state.
    pca9557_write(0x02, 0x00);                 // polarity normal
    pca9557_write(0x01, 0x00);                 // outputs low
    // setMode(OUTPUT): make IO0 and IO1 outputs (config bit = 0).
    pca9557_write(0x03, (uint8_t)~(io0 | io1));

    // IO0 LOW, IO1 LOW  (RST asserted, INT low → selects 0x5D)
    pca9557_write(0x01, 0x00);
    delay(20);
    // IO0 HIGH (release RST) with IO1 still LOW → GT911 latches 0x5D
    pca9557_write(0x01, io0);
    delay(100);
    // IO1 -> INPUT (INT becomes the GT911 interrupt line)
    pca9557_write(0x03, (uint8_t)~io0);
    delay(50);                                 // GT911 firmware boot time
    Serial.printf("[TOUCH] GT911 Elecrow reset sequence done (addr should be 0x5D)\n");
}

// Returns true if a device ACKs at `addr` on the current Wire bus.
static bool i2c_present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// One-shot I2C scan.  Uses Serial.printf directly (not ESP_LOG) so the output
// shows regardless of log level / Serial-init ordering.
// Look for 0x5D (or 0x14) = GT911 and 0x18 = PCA9557.
static void i2c_scan_touch_bus() {
    Serial.printf("[TOUCH] I2C scan on SDA %d / SCL %d:\n", I2C_SDA, I2C_SCL);
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 0x7F; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[TOUCH]   device @ 0x%02X\n", addr);
            found++;
        }
    }
    if (!found) Serial.printf("[TOUCH]   NO I2C devices found — bus dead (USB PHY? wrong pins?)\n");
}

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
            cfg.freq_write        = 10000000; // 12 MHz
            cfg.pin_hsync         = SCREEN_HSYNC;
            cfg.pin_vsync         = SCREEN_VSYNC;
            cfg.pin_pclk          = SCREEN_PCLK;
            cfg.pin_henable       = SCREEN_DE;
            cfg.panel             = &_panel;  // Bus_RGB requires back-pointer to panel
            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch  = 40;
            cfg.vsync_front_porch = 13;
            cfg.vsync_pulse_width = 1;
            cfg.vsync_back_porch  = 31;
            cfg.pclk_active_neg   = 1;
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
            cfg.pin_int         = TOUCH_INT;  // -1: INT is the GT911 addr strap (GPIO 38) — leave it alone
            cfg.pin_rst         = TOUCH_RST;  // -1: RST released by board pull-up via PCA9557
            cfg.pin_sda         = I2C_SDA;
            cfg.pin_scl         = I2C_SCL;
            cfg.i2c_port        = 0;
            cfg.i2c_addr        = TOUCH_I2C_ADDR;  // 0x5D default; flip to 0x14 in config if touch is dead
            cfg.freq            = 400000;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

// Constructed in ui_init() so Panel_RGB's LCD_CAM interrupt runs after setup().
static LGFX_DIS08070H *_gfx = nullptr;

// ---- LVGL glue --------------------------------------------------------------

static lv_disp_draw_buf_t _draw_buf;
static lv_color_t _buf1[SCREEN_W * 10];
static lv_color_t _buf2[SCREEN_W * 10];

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colors) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    _gfx->startWrite();
    _gfx->setAddrWindow(area->x1, area->y1, w, h);
    _gfx->writePixels((lgfx::rgb565_t *)colors, w * h);
    _gfx->endWrite();
    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t x, y;
    if (_gfx->getTouch(&x, &y)) {
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

void ui_set_incline_callback(void (*cb)(int32_t steps)) {
    (void)cb;  // no incline buttons on this board's layout
}

void ui_init() {
    ESP_LOGI(TAG_UI, "PSRAM=%s size=%u heap=%u",
        psramFound() ? "found" : "MISSING",
        (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreeHeap());
    // ---- Touch (GT911) bring-up + self-test --------------------------------
    // ui_init() runs BEFORE setup()'s Serial.begin(), so start Serial here too
    // (idempotent) and use Serial.printf so the report is always visible on UART0.
    //
    // Sequence: bring up the touch I2C bus on GPIO 19/20, run the Elecrow PCA9557
    // reset so a healthy GT911 boots and latches address 0x5D, then verify it
    // responds.  If it doesn't, the GT911 panel is absent/defective (confirmed on
    // this unit: even Elecrow's factory firmware gets no touch) — the rest of the
    // UI still runs, it just won't receive touch input.
    Serial.begin(115200);
    delay(50);
    Wire.begin(I2C_SDA, I2C_SCL, 400000);
    gt911_reset_sequence();

    bool touch_ok = i2c_present(TOUCH_I2C_ADDR);
    if (touch_ok) {
        Serial.printf("[TOUCH] GT911 detected at 0x%02X — touch enabled\n", TOUCH_I2C_ADDR);
    } else {
        Serial.printf("[TOUCH] GT911 NOT responding at 0x%02X (tried PCA9557 reset).\n",
                      TOUCH_I2C_ADDR);
        Serial.printf("[TOUCH] Touch unavailable — likely absent/defective panel. "
                      "Running an I2C scan for reference:\n");
        i2c_scan_touch_bus();   // shows what IS on the bus (e.g. PCA9557 @ 0x18)
    }

    ESP_LOGI(TAG_UI, "new LGFX...");
    _gfx = new LGFX_DIS08070H();
    ESP_LOGI(TAG_UI, "gfx.init...");
    if (!_gfx->init()) {
        ESP_LOGE(TAG_UI, "gfx.init FAILED — check USB Mode (LCD_CAM interrupt conflict)");
        return;
    }
    ESP_LOGI(TAG_UI, "display OK — flashing red then black");
    _gfx->fillScreen(TFT_RED);   // visible for 2 s → confirms display hardware works
    delay(2000);
    _gfx->fillScreen(TFT_BLACK);
    ESP_LOGI(TAG_UI, "lv_init...");
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
    if (!_arc) return;  // init failed — no widgets to update
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
