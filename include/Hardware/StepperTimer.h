#pragma once

#include <Arduino.h>
#include <STM32Step.h>
#include "Config/SystemConfig.h"

class StepperTimer {
public:
    // Operating modes
    enum class Mode {
        IDLE,
        TURNING,
        THREADING,
        MANUAL
    };
    
    // Status structure
    struct Status {
        int32_t position;
        int32_t target_position;
        uint32_t speed;
        bool enabled;
        bool running;
        bool error;
        const char* error_message;
    };
    
    // Pin configuration
    struct PinConfig {
        uint8_t step_pin;
        uint8_t dir_pin;
        uint8_t enable_pin;
    };
    
    StepperTimer();
    ~StepperTimer();
    
    // Basic operations
    bool begin(const PinConfig& pins);
    void end();
    
    // Stepper control
    void enable(bool enable = true);
    void disable();
    void setMode(Mode mode);
    void setMicrosteps(uint16_t microsteps);
    bool setSpeed(uint32_t steps_per_second);
    
    // Position control
    void setPosition(int32_t position);
    void setRelativePosition(int32_t steps);
    int32_t getPosition() const;
    void stop();
    void emergencyStop();
    void resetPosition();
    
    // Status
    Status getStatus() const;
    bool isRunning() const;
    bool hasError() const;
    const char* getErrorMessage() const;
    
private:
    STM32Step::Stepper* _stepper;
    Mode _currentMode;
    bool _error;
    const char* _errorMsg;
    
    // Helper methods
    void configureForMode(Mode mode);
    void handleError(const char* msg);
};