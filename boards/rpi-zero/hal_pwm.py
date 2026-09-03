import os
import time
import atexit
import signal

from config import PWM_PERIOD_NS, MAX_DUTY_PCT

PWMCHIP = 0
PWM_CH = 1
PERIOD = PWM_PERIOD_NS
MAX_DUTY_NS = int(PERIOD * MAX_DUTY_PCT)

_chip_path = f"/sys/class/pwm/pwmchip{PWMCHIP}"
_pwm_path = f"{_chip_path}/pwm{PWM_CH}"
_export_path = f"{_chip_path}/export"
_period_path = f"{_pwm_path}/period"
_duty_path = f"{_pwm_path}/duty_cycle"
_enable_path = f"{_pwm_path}/enable"

_mock = not os.path.exists(_chip_path)
_current_duty = 0


def _write(path, value):
    with open(path, "w") as f:
        f.write(str(value))


def _ensure_exported():
    if _mock:
        return
    if not os.path.exists(_pwm_path):
        _write(_export_path, PWM_CH)
        time.sleep(0.2)
    _write(_period_path, PERIOD)
    _write(_duty_path, 0)
    _write(_enable_path, 1)


def init():
    _ensure_exported()
    set_duty_pct(0)
    atexit.register(stop)
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            signal.signal(sig, lambda s, f: stop())
        except (ValueError, AttributeError):
            pass
    if hasattr(signal, "SIGHUP"):
        try:
            signal.signal(signal.SIGHUP, lambda s, f: stop())
        except (ValueError, AttributeError):
            pass


def set_duty_pct(pct):
    global _current_duty
    pct = max(0.0, min(float(pct), MAX_DUTY_PCT))
    duty_ns = int(PERIOD * pct)
    _current_duty = duty_ns
    if _mock:
        return
    try:
        _write(_duty_path, duty_ns)
    except OSError:
        pass


def set_speed_mph(speed, max_speed):
    if speed < 0.01:
        set_duty_pct(0)
        return
    pct = (speed / max_speed) * MAX_DUTY_PCT
    set_duty_pct(pct)


def stop():
    global _current_duty
    _current_duty = 0
    if _mock:
        return
    try:
        _write(_duty_path, 0)
    except OSError:
        pass


def is_mock():
    return _mock
