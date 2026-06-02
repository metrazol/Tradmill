import time
from machine import Pin, ADC, PWM, I2C
from mcp23017 import MCP23017, IN, OUT
from config import (
    SPEED_PIN, SPEED_POT_PIN,
    PWM_FREQ, MAX_DUTY_CYCLE,
    SPEED_ANALOG_THRESHOLD, MAX_SPEED_MPH,
    MAX_INCLINE_LEVEL, MIN_INCLINE_LEVEL, INCLINE_BTN_ACTIVE_LOW,
    ENCODER_SW,
    MCP_I2C_SDA, MCP_I2C_SCL, MCP_I2C_ADDR,
    MCP_INCLINE_UP, MCP_INCLINE_DOWN,
    MCP_INCLINE_UP_BTN, MCP_INCLINE_DOWN_BTN, MCP_SAFETY_KEY,
)

_MAX_ADC = 4095   # ESP32 12-bit ADC
_MAX_DUTY = 1023  # MicroPython PWM duty range
_INCLINE_DEBOUNCE_MS = 300
_SW_DEBOUNCE_MS = 50


class Treadmill:
    def __init__(self):
        self.speed_pwm = PWM(Pin(SPEED_PIN), freq=PWM_FREQ, duty=0)

        # MCP23017 on a dedicated I2C bus carries incline relays, incline
        # buttons, and the safety key.  Speed PWM stays on a native GPIO.
        i2c = I2C(1, sda=Pin(MCP_I2C_SDA), scl=Pin(MCP_I2C_SCL), freq=400000)
        self.mcp = MCP23017(i2c, MCP_I2C_ADDR)
        self.mcp.pin_mode(MCP_INCLINE_UP, OUT)
        self.mcp.pin_mode(MCP_INCLINE_DOWN, OUT)
        self.mcp.output(MCP_INCLINE_UP, 0)
        self.mcp.output(MCP_INCLINE_DOWN, 0)
        # Buttons use the expander's internal pull-ups when active-low.
        self.mcp.pin_mode(MCP_INCLINE_UP_BTN, IN, pull_up=INCLINE_BTN_ACTIVE_LOW)
        self.mcp.pin_mode(MCP_INCLINE_DOWN_BTN, IN, pull_up=INCLINE_BTN_ACTIVE_LOW)

        self.speed_pot = ADC(Pin(SPEED_POT_PIN))
        self.speed_pot.atten(ADC.ATTN_11DB)  # full 0–3.3V range

        self.safety_key = MCP_SAFETY_KEY
        if self.safety_key is not None:
            self.mcp.pin_mode(self.safety_key, IN, pull_up=True)
        self.encoder_sw = Pin(ENCODER_SW, Pin.IN, Pin.PULL_UP)

        self.safety_triggered = False
        self.incline_level = 0
        self.speed_mph = 0.0
        self.elapsed_seconds = 0
        self.running = False

        self._last_btn_up = False
        self._last_btn_down = False
        self._last_incline_ms = 0
        self._last_sw = False
        self._last_sw_ms = 0
        self._session_start = None

    def update(self):
        pot_value = self.speed_pot.read()
        safety_active = self.safety_key is not None and self.mcp.input(self.safety_key) == 0

        if safety_active and not self.safety_triggered:
            self._stop_all()
            self.safety_triggered = True
            self.running = False
            return

        if self.safety_triggered:
            self._stop_all()
            if pot_value < SPEED_ANALOG_THRESHOLD:
                self.safety_triggered = False
            return

        self._check_encoder_click()

        if not self.running:
            self._stop_all()
            return

        self._update_incline()
        self._update_speed(pot_value)

        if self.speed_mph > 0 and self._session_start is None:
            self._session_start = time.time()

        if self._session_start is not None:
            self.elapsed_seconds = time.time() - self._session_start

    def _check_encoder_click(self):
        now = time.ticks_ms()
        pressed = self.encoder_sw.value() == 0  # active-low
        if pressed and not self._last_sw and time.ticks_diff(now, self._last_sw_ms) > _SW_DEBOUNCE_MS:
            self.running = not self.running
            self._last_sw_ms = now
        self._last_sw = pressed

    def _btn_pressed(self, mcp_pin):
        val = self.mcp.input(mcp_pin)
        return val == 0 if INCLINE_BTN_ACTIVE_LOW else val == 1

    def _update_incline(self):
        now = time.ticks_ms()
        debounce_ok = time.ticks_diff(now, self._last_incline_ms) > _INCLINE_DEBOUNCE_MS

        up = self._btn_pressed(MCP_INCLINE_UP_BTN)
        down = self._btn_pressed(MCP_INCLINE_DOWN_BTN)

        if up and not down:
            self.mcp.output(MCP_INCLINE_UP, 1)
            self.mcp.output(MCP_INCLINE_DOWN, 0)
            if not self._last_btn_up and debounce_ok:
                self.incline_level = min(self.incline_level + 1, MAX_INCLINE_LEVEL)
                self._last_incline_ms = now
        elif down and not up:
            self.mcp.output(MCP_INCLINE_UP, 0)
            self.mcp.output(MCP_INCLINE_DOWN, 1)
            if not self._last_btn_down and debounce_ok:
                self.incline_level = max(self.incline_level - 1, MIN_INCLINE_LEVEL)
                self._last_incline_ms = now
        else:
            self.mcp.output(MCP_INCLINE_UP, 0)
            self.mcp.output(MCP_INCLINE_DOWN, 0)

        self._last_btn_up = up
        self._last_btn_down = down

    def _update_speed(self, pot_value):
        if pot_value <= SPEED_ANALOG_THRESHOLD:
            self.speed_pwm.duty(0)
            self.speed_mph = 0.0
            return

        duty = int((pot_value / _MAX_ADC) * MAX_DUTY_CYCLE * _MAX_DUTY)
        duty = min(duty, int(MAX_DUTY_CYCLE * _MAX_DUTY))
        self.speed_pwm.duty(duty)
        self.speed_mph = (duty / (_MAX_DUTY * MAX_DUTY_CYCLE)) * MAX_SPEED_MPH

    def _stop_all(self):
        self.speed_pwm.duty(0)
        self.mcp.output(MCP_INCLINE_UP, 0)
        self.mcp.output(MCP_INCLINE_DOWN, 0)
        self.speed_mph = 0.0
