import time
import signal
import sys

import display_gc9a01_pi
import connectivity
from treadmill_rpi import Treadmill
from config import DISPLAY_UPDATE_MS, DEBUG

treadmill = None
_last_display = 0
_last_lvgl = 0
_running = True


def _handle_signal(signum, frame):
    global _running
    _running = False


def main():
    global treadmill, _last_display, _running
    signal.signal(signal.SIGTERM, _handle_signal)
    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGHUP, _handle_signal)

    print("Tradmill Pi Zero W v1.1 starting")
    display_gc9a01_pi.init()
    connectivity.init()
    treadmill = Treadmill()
    print("Tradmill ready — pots idle = stop, GC9A01 240x240, MCP23017 0x20, ADS1115 0x48")

    _last_display = time.monotonic()
    try:
        while _running:
            now = time.monotonic()
            treadmill.update()
            if (now - _last_display) * 1000 >= DISPLAY_UPDATE_MS:
                display_gc9a01_pi.update(
                    treadmill.get_speed_mph(),
                    treadmill.get_incline_level(),
                    treadmill.get_elapsed_seconds(),
                    treadmill.is_safety_triggered(),
                    treadmill.is_running(),
                )
                _last_display = now
            connectivity.update(treadmill.get_speed_mph(), treadmill.get_elapsed_seconds(), treadmill.is_running())
            if DEBUG and int(now * 2) % 20 == 0:
                pass
            time.sleep(0.005)
    finally:
        print("Shutting down — PWM 0, relays off")
        if treadmill:
            treadmill.shutdown()
        display_gc9a01_pi.shutdown()
        sys.exit(0)


if __name__ == "__main__":
    main()
