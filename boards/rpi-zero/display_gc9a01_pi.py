import time

try:
    import spidev
    import lgpio
    _has_hw = True
except ImportError:
    _has_hw = False

try:
    from luma.core.interface.serial import spi as luma_spi
    from luma.lcd.device import gc9a01
    from luma.core.render import canvas
    from PIL import ImageDraw, ImageFont
    _has_luma = True
except ImportError:
    _has_luma = False

from config import (
    SPI_BUS, SPI_DEVICE, SPI_SPEED_HZ, GC9A01_DC_PIN, GC9A01_RST_PIN,
    GC9A01_BL_PIN, SCREEN_W, SCREEN_H, ARC_START, ARC_END, ARC_MAX, DEBUG
)

_mock = True
_device = None
_h = None


def _speed_colour(mph):
    if mph < 4.0:
        return "#00C8FF"
    elif mph < 8.0:
        return "#00E060"
    elif mph < 10.0:
        return "#FFCC00"
    else:
        return "#FF4444"


def init():
    global _mock, _device, _h
    if _has_luma:
        try:
            serial = luma_spi(port=SPI_BUS, device=SPI_DEVICE, gpio_DC=GC9A01_DC_PIN, gpio_RST=GC9A01_RST_PIN, bus_speed_hz=SPI_SPEED_HZ)
            _device = gc9a01(serial, width=SCREEN_W, height=SCREEN_H, rotate=0)
            _mock = False
            if _has_hw:
                try:
                    _h = lgpio.gpiochip_open(0)
                    lgpio.gpio_claim_output(_h, GC9A01_BL_PIN)
                    lgpio.gpio_write(_h, GC9A01_BL_PIN, 1)
                except Exception:
                    pass
            return
        except Exception as e:
            if DEBUG:
                print(f"[DISPLAY] luma init failed: {e} — mock")
    if _has_hw:
        try:
            _h = lgpio.gpiochip_open(0)
            lgpio.gpio_claim_output(_h, GC9A01_RST_PIN)
            lgpio.gpio_claim_output(_h, GC9A01_DC_PIN)
            lgpio.gpio_claim_output(_h, GC9A01_BL_PIN)
            lgpio.gpio_write(_h, GC9A01_BL_PIN, 1)
            lgpio.gpio_write(_h, GC9A01_RST_PIN, 0)
            time.sleep(0.05)
            lgpio.gpio_write(_h, GC9A01_RST_PIN, 1)
            time.sleep(0.12)
            _mock = False
            return
        except Exception as e:
            if DEBUG:
                print(f"[DISPLAY] gpio init failed: {e}")
    _mock = True
    if DEBUG:
        print("[DISPLAY] mock mode")

_last_draw = 0


def update(speed_mph, incline, elapsed_sec, safety, running):
    global _last_draw
    if _mock:
        return
    if DEBUG and int(time.monotonic() * 2) % 10 == 0:
        pass
    if _device is not None and _has_luma:
        try:
            with canvas(_device) as draw:
                draw.rectangle((0, 0, SCREEN_W, SCREEN_H), fill="black")
                if safety:
                    draw.text((SCREEN_W // 2, SCREEN_H // 2), "!! STOPPED !!\nSet speed to 0", fill="red", anchor="mm", align="center")
                    return
                colour = _speed_colour(speed_mph)
                pct = max(0.0, min(1.0, speed_mph / 12.0))
                draw.ellipse((15, 15, SCREEN_W - 15, SCREEN_H - 15), outline="#333333", width=16)
                if pct > 0:
                    start = ARC_START
                    end = ARC_START + pct * (360 - (ARC_START - ARC_END) if ARC_END < ARC_START else ARC_END - ARC_START)
                    draw.arc((15, 15, SCREEN_W - 15, SCREEN_H - 15), start, end, fill=colour, width=16)
                draw.text((SCREEN_W // 2, SCREEN_H // 2 - 20), f"{speed_mph:.1f}", fill="white", anchor="mm")
                draw.text((SCREEN_W // 2, SCREEN_H // 2 + 8), "mph", fill="#AAAAAA", anchor="mm")
                draw.text((SCREEN_W // 2, 70), f"Incline: {incline}", fill="#FFCC00", anchor="mm")
                mins = elapsed_sec // 60
                secs = elapsed_sec % 60
                draw.text((SCREEN_W // 2, SCREEN_H - 40), f"{mins:02d}:{secs:02d}", fill="#AAAAAA", anchor="mm")
                if not running and speed_mph < 0.01:
                    draw.text((SCREEN_W // 2, SCREEN_H // 2 + 40), "IDLE", fill="#888888", anchor="mm")
        except Exception as e:
            if DEBUG:
                print(f"[DISPLAY] draw failed: {e}")


def is_mock():
    return _mock


def shutdown():
    global _h
    if _h is not None:
        try:
            lgpio.gpiochip_close(_h)
        except Exception:
            pass
