#pragma once
#include "config.h" // For OperationMode enum and PinConfig
#include "timer_base.h"
#include "Config/systemconfig.h" // For SystemConfig access if needed, though direct use is in .cpp

class SyncTimer;

namespace STM32Step
{
    /**
     * @brief Holds status information for the stepper motor.
     */
    struct StepperStatus
    {
        bool enabled;            ///< True if the motor driver is enabled.
        bool running;            ///< True if the motor is currently executing a move.
        int32_t currentPosition; ///< The current software-tracked position of the motor in steps.
        int32_t targetPosition;  ///< The target position the motor is moving towards, in steps.
    };

    /**
     * @class Stepper
     * @brief Controls a single stepper motor using hardware-generated step pulses via TIM1.
     *
     * This class manages the state of a stepper motor, including its position,
     * speed, and direction. Step pulse generation is handled entirely by the hardware timer
     * in PWM mode, ensuring jitter-free operation with minimal CPU load.
     */
    class Stepper
    {
        friend class ::SyncTimer;

    public:
        /**
         * @brief Constructs a Stepper object.
         * @param stepPin The GPIO pin number for the STEP signal (must be a TIM1 channel).
         * @param dirPin The GPIO pin number for the DIRECTION signal.
         * @param enablePin The GPIO pin number for the ENABLE signal.
         */
        Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin);

        /**
         * @brief Destructor. Disables the stepper motor.
         */
        ~Stepper();

        // --- Basic Control ---
        void enable();
        void disable();
        void stop();
        void emergencyStop();

        // --- Configuration ---
        void setOperationMode(OperationMode mode) { _operationMode = mode; }
        void setMicrosteps(uint32_t microsteps);

        // --- Status ---
        OperationMode getOperationMode() const { return _operationMode; }
        bool isEnabled() const { return _enabled; }
        StepperStatus getStatus() const;
        bool isRunning() const { return _running; }
        uint32_t getCurrentSpeed() const { return static_cast<uint32_t>(_currentSpeedHz); }

        // --- Speed and Acceleration Control ---
        void setSpeedHz(float frequency_hz);
        void setAcceleration(float accel_steps_per_s2);
        void runContinuous(bool direction);

        // --- Position Control ---
        void setDesiredPosition(int32_t position);
        void setTargetPosition(int32_t position);
        void setRelativePosition(int32_t delta);

        /**
         * @brief Moves the stepper a precise number of steps at a given frequency.
         * Uses the hardware repetition counter for accurate, non-blocking moves.
         * @param steps The number of steps to move. Negative values move in the opposite direction.
         * @param frequency_hz The speed of the movement in steps per second.
         */
        void moveExact(int32_t steps, uint32_t frequency_hz);

        int32_t getCurrentPosition() const { return _currentPosition; }
        int32_t getTargetPosition() const { return _targetPosition; }
        void setPosition(int32_t position) { _currentPosition = position; }
        void resetPosition()
        {
            _currentPosition = 0;
            _targetPosition = 0;
        }
        void setSpeed(uint32_t steps_per_second) { setSpeedHz(static_cast<float>(steps_per_second)); }
        void incrementCurrentPosition(int32_t increment);
        void adjustPosition(int32_t adjustment);

        /**
         * @brief Reads the hardware pulse counter and updates the software position.
         * This method ensures _currentPosition stays in sync with the hardware timer.
         * Should be called periodically (e.g. in getStatus() or ISR).
         */
        void updatePositionFromHardware();

        /**
         * @brief Callback function executed by the Timer ISR upon completion of a move.
         * This should not be called by user code.
         */
        void onMoveComplete();

    protected:
        friend class TimerControl; // Allow TimerControl to access _running

        /**
         * @brief Main ISR for the stepper motion control.
         * This is called from the high-frequency SyncTimer interrupt.
         */
        void ISR();

        void initPins();

        // --- Low-level GPIO control ---
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
        volatile bool _currentDirection; // false=CW, true=CCW
        volatile int32_t _currentPosition;
        volatile int32_t _targetPosition;
        volatile int32_t _desiredPosition;
        volatile int32_t _steps_pending_for_isr;   // Stores the number of steps for the current hardware move
        volatile uint32_t _lastHardwarePulseCount; // Tracks the last read value from the hardware counter
        OperationMode _operationMode;

        // Speed/Accel variables
        volatile float _targetSpeedHz;
        volatile float _currentSpeedHz;
        float _accelerationStepsPerS2;
    };

} // namespace STM32Step
