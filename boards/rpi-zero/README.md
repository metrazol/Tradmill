# Tradmill — Pi Zero W v1.1

Port of Tradmill ESP32 firmware to Raspberry Pi Zero W v1.1 with physical pots and GC9A01 round display.

## Wiring

| Pi Pin | BCM | Function |
|---|---|---|
| 1 3.3V | — | MCP VDD + ADS1115 VDD + pots CW |
| 6 GND | — | Star GND |
| 3 SDA | GPIO2 | I2C1 SDA → MCP SDA + ADS1115 SDA |
| 5 SCL | GPIO3 | I2C1 SCL → MCP SCL + ADS1115 SCL |
| 19 MOSI | GPIO10 | SPI0 MOSI → GC9A01 SDA |
| 23 SCLK | GPIO11 | SPI0 SCLK → GC9A01 SCL |
| 24 CE0 | GPIO8 | GC9A01 CS |
| 22 | GPIO25 | GC9A01 DC |
| 13 | GPIO27 | GC9A01 RST |
| 12 | GPIO18 | GC9A01 BL (HIGH) |
| 35 | GPIO19 | PWM → 330Ω → PC817 → MC2100 (10k pulldown) |

- I2C devices: MCP23017 `0x20` (A0-2 GND) GPA0=up relay GPA1=down relay, ADS1115 `0x48` (ADDR GND) A0=speed pot A1=incline pot.
- PWM: kernel `pwm-2chan pin=18 func=2 pin2=19 func2=2` period `50M ns` duty `0-27.5M ns` (55% max) on `pwmchip0/pwm1`.
- Pots: 10k linear, 1k series + 100nF wiper filter. Idle CCW <1.5% norm → auto-stop with 3s ramp.
- Relays: JD-VCC=5V VCC=3.3V HIGH=on opto-isolated.

## Setup (Bookworm)

```bash
sudo raspi-config  # Enable I2C + SPI
# /boot/firmware/config.txt add:
# dtparam=i2c_arm=on
# dtparam=spi=on
# dtoverlay=pwm-2chan,pin=18,func=2,pin2=19,func2=2
# dtparam=watchdog=on
sudo reboot
pip install -r requirements.txt
python3 main.py
# service:
sudo cp tradmill.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now tradmill
```

## Behaviour

- Speed pot: `0-12mph` `.25` steps, hysteresis ~22mV, idle <0.015 norm → ramp stop 3s, re-twist CW → run.
- Incline pot: `0-15` 16 levels, hysteresis ~60mV, one `300ms` pulse per `700ms` gap queued toward target.
- Display: GC9A01 240x240 arc 150°→30° MAX 120 (speed*10) colour blue/green/yellow/red.
- HA webhook: set `HA_WEBHOOK_URL` in `config.py`.

## Verify

```bash
i2cdetect -y 1  # expect 20 + 48
ls /sys/class/pwm/pwmchip0
ls /dev/spidev0.0
journalctl -u tradmill -f
```
