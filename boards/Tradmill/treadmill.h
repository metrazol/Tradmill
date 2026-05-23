#pragma once
#include <Arduino.h>

class Treadmill {
public:
    void begin();
    void update();

    // Adjust target speed by `ticks` encoder steps (positive = faster).
    void adjustSpeed(int32_t ticks);

    // Toggle between running and paused.  When paused, the motor stops
    // immediately but the target speed is preserved for when it resumes.
    void toggleRunning();

    float    getSpeedMph()       const { return _speedMph; }
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

    bool     _lastBtnUp       = false;
    bool     _lastBtnDown     = false;
    uint32_t _lastInclineMs   = 0;

    void _applySpeed();
    void _updateIncline();

    // Kills motor and resets target speed.  Used by safety-key logic so
    // the user must dial back to 0 before the safety latch clears.
    void _stopAll();
};
