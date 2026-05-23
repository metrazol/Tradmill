from machine import I2C, Pin
import ssd1306


class TreadmillDisplay:
    WIDTH = 128
    HEIGHT = 64

    def __init__(self, sda_pin, scl_pin):
        i2c = I2C(0, sda=Pin(sda_pin), scl=Pin(scl_pin), freq=400000)
        self.oled = ssd1306.SSD1306_I2C(self.WIDTH, self.HEIGHT, i2c)
        self.oled.fill(0)
        self.oled.show()

    def update(self, speed_mph, incline_level, elapsed_seconds, safety=False, running=True):
        self.oled.fill(0)

        if safety:
            self.oled.text("!! STOPPED !!", 10, 24)
            self.oled.text("Set speed to 0", 4, 40)
        elif not running:
            self.oled.text("-- PAUSED --", 16, 24)
            self.oled.text("Click to start", 4, 40)
        else:
            mins = int(elapsed_seconds) // 60
            secs = int(elapsed_seconds) % 60

            self.oled.text("Speed", 0, 0)
            self.oled.text("{:.1f} mph".format(speed_mph), 0, 14)

            self.oled.text("Incline", 72, 0)
            self.oled.text("Lvl {}".format(incline_level), 80, 14)

            self.oled.text("Time", 44, 38)
            self.oled.text("{:02d}:{:02d}".format(mins, secs), 40, 52)

        self.oled.show()
