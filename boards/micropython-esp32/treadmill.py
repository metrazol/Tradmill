import time
from machine import Pin, ADC, PWM
from config import (
    SPEED_PIN, INCLINE_UP_PIN, INCLINE_DOWN_PIN,
    SPEED_POT_PIN, INCLINE_UP_BTN, INCLINE_DOWN_BTN,
    SAFETY_KEY_PIN, PWM_FREQ, MAX_DUTY_CYCLE,
    SPEED_ANALOG_THRESHOLD, MAX_SPEED_MPH,
    MAX_INCLINE_LEVEL, MIN_INCLINE_LEVEL, INCLINE_BTN_ACTIVE_LOW,
    ENCODER_SW,
)

_MAX_ADC = 4095   # ESP32 12-bit ADC
_MAX_DUTY = 1023  # MicroPython PWM duty range
_INCLINE_DEBOUNCE_MS = 300
_SW_DEBOUNCE_MS = 50


class Treadmill:
    def __init__(self):
        self.speed_pwm = PWM(Pin(SPEED_PIN), freq=PWM_FREQ, duty=0)
        self.incline_up_out = Pin(INCLINE_UP_PIN, Pin.OUT, value=0)
        self.incline_down_out = Pin(INCLINE_DOWN_PIN, Pin.OUT, value=0)

        self.speed_pot = ADC(Pin(SPEED_POT_PIN))
        self.speed_pot.atten(ADC.ATTN_11DB)  # full 0–3.3V range

        pull = Pin.PULL_UP if INCLINE_BTN_ACTIVE_LOW else Pin.PULL_DOWN
        self.btn_up = Pin(INCLINE_UP_BTN, Pin.IN, pull)
        self.btn_down = Pin(INCLINE_DOWN_BTN, Pin.IN, pull)

        self.safety_key = Pin(SAFETY_KEY_PIN, Pin.IN) if SAFETY_KEY_PIN is not None else None
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
        safety_active = self.safety_key is not None and self.safety_key.value() == 0

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

    def _btn_pressed(self, pin):
        return pin.value() == 0 if INCLINE_BTN_ACTIVE_LOW else pin.value() == 1

    def _update_incline(self):
        now = time.ticks_ms()
        debounce_ok = time.ticks_diff(now, self._last_incline_ms) > _INCLINE_DEBOUNCE_MS

        up = self._btn_pressed(self.btn_up)
        down = self._btn_pressed(self.btn_down)

        if up and not down:
            self.incline_up_out.value(1)
            self.incline_down_out.value(0)
            if not self._last_btn_up and debounce_ok:
                self.incline_level = min(self.incline_level + 1, MAX_INCLINE_LEVEL)
                self._last_incline_ms = now
        elif down and not up:
            self.incline_up_out.value(0)
            self.incline_down_out.value(1)
            if not self._last_btn_down and debounce_ok:
                self.incline_level = max(self.incline_level - 1, MIN_INCLINE_LEVEL)
                self._last_incline_ms = now
        else:
            self.incline_up_out.value(0)
            self.incline_down_out.value(0)

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
        self.incline_up_out.value(0)
        self.incline_down_out.value(0)
        self.speed_mph = 0.0
