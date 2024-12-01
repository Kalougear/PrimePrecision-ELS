#pragma once

#include "config.h"
#include "timer_base.h"

// Critical section macros
#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL() __enable_irq()

namespace STM32Step
{
    class Stepper;
    class TimerControl; // Forward declaration

    struct StepperStatus
    {
        bool enabled;
        bool running;
        int32_t currentPosition;
        int32_t targetPosition;
        uint32_t currentSpeed;
        int32_t stepsRemaining;
    };

    class Stepper
    {
    public:
        // Constructor
        Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin = 0);
        ~Stepper();

        // Basic control
        void enable();
        void disable();
        void stop();
        void emergencyStop();

        // Motion control
        bool moveSteps(int32_t steps, bool wait = false);
        void setSpeed(uint32_t speed);
        void setTargetPosition(int32_t position);
        void setRelativePosition(int32_t steps);
        void incrementCurrentPosition(int32_t increment);
        void ISR(void);

        // Configuration
        void setOperationMode(OperationMode mode);
        void setInvertDirection(bool invert);
        void setInvertEnable(bool invert);
        void setMaxSpeed(uint32_t speed);
        void setMicrosteps(uint32_t microsteps);

        // Status methods
        bool isRunning() const { return _running; }
        bool isEnabled() const { return _enabled; }
        bool isAtPosition(int32_t position) const;
        bool waitForIdle(uint32_t timeout_ms = 0);
        StepperStatus getStatus() const;

        // Getters
        int32_t getCurrentPosition() const { return _currentPosition; }
        int32_t getTargetPosition() const { return _targetPosition; }
        uint32_t getCurrentSpeed() const { return _currentSpeed; }
        OperationMode getOperationMode() const { return _config.mode; }
        uint32_t getMaxSpeed() const { return _config.maxSpeed; }
        uint32_t getMicrosteps() const { return _config.microsteps; }
        uint32_t calculateMaxSpeedForMode() const;

    protected:
        friend class TimerControl; // Allow TimerControl to access protected members

        // Hardware pins
        const uint8_t _stepPin;
        const uint8_t _dirPin;
        const uint8_t _enablePin;

        // Motion state
        volatile int32_t _currentPosition;
        volatile int32_t _targetPosition;
        volatile uint32_t _currentSpeed;
        volatile bool _enabled;
        volatile bool _running;
        volatile bool _currentDirection;
        volatile bool _directionChanged;

        // Configuration state
        MotorConfig _config;
        OperationMode _mode;

    private:
        // Internal control methods
        void setDirection(bool dir);
        void applyConfig();
        void initPins();
        void waitDirectionSetupTime() const;
        uint32_t validateSpeed(uint32_t speed) const;
        void enforceModeLimits();
        uint32_t _stepsPerFullStep;

        // State machine state
        uint16_t state;

        // GPIO manipulation methods
        void GPIO_SET_STEP();
        void GPIO_CLEAR_STEP();
        void GPIO_SET_DIRECTION();
        void GPIO_CLEAR_DIRECTION();
    };

} // namespace STM32Step
