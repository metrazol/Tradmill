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
| AITRIP/Sunton CYD ESP32-2432S028R | `boards/Tradmill/` (`BOARD_ESP32_2432S028R`) | ILI9341 240×320 (SPI) + XPT2046 resistive touch | ESP32 |

All three targets drive the same treadmill hardware:

- **Motor speed** — MC2100 controller via 20 Hz PWM on a native MCU GPIO
- **Incline** — relay pair driven by two active-low push buttons
- **Safety key** — optional digital input; kills the motor if pulled low

Incline relays, incline buttons, and the safety key are wired to an
**MCP23017 I2C GPIO expander** (address `0x20`) on a dedicated I2C bus,
separate from any display/touch bus.  The motor speed PWM stays on a native
GPIO — the MCP23017 cannot generate a clean 20 Hz signal.

Set the expander's `A0`/`A1`/`A2` address pins to GND for address `0x20`.
The expander pin map is the same on every target:

| Treadmill signal | MCP23017 pin |
|---|---|
| Incline up relay | GPA0 (pin 0) |
| Incline down relay | GPA1 (pin 1) |
| Incline up button | GPB0 (pin 8) |
| Incline down button | GPB1 (pin 9) |
| Safety key (optional) | GPB2 (pin 10) |

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
// #define BOARD_GC9A01            // CrowPanel 1.28" 240x240 round
// #define BOARD_ZX2D80CE02S       // Smartpanel ZX2D80CE02S 240x320
// #define BOARD_ESP32_2432S028R   // AITRIP/Sunton CYD 2.8" 240x320
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
| _all boards_ | `Adafruit MCP23017 Arduino Library` | Library Manager |

Install the libraries for your selected board, plus the Adafruit MCP23017
library (pulls in `Adafruit BusIO`) which every board needs for the
treadmill incline/safety I/O.

---

## Pin reference

### MicroPython ESP32

| Signal | GPIO |
|---|---|
| Speed PWM out | 18 |
| Speed potentiometer | 34 |
| Display SDA | 21 |
| Display SCL | 22 |
| MCP23017 SDA | 16 |
| MCP23017 SCL | 17 |
| Encoder CLK | 32 |
| Encoder DT | 33 |
| Encoder SW (click) | 35 |

Incline relays, incline buttons, and the safety key are on the MCP23017
expander (see the expander pin map above), reached via the MCP23017 SDA/SCL
bus.

### CrowPanel GC9A01 (ESP32-S3)

See `boards/Tradmill/config_gc9a01.h` for the full pin list.

### Smartpanel ZX2D80CE02S (ESP32-S3)

See `boards/Tradmill/config_zx2d80ce02s.h` for the full pin list.
Note: the MCP23017 I2C bus repurposes the RS485 RXD/RTS pins (GPIO 4/5),
so RS485 is unavailable while the expander is in use.

### Elecrow CrowPanel 7" DIS08070H (ESP32-S3)

See `boards/Tradmill/config_dis08070h.h` for the full pin list.
The MCP23017 bus uses GPIO 17/18.  Moving incline I/O onto the expander
restores the incline-down button this board previously lacked.

**Touch (GT911):** the capacitive touch I2C bus runs on GPIO 19/20, which are
also the ESP32-S3 USB_SERIAL_JTAG D-/D+ pins.  The firmware disables the USB PHY
at boot (`ui_dis08070h.cpp`) so the I2C bus stops hanging with timeouts —
without this, touch never responds.  Because of that you must set Arduino IDE
*Tools → USB CDC On Boot: **Disabled*** so serial logging stays on UART0
(GPIO 43/44); USB-CDC serial will not work on this board.

The GT911 address is strapped by GPIO 38 at power-on and varies per unit
(`0x5D` most common, `0x14` otherwise).  If touch is dead after flashing,
flip `TOUCH_I2C_ADDR` in `config_dis08070h.h` to `0x14`.  Do **not** drive the
INT/RST lines (they go through the on-board PCA9557 and a board pull-up already
releases the GT911 from reset).

### AITRIP/Sunton ESP32-2432S028R (CYD, ESP32)

See `boards/Tradmill/config_esp32_2432s028r.h` for the full pin list.

The display and touch live on **two separate SPI buses**:

| Signal | GPIO | Bus |
|---|---|---|
| Display SCLK | 14 | HSPI |
| Display MOSI | 13 | HSPI |
| Display MISO | 12 | HSPI |
| Display CS | 15 | HSPI |
| Display DC | 2 | HSPI |
| Display RST | -1 (board reset) | — |
| Display backlight | 21 | PWM |
| Touch CLK | 25 | VSPI |
| Touch MOSI | 32 | VSPI |
| Touch MISO | 39 | VSPI |
| Touch CS | 33 | VSPI |
| Touch IRQ | 36 | — |

The MCP23017 I2C bus uses GPIO 22 (SDA) and GPIO 5 (SCL).  GPIO 5 is the
on-board SD-card CS pin, repurposed here because the firmware never uses the
microSD slot.  The motor speed PWM output is on GPIO 27.

This board has no rotary encoder — all control is via the touch screen,
identical in operation to the ZX2D80CE02S.

**Touch calibration:** the raw XPT2046 range is set to `x_min=300 / x_max=3900
/ y_min=200 / y_max=3700` in `ui_esp32_2432s028r.cpp`.  If taps feel offset,
touch the four corners with a stylus and note the raw values from Serial, then
update those four constants.

### MCP23017 I2C bus per board

| Board | MCP SDA | MCP SCL |
|---|---|---|
| MicroPython ESP32 | 16 | 17 |
| GC9A01 | 21 | 20 |
| ZX2D80CE02S | 4 | 5 (RS485 repurposed) |
| DIS08070H | 17 | 18 |
| ESP32-2432S028R | 22 | 5 (SD CS repurposed) |

---

## Repository layout

```
boards/
├── micropython-esp32/    # MicroPython target
│   ├── config.py         # Pin and tuning constants — edit this for your wiring
│   ├── main.py           # Entry point and main loop
│   ├── treadmill.py      # Motor, incline, safety, and encoder-click logic
│   ├── mcp23017.py       # MCP23017 I2C GPIO expander driver
│   ├── display.py        # SSD1306 OLED driver wrapper
│   └── blink.py          # GPIO blink utility for hardware testing
└── Tradmill/             # Arduino sketch (open this folder in Arduino IDE)
    ├── board_select.h    # ← Edit this to pick your board
    ├── config.h          # Dispatches to the selected board config
    ├── config_gc9a01.h   # Pin and PWM constants for the GC9A01 board
    ├── config_zx2d80ce02s.h  # Pin and PWM constants for the ZX2D80CE02S board
    ├── config_esp32_2432s028r.h  # Pin and PWM constants for the CYD board
    ├── treadmill.h / .cpp    # Motor, incline, safety logic — shared by both boards
    ├── ui.h                  # Display interface (init / update / speed callback)
    ├── ui_gc9a01.cpp         # GC9A01 display init + LVGL widgets
    ├── ui_esp32_2432s028r.cpp  # CYD ILI9341 + XPT2046 display init + LVGL widgets + touch
    ├── ui_zx2d80ce02s.cpp    # ZX2D80CE02S display init + LVGL widgets + touch
    └── Tradmill.ino          # Main loop — board-agnostic
```
