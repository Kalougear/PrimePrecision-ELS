#pragma once
#include "config.h"
#include "timer_base.h"
#include "Config/systemconfig.h"

namespace STM32Step
{
    /**
     * @brief Status information for the stepper motor
     */
    struct StepperStatus
    {
        bool enabled;            ///< Motor enabled state
        bool running;            ///< Motor running state
        int32_t currentPosition; ///< Current position
        int32_t targetPosition;  ///< Target position
        int32_t stepsRemaining;  ///< Steps remaining to target
    };

    class Stepper
    {
    public:
        // Constructors
        Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin);
        ~Stepper();

        // Basic control
        void enable();
        void disable();
        void stop();
        void emergencyStop();

        // Configuration
        void setOperationMode(OperationMode mode) { _operationMode = mode; }
        void setMicrosteps(uint32_t microsteps);

        // Status
        OperationMode getOperationMode() const { return _operationMode; }
        bool isEnabled() const { return _enabled; }
        StepperStatus getStatus() const;

        // Position control
        void setTargetPosition(int32_t position);
        void setRelativePosition(int32_t delta);
        int32_t getCurrentPosition() const { return _currentPosition; }
        int32_t getTargetPosition() const { return _targetPosition; }
        void incrementCurrentPosition(int32_t increment);

        // ISR handler
        void ISR(void);

    protected:
        friend class TimerControl;

        // Hardware initialization
        void initPins();

        // GPIO control
        void GPIO_SET_STEP();
        void GPIO_CLEAR_STEP();
        void GPIO_SET_DIRECTION();
        void GPIO_CLEAR_DIRECTION();

    private:
        // Hardware pins
        uint8_t _stepPin;
        uint8_t _dirPin;
        uint8_t _enablePin;

        // State variables
        volatile bool _enabled;
        volatile bool _running;
        volatile bool _currentDirection;
        volatile int32_t _currentPosition;
        volatile int32_t _targetPosition;
        OperationMode _operationMode;
        uint8_t state;
    };

} // namespace STM32Step
