#pragma once

#include <Arduino.h>
#include "Hardware/EncoderTimer.h"
#include "Motion/SyncTimer.h"
#include <STM32Step.h>

class MotionControl
{
public:
    // Pin configuration structure
    struct MotionPins
    {
        uint32_t step_pin;
        uint32_t dir_pin;
        uint32_t enable_pin;
    };

    // Configuration structure
    struct Config
    {
        float thread_pitch;      // Desired thread pitch
        float leadscrew_pitch;   // Leadscrew pitch
        uint32_t steps_per_rev;  // Steps per revolution
        uint32_t microsteps;     // Microsteps
        bool reverse_direction;  // Reverse direction
        uint32_t sync_frequency; // Sync update frequency (Hz)
    };

    // Status structure
    struct Status
    {
        int32_t encoder_position;
        int32_t stepper_position;
        int16_t spindle_rpm;
        bool error;
        const char *error_message;
    };

    // Operating modes
    enum class Mode
    {
        IDLE,
        THREADING,
        TURNING,
        FEEDING
    };

    // Constructors
    MotionControl();
    explicit MotionControl(const MotionPins &pins);
    ~MotionControl();

    // Basic operations
    bool begin();
    void end();
    void update(); // New method to process sync requests

    // Configuration
    void setConfig(const Config &config);
    void setMode(Mode mode);

    // Motion control
    void startMotion();
    void stopMotion();
    void emergencyStop();

    // Status
    Status getStatus() const;

private:
    // Components
    EncoderTimer _encoder;
    SyncTimer _syncTimer;
    STM32Step::Stepper *_stepper;

    // Configuration
    MotionPins _pins;
    Config _config;
    Mode _currentMode;

    // State
    volatile bool _running;
    volatile bool _error;
    const char *_errorMsg;

    // Private methods
    void configureForMode(Mode mode);
    void handleError(const char *msg);
    void updateSyncParameters();
};
