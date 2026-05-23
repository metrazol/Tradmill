import time
from treadmill import Treadmill
from config import I2C_SDA, I2C_SCL, DISPLAY_UPDATE_MS, DEBUG


def main():
    treadmill = Treadmill()

    display = None
    try:
        from display import TreadmillDisplay
        display = TreadmillDisplay(I2C_SDA, I2C_SCL)
    except Exception:
        pass  # run headless if no display is wired up

    last_display_ms = 0

    while True:
        treadmill.update()

        if display is not None:
            now = time.ticks_ms()
            if time.ticks_diff(now, last_display_ms) >= DISPLAY_UPDATE_MS:
                display.update(
                    treadmill.speed_mph,
                    treadmill.incline_level,
                    treadmill.elapsed_seconds,
                    treadmill.safety_triggered,
                    treadmill.running,
                )
                last_display_ms = now

        if DEBUG:
            print("spd:{:.1f}mph inc:{} t:{}s safety:{}".format(
                treadmill.speed_mph,
                treadmill.incline_level,
                treadmill.elapsed_seconds,
                treadmill.safety_triggered,
            ))

        time.sleep_ms(5)


main()
