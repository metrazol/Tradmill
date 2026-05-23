from machine import Pin
import time

# Configure GPIO pin (commonly GPIO 2 for built-in LED)
led = Pin(2, Pin.OUT)

while True:
    led.on()
    time.sleep(.5)
    led.off()
    time.sleep(.2)