import time
import hal_pwm
from hal_mcp import MCP23017
from hal_adc import ADCReader
from config import (
    MAX_SPEED_MPH, SPEED_STEP_MPH, SPEED_LEVELS, SPEED_POT_THRESHOLD_NORM,
    MAX_INCLINE_LEVEL, MIN_INCLINE_LEVEL, INCLINE_DEBOUNCE_MS,
    INCLINE_INTER_PULSE_GAP_MS, MCP_I2C_ADDR, MCP_INCLINE_UP_PIN,
    MCP_INCLINE_DOWN_PIN, MCP_INCLINE_UP_BTN, MCP_INCLINE_DOWN_BTN,
    MCP_SAFETY_KEY_PIN, I2C_BUS, ADC_HYST_SPEED, ADC_HYST_INCLINE,
    ADC_RAW_MIN, ADC_RAW_MAX, STOP_RAMP_MS
)


class Treadmill:
    def __init__(self):
        hal_pwm.init()
        self.mcp = MCP23017(bus=I2C_BUS, address=MCP_I2C_ADDR)
        self.adc = ADCReader(bus_num=I2C_BUS, address=0x48)
        self._setup_mcp()
        self._target_speed = 0.0
        self._speed_mph = 0.0
        self._incline_level = 0
        self._elapsed_seconds = 0
        self._running = False
        self._stopping = False
        self._stop_ramp_from = 0.0
        self._stop_ramp_start = 0
        self._session_start = 0
        self._session_running = False
        self._last_btn_up = False
        self._last_btn_down = False
        self._last_incline_ms = 0
        self._incline_pulse_dir = 0
        self._incline_pulse_end = 0
        self._raw_speed_filt = ADC_RAW_MIN
        self._raw_incline_filt = ADC_RAW_MIN
        self._speed_level = 0
        self._incline_target = 0
        self._speed_buf = []
        self._incline_buf = []

    def _setup_mcp(self):
        if not self.mcp.ok:
            return
        self.mcp.pin_mode(MCP_INCLINE_UP_PIN, 0)
        self.mcp.pin_mode(MCP_INCLINE_DOWN_PIN, 0)
        self.mcp.output(MCP_INCLINE_UP_PIN, 0)
        self.mcp.output(MCP_INCLINE_DOWN_PIN, 0)
        if MCP_INCLINE_UP_BTN != -1:
            self.mcp.pin_mode(MCP_INCLINE_UP_BTN, 1, pull_up=True)
        if MCP_INCLINE_DOWN_BTN != -1:
            self.mcp.pin_mode(MCP_INCLINE_DOWN_BTN, 1, pull_up=True)
        if MCP_SAFETY_KEY_PIN != -1:
            self.mcp.pin_mode(MCP_SAFETY_KEY_PIN, 1, pull_up=True)

    def _median(self, buf):
        s = sorted(buf)
        return s[len(s) // 2]

    def _filter_raw(self, raw, buf, filt):
        buf.append(raw)
        if len(buf) > 8:
            buf.pop(0)
        med = self._median(buf)
        return int(filt * 0.8 + med * 0.2)

    def _poll_pots(self):
        raw_s = self.adc.read_speed_raw()
        raw_i = self.adc.read_incline_raw()
        if raw_s == 0 and raw_i == 0 and not self.adc.ok:
            return
        self._raw_speed_filt = self._filter_raw(raw_s, self._speed_buf, self._raw_speed_filt)
        self._raw_incline_filt = self._filter_raw(raw_i, self._incline_buf, self._raw_incline_filt)

    def _quantize_speed(self):
        span = ADC_RAW_MAX - ADC_RAW_MIN
        if span <= 0:
            return 0
        norm = (self._raw_speed_filt - ADC_RAW_MIN) / span
        norm = max(0.0, min(1.0, norm))
        if norm < SPEED_POT_THRESHOLD_NORM:
            return 0
        desired = int(round(norm * SPEED_LEVELS))
        desired = max(0, min(SPEED_LEVELS, desired))
        if desired == self._speed_level:
            return desired
        bin_w = span / SPEED_LEVELS if SPEED_LEVELS else span
        boundary = ADC_RAW_MIN + max(desired, self._speed_level) * bin_w
        if desired > self._speed_level and self._raw_speed_filt < boundary + ADC_HYST_SPEED // 2:
            return self._speed_level
        if desired < self._speed_level and self._raw_speed_filt > boundary - ADC_HYST_SPEED // 2:
            return self._speed_level
        return desired

    def _quantize_incline(self):
        span = ADC_RAW_MAX - ADC_RAW_MIN
        if span <= 0:
            return 0
        levels = MAX_INCLINE_LEVEL - MIN_INCLINE_LEVEL + 1
        bin_w = span / levels
        naive = int(((self._raw_incline_filt - ADC_RAW_MIN) / span) * levels)
        naive = max(MIN_INCLINE_LEVEL, min(MAX_INCLINE_LEVEL, naive))
        if naive == self._incline_level:
            return naive
        boundary = ADC_RAW_MIN + (max(naive, self._incline_level) - MIN_INCLINE_LEVEL) * bin_w
        if naive > self._incline_level and self._raw_incline_filt < boundary + ADC_HYST_INCLINE // 2:
            return self._incline_level
        if naive < self._incline_level and self._raw_incline_filt > boundary - ADC_HYST_INCLINE // 2:
            return self._incline_level
        return naive

    def _apply_speed(self):
        self._speed_mph = self._target_speed
        if self._speed_mph < 0.01:
            hal_pwm.set_duty_pct(0)
            return
        hal_pwm.set_speed_mph(self._speed_mph, MAX_SPEED_MPH)

    def _stop_all(self):
        hal_pwm.set_duty_pct(0)
        if self.mcp.ok:
            self.mcp.output(MCP_INCLINE_UP_PIN, 0)
            self.mcp.output(MCP_INCLINE_DOWN_PIN, 0)
        self._speed_mph = 0.0
        self._target_speed = 0.0

    def _schedule_incline(self, now):
        self._incline_target = self._quantize_incline()
        if self._incline_target == self._incline_level:
            return
        if self._incline_pulse_dir != 0:
            return
        if now - self._last_incline_ms < INCLINE_INTER_PULSE_GAP_MS:
            return
        direction = 1 if self._incline_target > self._incline_level else -1
        self._incline_level = max(MIN_INCLINE_LEVEL, min(MAX_INCLINE_LEVEL, self._incline_level + direction))
        self._incline_pulse_dir = direction
        self._incline_pulse_end = now + INCLINE_DEBOUNCE_MS
        self._last_incline_ms = now

    def _update_incline(self, now):
        pulse_up = self._incline_pulse_dir > 0 and now < self._incline_pulse_end
        pulse_down = self._incline_pulse_dir < 0 and now < self._incline_pulse_end
        if self._incline_pulse_dir != 0 and now >= self._incline_pulse_end:
            self._incline_pulse_dir = 0
        up = False
        down = False
        if self.mcp.ok:
            if MCP_INCLINE_UP_BTN != -1:
                up = self.mcp.input(MCP_INCLINE_UP_BTN) == 0
            if MCP_INCLINE_DOWN_BTN != -1:
                down = self.mcp.input(MCP_INCLINE_DOWN_BTN) == 0
        debounce_ok = (now - self._last_incline_ms) > INCLINE_DEBOUNCE_MS
        if self.mcp.ok:
            if (up or pulse_up) and not (down or pulse_down):
                self.mcp.output(MCP_INCLINE_UP_PIN, 1)
                self.mcp.output(MCP_INCLINE_DOWN_PIN, 0)
                if up and not self._last_btn_up and debounce_ok:
                    self._incline_level = min(self._incline_level + 1, MAX_INCLINE_LEVEL)
                    self._last_incline_ms = now
            elif (down or pulse_down) and not (up or pulse_up):
                self.mcp.output(MCP_INCLINE_UP_PIN, 0)
                self.mcp.output(MCP_INCLINE_DOWN_PIN, 1)
                if down and not self._last_btn_down and debounce_ok:
                    self._incline_level = max(self._incline_level - 1, MIN_INCLINE_LEVEL)
                    self._last_incline_ms = now
            else:
                self.mcp.output(MCP_INCLINE_UP_PIN, 0)
                self.mcp.output(MCP_INCLINE_DOWN_PIN, 0)
        self._last_btn_up = up
        self._last_btn_down = down

    def update(self):
        now = int(time.monotonic() * 1000)
        self._poll_pots()
        speed_level = self._quantize_speed()
        self._speed_level = speed_level
        new_target = speed_level * SPEED_STEP_MPH
        new_target = max(0.0, min(MAX_SPEED_MPH, new_target))
        pot_idle = new_target < 0.01
        if pot_idle and self._running:
            if new_target >= 0.01:
                pass
            else:
                self._stopping = True
                self._stop_ramp_from = self._target_speed
                self._stop_ramp_start = now
                self._running = False
        if not pot_idle and not self._running and not self._stopping:
            self._running = True
            self._stopping = False
        if self._stopping:
            elapsed = now - self._stop_ramp_start
            if elapsed >= STOP_RAMP_MS:
                self._stop_all()
                self._stopping = False
                self._running = False
                self._session_running = False
            else:
                self._target_speed = self._stop_ramp_from * (1.0 - elapsed / STOP_RAMP_MS)
                self._apply_speed()
                if self._session_running:
                    self._elapsed_seconds = int(time.monotonic() - self._session_start)
            self._schedule_incline(now)
            self._update_incline(now)
            return
        if not self._running:
            self._target_speed = 0.0
            self._apply_speed()
            self._schedule_incline(now)
            self._update_incline(now)
            if self._session_running:
                self._elapsed_seconds = int(time.monotonic() - self._session_start)
            return
        self._target_speed = new_target
        self._schedule_incline(now)
        self._update_incline(now)
        self._apply_speed()
        if self._speed_mph > 0.01 and not self._session_running:
            self._session_start = time.monotonic()
            self._session_running = True
        if self._session_running:
            self._elapsed_seconds = int(time.monotonic() - self._session_start)

    def get_speed_mph(self):
        return self._target_speed

    def get_incline_level(self):
        return self._incline_level

    def get_elapsed_seconds(self):
        return self._elapsed_seconds

    def is_running(self):
        return self._running or self._stopping

    def is_safety_triggered(self):
        return False

    def shutdown(self):
        self._stop_all()
        try:
            self.mcp.close()
        except Exception:
            pass
        try:
            self.adc.close()
        except Exception:
            pass
        hal_pwm.stop()
