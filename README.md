# Tradmill

ESP32 firmware for a hacked treadmill motor controller.  Three hardware
targets live in this single repository — pick your board and flash.

---

## Hardware targets

| Board | Directory | Display | MCU |
|---|---|---|---|
| MicroPython ESP32 | `boards/micropython-esp32/` | SSD1306 128×64 OLED (I2C) | ESP32 |
| CrowPanel 1.28" round | `boards/Tradmill/` (`BOARD_GC9A01`) | GC9A01 240×240 (SPI) | ESP32-S3 |
| Smartpanel ZX2D80CE02S | `boards/Tradmill/` (`BOARD_ZX2D80CE02S`) | ST7789 240×320 (8080 parallel) + touch | ESP32-S3 |

All three targets drive the same treadmill hardware:

- **Motor speed** — MC2100 controller via 20 Hz PWM
- **Incline** — relay pair driven by two active-low push buttons
- **Safety key** — optional digital input; kills the motor if pulled low

---

## Selecting your board

### MicroPython (boards/micropython-esp32/)

No selection needed — just deploy the Python files to your device.

1. Flash MicroPython to the ESP32 (`ESP32_GENERIC` build).
2. Copy `boards/micropython-esp32/` to the device root with `mpremote`, Thonny, or
   `ampy`.
3. Edit `config.py` to match your wiring.

**Speed control:** analog potentiometer on `SPEED_POT_PIN`.  
**Start / stop:** click the rotary encoder button (`ENCODER_SW`).

---

### Arduino — GC9A01 or ZX2D80CE02S (boards/Tradmill/)

1. Open `boards/Tradmill/Tradmill.ino` in Arduino IDE 2.x.
2. **Open `board_select.h` and uncomment exactly one `#define`:**

```cpp
// #define BOARD_GC9A01       // CrowPanel 1.28" 240x240 round
// #define BOARD_ZX2D80CE02S  // Smartpanel ZX2D80CE02S 240x320
```

3. Install the required libraries for your board (see below).
4. Select the correct ESP32-S3 board in *Tools → Board*.
5. Compile and flash.

**Speed control:** rotate the encoder (±0.5 mph per tick).  
**Start / stop:** click the rotary encoder button.  
**ZX2D80CE02S only:** on-screen **Slow −** / **Fast +** touch buttons also adjust speed.

#### Library dependencies

| Board | Library | Install via |
|---|---|---|
| GC9A01 | `Arduino_GFX_Library` | Library Manager |
| GC9A01 | `lvgl` ≥ 8.x | Library Manager |
| ZX2D80CE02S | `LovyanGFX` | Library Manager |
| ZX2D80CE02S | `lvgl` ≥ 8.x | Library Manager |

Only the libraries for your selected board need to be installed.

---

## Pin reference

### MicroPython ESP32

| Signal | GPIO |
|---|---|
| Speed PWM out | 18 |
| Incline up relay | 19 |
| Incline down relay | 23 |
| Speed potentiometer | 34 |
| Incline up button | 25 |
| Incline down button | 26 |
| Display SDA | 21 |
| Display SCL | 22 |
| Encoder CLK | 32 |
| Encoder DT | 33 |
| Encoder SW (click) | 35 |

### CrowPanel GC9A01 (ESP32-S3)

See `boards/Tradmill/config_gc9a01.h` for the full pin list.

### Smartpanel ZX2D80CE02S (ESP32-S3)

See `boards/Tradmill/config_zx2d80ce02s.h` for the full pin list.
Note: incline buttons repurpose the RS485 RXD/RTS pins (GPIO 4/5).

---

## Repository layout

```
boards/
├── micropython-esp32/    # MicroPython target
│   ├── config.py         # Pin and tuning constants — edit this for your wiring
│   ├── main.py           # Entry point and main loop
│   ├── treadmill.py      # Motor, incline, safety, and encoder-click logic
│   ├── display.py        # SSD1306 OLED driver wrapper
│   └── blink.py          # GPIO blink utility for hardware testing
└── Tradmill/             # Arduino sketch (open this folder in Arduino IDE)
    ├── board_select.h    # ← Edit this to pick your board
    ├── config.h          # Dispatches to the selected board config
    ├── config_gc9a01.h   # Pin and PWM constants for the GC9A01 board
    ├── config_zx2d80ce02s.h  # Pin and PWM constants for the ZX2D80CE02S board
    ├── treadmill.h / .cpp    # Motor, incline, safety logic — shared by both boards
    ├── ui.h                  # Display interface (init / update / speed callback)
    ├── ui_gc9a01.cpp         # GC9A01 display init + LVGL widgets
    ├── ui_zx2d80ce02s.cpp    # ZX2D80CE02S display init + LVGL widgets + touch
    └── Tradmill.ino          # Main loop — board-agnostic
```
