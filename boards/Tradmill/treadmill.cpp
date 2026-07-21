#include "treadmill.h"
#include "config.h"

// PWM full-scale duty value derived from the board's configured resolution.
// uint32_t covers both 8-bit (GC9A01, max 255) and 13-bit (ZX2D80CE02S, max 8191).
static const uint32_t MAX_DUTY = (1u << PWM_RESOLUTION) - 1u;

// Define a PWM channel for the ESP32 Core v2 API
static const uint8_t PWM_CHANNEL = 0;

void Treadmill::begin() {
    // v2 API: setup channel, attach pin to channel, write to channel
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(SPEED_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    // MCP23017 on its own I2C bus (Wire1), separate from any display/touch bus.
    Wire1.begin(MCP_I2C_SDA, MCP_I2C_SCL);
    _mcp.begin_I2C(MCP_I2C_ADDR, &Wire1);

    _mcp.pinMode(MCP_INCLINE_UP_PIN,   OUTPUT); _mcp.digitalWrite(MCP_INCLINE_UP_PIN,   LOW);
    _mcp.pinMode(MCP_INCLINE_DOWN_PIN, OUTPUT); _mcp.digitalWrite(MCP_INCLINE_DOWN_PIN, LOW);

    if (MCP_INCLINE_UP_BTN   != -1) _mcp.pinMode(MCP_INCLINE_UP_BTN,   INPUT_PULLUP);
    if (MCP_INCLINE_DOWN_BTN != -1) _mcp.pinMode(MCP_INCLINE_DOWN_BTN, INPUT_PULLUP);

    if (MCP_SAFETY_KEY_PIN != -1) {
        _mcp.pinMode(MCP_SAFETY_KEY_PIN, INPUT_PULLUP);
    }
}

void Treadmill::adjustSpeed(int32_t ticks) {
    if (_stopping) return;
    _targetSpeedMph += ticks * SPEED_STEP_MPH;
    _targetSpeedMph = constrain(_targetSpeedMph, 0.0f, MAX_SPEED_MPH);
}

void Treadmill::adjustIncline(int32_t steps) {
    if (steps == 0) return;
    int dir = (steps > 0) ? 1 : -1;
    int newLevel = constrain(_inclineLevel + dir, MIN_INCLINE_LEVEL, MAX_INCLINE_LEVEL);
    if (newLevel == _inclineLevel) return;   // already at a limit
    _inclineLevel       = newLevel;
    // Pulse the incline relay so the motor physically moves the deck.
    _inclinePulseDir    = dir;
    _inclinePulseEndMs  = millis() + INCLINE_DEBOUNCE_MS;
}

void Treadmill::toggleRunning() {
    if (_stopping) {
        // Cancel the ramp; resume at whatever speed we've ramped down to.
        _stopping = false;
    } else if (_running) {
        if (_targetSpeedMph < 0.01f) {
            // Already at zero — stop immediately.
            ledcWrite(PWM_CHANNEL, 0);
            _running = false;
        } else {
            _stopping        = true;
            _stopRampFrom    = _targetSpeedMph;
            _stopRampStartMs = millis();
        }
    } else {
        _running = true;
    }
}

void Treadmill::update() {
    bool safetyActive = (MCP_SAFETY_KEY_PIN != -1) && (_mcp.digitalRead(MCP_SAFETY_KEY_PIN) == LOW);

    if (safetyActive && !_safetyTriggered) {
        _stopAll();
        _safetyTriggered = true;
        _running  = false;
        _stopping = false;
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
        // Still service incline so on-screen / physical incline buttons work
        // while the belt is paused.
        _updateIncline();
        return;
    }

    if (_stopping) {
        uint32_t elapsed = millis() - _stopRampStartMs;
        if (elapsed >= 3000) {
            _stopAll();
            _running  = false;
            _stopping = false;
        } else {
            _targetSpeedMph = _stopRampFrom * (1.0f - (float)elapsed / 3000.0f);
            _applySpeed();
            if (_sessionRunning) {
                _elapsedSeconds = (millis() - _sessionStartMs) / 1000;
            }
        }
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
        ledcWrite(PWM_CHANNEL, 0);
        return;
    }
    uint32_t duty = (uint32_t)((_speedMph / MAX_SPEED_MPH) * MAX_DUTY_PCT * MAX_DUTY);
    ledcWrite(PWM_CHANNEL, duty);
}

void Treadmill::_updateIncline() {
    uint32_t now    = millis();
    bool debounceOk = (now - _lastInclineMs) > INCLINE_DEBOUNCE_MS;

    bool up   = (MCP_INCLINE_UP_BTN   != -1) && (_mcp.digitalRead(MCP_INCLINE_UP_BTN)   == LOW);
    bool down = (MCP_INCLINE_DOWN_BTN != -1) && (_mcp.digitalRead(MCP_INCLINE_DOWN_BTN) == LOW);

    // A UI-driven incline pulse still in progress drives the relay too.
    bool pulseUp   = (_inclinePulseDir > 0) && (now < _inclinePulseEndMs);
    bool pulseDown = (_inclinePulseDir < 0) && (now < _inclinePulseEndMs);
    if (_inclinePulseDir != 0 && now >= _inclinePulseEndMs) {
        _inclinePulseDir = 0;   // pulse finished
    }

    if ((up || pulseUp) && !(down || pulseDown)) {
        _mcp.digitalWrite(MCP_INCLINE_UP_PIN,   HIGH);
        _mcp.digitalWrite(MCP_INCLINE_DOWN_PIN, LOW);
        if (up && !_lastBtnUp && debounceOk) {   // physical button edge: count a level
            _inclineLevel  = min(_inclineLevel + 1, MAX_INCLINE_LEVEL);
            _lastInclineMs = now;
        }
    } else if ((down || pulseDown) && !(up || pulseUp)) {
        _mcp.digitalWrite(MCP_INCLINE_UP_PIN,   LOW);
        _mcp.digitalWrite(MCP_INCLINE_DOWN_PIN, HIGH);
        if (down && !_lastBtnDown && debounceOk) {
            _inclineLevel  = max(_inclineLevel - 1, MIN_INCLINE_LEVEL);
            _lastInclineMs = now;
        }
    } else {
        _mcp.digitalWrite(MCP_INCLINE_UP_PIN,   LOW);
        _mcp.digitalWrite(MCP_INCLINE_DOWN_PIN, LOW);
    }

    _lastBtnUp   = up;
    _lastBtnDown = down;
}

void Treadmill::_stopAll() {
    ledcWrite(PWM_CHANNEL, 0);
    _mcp.digitalWrite(MCP_INCLINE_UP_PIN,   LOW);
    _mcp.digitalWrite(MCP_INCLINE_DOWN_PIN, LOW);
    _speedMph       = 0.0f;
    _targetSpeedMph = 0.0f;  // safety reset: user must dial back to 0 to clear latch
}