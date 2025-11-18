#pragma once
#include "config.h" // For OperationMode enum and PinConfig
#include "timer_base.h"
#include "Config/systemconfig.h" // For SystemConfig access if needed, though direct use is in .cpp

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
        int32_t stepsRemaining;  ///< The number of steps remaining to reach the target position.
    };

    /**
     * @class Stepper
     * @brief Controls a single stepper motor using software-generated step pulses via an ISR.
     *
     * This class manages the state of a stepper motor, including its position,
     * whether it's enabled or running, and the direction of movement.
     * Step pulse generation is handled by the ISR() method, which must be called
     * periodically by a hardware timer (managed by the TimerControl class).
     */
    class Stepper
    {
    public:
        /**
         * @brief Constructs a Stepper object.
         * @param stepPin The GPIO pin number (0-15) on GPIOE for the STEP signal.
         * @param dirPin The GPIO pin number (0-15) on GPIOE for the DIRECTION signal.
         * @param enablePin The GPIO pin number (0-15) on GPIOE for the ENABLE signal.
         */
        Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin);

        /**
         * @brief Destructor. Disables the stepper motor.
         */
        ~Stepper();

        // --- Basic Control ---
        /** @brief Enables the stepper motor driver. */
        void enable();
        /** @brief Disables the stepper motor driver. Stops motion first. */
        void disable();
        /** @brief Stops motor movement immediately and pauses the step generation timer. */
        void stop();
        /** @brief Performs an emergency stop: stops motion, disables driver, signals TimerControl. */
        void emergencyStop();

        // --- Configuration ---
        /**
         * @brief Sets the operation mode of the stepper.
         * @param mode The OperationMode (e.g., IDLE, THREADING, TURNING).
         * @note This is primarily for informational purposes or for higher-level logic to query.
         */
        void setOperationMode(OperationMode mode) { _operationMode = mode; }

        /**
         * @brief Sets the microstepping value in the global SystemConfig.
         * @param microsteps The microstepping value (e.g., 8, 16).
         * @note This updates a software configuration value. The physical driver must be set accordingly.
         */
        void setMicrosteps(uint32_t microsteps);

        // --- Status ---
        /** @brief Gets the current operation mode. @return The current OperationMode. */
        OperationMode getOperationMode() const { return _operationMode; }
        /** @brief Checks if the motor driver is enabled. @return True if enabled, false otherwise. */
        bool isEnabled() const { return _enabled; }
        /** @brief Gets detailed status of the stepper motor. @return StepperStatus struct. */
        StepperStatus getStatus() const;
        /** @brief Checks if the motor is currently running. @return True if running. */
        bool isRunning() const { return _running; }
        /** @brief Gets the current speed in steps per second. @return Speed in Hz. */
        uint32_t getCurrentSpeed() const { return static_cast<uint32_t>(_currentSpeedHz); }

        // --- Speed and Acceleration Control (New for Continuous Mode) ---
        /**
         * @brief Sets the target speed for continuous run mode.
         * @param frequency_hz Target speed in steps per second (Hz).
         */
        void setSpeedHz(float frequency_hz);

        /**
         * @brief Sets the acceleration for continuous run mode.
         * @param accel_steps_per_s2 Acceleration in steps per second squared.
         */
        void setAcceleration(float accel_steps_per_s2);

        /**
         * @brief Starts continuous motion at the configured speed and acceleration.
         * @param direction True for one direction (e.g., forward/positive), false for the other.
         *                  Actual physical direction depends on wiring and DIR pin logic.
         */
        void runContinuous(bool direction);

        // --- Position Control ---
        /**
         * @brief Sets the absolute target position for the motor.
         * Initiates movement if not already running and target is different from current.
         * @param position Absolute target position in steps.
         */
        void setTargetPosition(int32_t position);

        /**
         * @brief Sets a target position relative to the current target.
         * Useful for commanding incremental moves.
         * @param delta Number of steps to move relative to the current target.
         */
        void setRelativePosition(int32_t delta);

        /** @brief Gets the current software-tracked position. @return Current position in steps. */
        int32_t getCurrentPosition() const { return _currentPosition; }
        /** @brief Gets the current target position. @return Target position in steps. */
        int32_t getTargetPosition() const { return _targetPosition; }
        /** @brief Sets the current software position to a specific value. */
        void setPosition(int32_t position) { _currentPosition = position; }
        /** @brief Resets the current software position to zero. */
        void resetPosition()
        {
            _currentPosition = 0;
            _targetPosition = 0;
        }
        /** @brief Sets the target speed (alias for setSpeedHz for compatibility). */
        void setSpeed(uint32_t steps_per_second) { setSpeedHz(static_cast<float>(steps_per_second)); }

        /**
         * @brief Manually increments/decrements both current and target software positions.
         * Useful for re-synchronizing after events like encoder overflow, maintaining relative move integrity.
         * @param increment Amount to add to current and target positions.
         */
        void incrementCurrentPosition(int32_t increment);

        // --- ISR Handler ---
        /**
         * @brief Interrupt Service Routine for step generation.
         * Called by TimerControl at high frequency to produce step pulses.
         * This should be public for TimerControl to call but not typically by user code.
         */
        void ISR(void);

    protected:
        friend class TimerControl; // Allow TimerControl to access protected/private members if needed (e.g. _running)

        /**
         * @brief Initializes GPIO pins for step, direction, and enable signals.
         * Called by the constructor.
         */
        void initPins();

        // --- Low-level GPIO control (protected for potential future direct use or testing) ---
        /** @brief Sets the STEP pin HIGH. */
        void GPIO_SET_STEP();
        /** @brief Sets the STEP pin LOW. */
        void GPIO_CLEAR_STEP();
        /** @brief Sets the DIRECTION pin HIGH (conventionally one direction). */
        void GPIO_SET_DIRECTION();
        /** @brief Sets the DIRECTION pin LOW (conventionally the other direction). */
        void GPIO_CLEAR_DIRECTION();

    private:
        // Hardware pins (GPIOE assumed)
        uint8_t _stepPin;   ///< Pin number for STEP signal (0-15 for Port E)
        uint8_t _dirPin;    ///< Pin number for DIR signal (0-15 for Port E)
        uint8_t _enablePin; ///< Pin number for ENABLE signal (0-15 for Port E)

        // State variables
        volatile bool _enabled;            ///< True if driver is enabled.
        volatile bool _running;            ///< True if motor is actively moving to a target.
        volatile bool _currentDirection;   ///< Current direction state (e.g. false=CW, true=CCW). Used by both modes.
        volatile int32_t _currentPosition; ///< Current software position in steps (primarily for ELS mode).
        volatile int32_t _targetPosition;  ///< Target software position in steps (primarily for ELS mode).
        OperationMode _operationMode;      ///< Current operational mode.
        uint8_t state;                     ///< State for the ISR's internal step generation state machine (for ELS position mode).

        // New members for continuous speed mode with acceleration
        volatile bool _isContinuousMode;       ///< True if operating in continuous speed mode.
        volatile float _targetSpeedHz;         ///< Target speed in Hz for continuous mode.
        volatile float _currentSpeedHz;        ///< Current instantaneous speed in Hz during acceleration/deceleration.
        float _accelerationStepsPerS2;         ///< Acceleration rate in steps/s^2.
        volatile uint32_t _nextStepTimeMicros; ///< Timestamp for the next step in continuous mode ISR.
                                               ///< Stores the micros() value when the next step should occur.
        volatile float _isrAccumulatedSteps;   ///< For accumulating fractional steps at lower speeds/frequencies in ISR.
                                               ///< Or, could be used to track progress during acceleration.
                                               ///< Alternative: _timePerStepMicros = 1.0f / _currentSpeedHz * 1e6;
        volatile uint32_t _lastStepTimeMicros; ///< Timestamp of the last step taken, for acceleration calculations.
    };

} // namespace STM32Step
