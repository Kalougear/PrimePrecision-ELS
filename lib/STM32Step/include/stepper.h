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
        volatile bool _currentDirection;   ///< Current direction state (e.g. false=CW, true=CCW).
        volatile int32_t _currentPosition; ///< Current software position in steps.
        volatile int32_t _targetPosition;  ///< Target software position in steps.
        OperationMode _operationMode;      ///< Current operational mode.
        uint8_t state;                     ///< State for the ISR's internal step generation state machine.
    };

} // namespace STM32Step
