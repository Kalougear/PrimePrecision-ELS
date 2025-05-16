#pragma once

#include "config.h"
#include <HardwareTimer.h>
#include "Hardware/SystemClock.h" // Required for SystemClock::GetInstance().GetPClk2Freq() in .cpp

namespace STM32Step
{
    // Forward declaration
    class Stepper;

    /**
     * @class TimerBase
     * @brief (Currently Unused) Base class intended for timer operations, inheriting from HardwareTimer.
     * Could be used for a more object-oriented timer approach if TimerControl were not static.
     */
    class TimerBase : public HardwareTimer
    {
    public:
        TimerBase() : HardwareTimer(TIM1) {} // Example: Default to TIM1
        virtual ~TimerBase() = default;

        virtual void start() { resume(); }
        virtual void stop() { pause(); }
        /**
         * @brief Sets the frequency of the timer.
         * @param freq The desired frequency in Hz.
         */
        virtual void setFrequency(uint32_t freq); // Implementation would be in .cpp

    protected:
        friend class Stepper;
        /**
         * @brief Configures the timer for stepper motor control.
         * Placeholder for specific timer setup logic.
         */
        void configureForStepper(); // Implementation would be in .cpp
    };

    /**
     * @class TimerControl
     * @brief Static class to manage the hardware timer (TIM1) for generating stepper ISR calls.
     *
     * This class handles the initialization, starting, and stopping of the timer
     * that provides the high-frequency interrupts necessary for the Stepper::ISR()
     * to generate step pulses.
     */
    class TimerControl
    {
    public:
        /**
         * @enum MotorState
         * @brief Defines the operational state of the timer/motor control system.
         */
        enum class MotorState
        {
            IDLE,    ///< Motor/Timer is idle.
            RUNNING, ///< Motor/Timer is actively running.
            ERROR    ///< An error has occurred in timer setup.
        };

        /**
         * @brief Initializes the hardware timer (TIM1) for stepper ISR.
         * Configures TIM1 to generate interrupts at a fixed frequency.
         * This must be called once before any stepper operations.
         */
        static void init();

        /**
         * @brief Starts or resumes the hardware timer for a given stepper.
         * @param stepper Pointer to the Stepper object whose ISR will be called.
         */
        static void start(Stepper *stepper);

        /**
         * @brief Stops or pauses the hardware timer.
         * Also updates the associated stepper's running state.
         */
        static void stop();

        /**
         * @brief Checks if the current stepper has reached its target position.
         * @return True if target is reached or no stepper is active, false otherwise.
         * @deprecated This might not be the best place for this logic; Stepper::getStatus() is preferred.
         */
        static bool isTargetPositionReached();

        /**
         * @brief Signals an emergency stop condition.
         * Currently calls stop(). Could be expanded for more specific e-stop actions.
         */
        static void emergencyStopRequest();

        /**
         * @brief (Currently Unused by ISR logic) Checks if the current stepper has reached its target.
         * @return True if target is reached or no stepper is active.
         * @deprecated Prefer Stepper::getStatus().
         */
        static bool checkTargetPosition();

        // --- Public Static Members ---
        // These are directly accessed by Stepper or ISR callbacks.

        /** @brief Pointer to the HardwareTimer instance (e.g., TIM1). Managed internally. */
        static HardwareTimer *htim;

        /** @brief Flag indicating if the TimerControl system considers itself actively running a stepper. */
        static volatile bool running; // True when timer is running to service a stepper

        /** @brief Pointer to the current Stepper instance being serviced by the timer ISR. */
        static Stepper *currentStepper;

        /** @brief Gets the current operational state of the TimerControl. @return MotorState */
        static MotorState getCurrentState() { return currentState; }

    private:
        // Private helper methods (if any specific to PWM, not currently used by software stepping)
        // static void configurePWM(); // Example if PWM mode was being configured here

        // State tracking
        static volatile MotorState currentState; ///< Current state of the TimerControl.
        static volatile bool emergencyStop;      ///< Flag indicating an emergency stop has been requested.
        static volatile bool positionReached;    ///< Flag indicating the target position was reached (used by isTargetPositionReached).
    };

} // namespace STM32Step
