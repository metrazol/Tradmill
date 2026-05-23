#include "treadmill.h"
#include "config.h"

// PWM full-scale duty value derived from the board's configured resolution.
// uint32_t covers both 8-bit (GC9A01, max 255) and 13-bit (ZX2D80CE02S, max 8191).
static const uint32_t MAX_DUTY = (1u << PWM_RESOLUTION) - 1u;

void Treadmill::begin() {
    ledcAttach(SPEED_PIN, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcWrite(SPEED_PIN, 0);

    pinMode(INCLINE_UP_PIN,   OUTPUT); digitalWrite(INCLINE_UP_PIN,   LOW);
    pinMode(INCLINE_DOWN_PIN, OUTPUT); digitalWrite(INCLINE_DOWN_PIN, LOW);

    pinMode(INCLINE_UP_BTN,   INPUT_PULLUP);
    pinMode(INCLINE_DOWN_BTN, INPUT_PULLUP);

    if (SAFETY_KEY_PIN != -1) {
        pinMode(SAFETY_KEY_PIN, INPUT);
    }
}

void Treadmill::adjustSpeed(int32_t ticks) {
    _targetSpeedMph += ticks * SPEED_STEP_MPH;
    _targetSpeedMph = constrain(_targetSpeedMph, 0.0f, MAX_SPEED_MPH);
}

void Treadmill::toggleRunning() {
    _running = !_running;
    if (!_running) {
        // Pause: kill the motor now but keep _targetSpeedMph so the belt
        // resumes at the same speed when the user clicks start again.
        ledcWrite(SPEED_PIN, 0);
        digitalWrite(INCLINE_UP_PIN,   LOW);
        digitalWrite(INCLINE_DOWN_PIN, LOW);
        _speedMph = 0.0f;
    }
}

void Treadmill::update() {
    bool safetyActive = (SAFETY_KEY_PIN != -1) && (digitalRead(SAFETY_KEY_PIN) == LOW);

    if (safetyActive && !_safetyTriggered) {
        _stopAll();
        _safetyTriggered = true;
        _running = false;
        return;
    }

    if (_safetyTriggered) {
        _stopAll();
        if (_targetSpeedMph < 0.01f) {
            _safetyTriggered = false;
        }
        return;
    }

    if (!_running) {
        return;
    }

    _updateIncline();
    _applySpeed();

    if (_speedMph > 0.01f && !_sessionRunning) {
        _sessionStartMs = millis();
        _sessionRunning = true;
    }
    if (_sessionRunning) {
        _elapsedSeconds = (millis() - _sessionStartMs) / 1000;
    }
}

void Treadmill::_applySpeed() {
    _speedMph = _targetSpeedMph;
    if (_speedMph < 0.01f) {
        ledcWrite(SPEED_PIN, 0);
        return;
    }
    uint32_t duty = (uint32_t)((_speedMph / MAX_SPEED_MPH) * MAX_DUTY_PCT * MAX_DUTY);
    ledcWrite(SPEED_PIN, duty);
}

void Treadmill::_updateIncline() {
    uint32_t now    = millis();
    bool debounceOk = (now - _lastInclineMs) > INCLINE_DEBOUNCE_MS;

    bool up   = (digitalRead(INCLINE_UP_BTN)   == LOW);
    bool down = (digitalRead(INCLINE_DOWN_BTN)  == LOW);

    if (up && !down) {
        digitalWrite(INCLINE_UP_PIN,   HIGH);
        digitalWrite(INCLINE_DOWN_PIN, LOW);
        if (!_lastBtnUp && debounceOk) {
            _inclineLevel  = min(_inclineLevel + 1, MAX_INCLINE_LEVEL);
            _lastInclineMs = now;
        }
    } else if (down && !up) {
        digitalWrite(INCLINE_UP_PIN,   LOW);
        digitalWrite(INCLINE_DOWN_PIN, HIGH);
        if (!_lastBtnDown && debounceOk) {
            _inclineLevel  = max(_inclineLevel - 1, MIN_INCLINE_LEVEL);
            _lastInclineMs = now;
        }
    } else {
        digitalWrite(INCLINE_UP_PIN,   LOW);
        digitalWrite(INCLINE_DOWN_PIN, LOW);
    }

    _lastBtnUp   = up;
    _lastBtnDown = down;
}

void Treadmill::_stopAll() {
    ledcWrite(SPEED_PIN, 0);
    digitalWrite(INCLINE_UP_PIN,   LOW);
    digitalWrite(INCLINE_DOWN_PIN, LOW);
    _speedMph       = 0.0f;
    _targetSpeedMph = 0.0f;  // safety reset: user must dial back to 0 to clear latch
}
