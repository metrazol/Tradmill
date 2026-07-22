#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

class Treadmill {
public:
    void begin();
    void update();

    // Adjust target speed by `ticks` encoder steps (positive = faster).
    void adjustSpeed(int32_t ticks);

    // Adjust incline by `steps` levels (positive = incline up, negative = down).
    // Drives the incline relay for a fixed pulse so on-screen buttons work the
    // same as the physical incline buttons on the MCP23017.
    void adjustIncline(int32_t steps);

    // Toggle between running and paused.  When paused, the motor stops
    // immediately but the target speed is preserved for when it resumes.
    void toggleRunning();

    float    getSpeedMph()       const { return _targetSpeedMph; }
    int      getInclineLevel()   const { return _inclineLevel; }
    uint32_t getElapsedSeconds() const { return _elapsedSeconds; }
    bool     isSafetyTriggered() const { return _safetyTriggered; }
    bool     isRunning()         const { return _running; }

private:
    float    _targetSpeedMph  = 0.0f;
    float    _speedMph        = 0.0f;
    int      _inclineLevel    = 0;
    uint32_t _elapsedSeconds  = 0;
    bool     _safetyTriggered = false;
    bool     _running         = false;

    uint32_t _sessionStartMs  = 0;
    bool     _sessionRunning  = false;

    bool     _stopping        = false;
    uint32_t _stopRampStartMs = 0;
    float    _stopRampFrom    = 0.0f;

    bool     _lastBtnUp       = false;
    bool     _lastBtnDown     = false;
    uint32_t _lastInclineMs   = 0;

    // UI-driven incline relay pulse (separate from the held-button logic).
    int      _inclinePulseDir   = 0;   // -1 down, +1 up, 0 idle
    uint32_t _inclinePulseEndMs = 0;

    // MCP23017 I2C expander carrying incline relays, incline buttons,
    // and the safety key.  Speed PWM stays on a native LEDC GPIO.
    // _mcpOk is false when the expander doesn't respond at begin() time
    // (e.g. during bench testing without hardware connected).
    Adafruit_MCP23X17 _mcp;
    bool              _mcpOk = false;

    void _applySpeed();
    void _updateIncline();

    // Kills motor and resets target speed.  Used by safety-key logic so
    // the user must dial back to 0 before the safety latch clears.
    void _stopAll();
};
