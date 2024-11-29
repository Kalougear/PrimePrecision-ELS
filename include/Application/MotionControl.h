#pragma once

#include "Core/encoder_timer.h"
#include "Core/sync_timer.h"
#include <STM32Step.h>

class MotionControl
{
public:
    // Motion modes
    enum class Mode
    {
        IDLE,
        THREADING,
        TURNING,
        FEEDING
    };

    // Configuration for the motion system
    struct Config
    {
        float thread_pitch;      // mm or TPI
        float leadscrew_pitch;   // mm
        uint32_t steps_per_rev;  // Full steps per revolution
        uint16_t microsteps;     // Microstep setting
        bool reverse_direction;  // Reverse sync direction
        uint32_t sync_frequency; // Hz
    };

    // Pin configuration for motion control
    struct MotionPins
    {
        uint32_t step_pin;   // Stepper step pin
        uint32_t dir_pin;    // Stepper direction pin
        uint32_t enable_pin; // Stepper enable pin
    };

    MotionControl();
    ~MotionControl();

    // Initialization
    bool begin();
    void end();

    // Mode control
    void setMode(Mode mode);
    Mode getMode() const { return _currentMode; }
    bool isRunning() const { return _running; }

    // Configuration
    void setConfig(const Config &config);
    const Config &getConfig() const { return _config; }

    // Motion control
    void startMotion();
    void stopMotion();
    void emergencyStop();

    MotionControl(const MotionPins &pins);

    // Status
    struct Status
    {
        Mode mode;
        bool running;
        int32_t encoder_position;
        int32_t stepper_position;
        int16_t spindle_rpm;
        bool error;
        const char *error_message;
    };
    Status getStatus() const;

private:
    // Components
    EncoderTimer _encoder;
    STM32Step::Stepper *_stepper;
    SyncTimer _syncTimer;
    MotionPins _pins;

    // State
    Mode _currentMode;
    bool _running;
    bool _error;
    const char *_errorMsg;
    Config _config;

    // Internal methods
    void configureForMode(Mode mode);
    void handleError(const char *msg);
    void updateSyncParameters();
};
