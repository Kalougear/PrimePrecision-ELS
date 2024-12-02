#pragma once

#include "config.h"
#include <HardwareTimer.h>
#include "Core/system_clock.h"

namespace STM32Step
{
    class Stepper;

    /**
     * @brief Base timer class for stepper control
     */
    class TimerBase : public HardwareTimer
    {
    public:
        TimerBase() : HardwareTimer(TIM1) {}
        virtual ~TimerBase() = default;

        virtual void start() { resume(); }
        virtual void stop() { pause(); }
        virtual void setFrequency(uint32_t freq);

    protected:
        friend class Stepper;
        void configureForStepper();
    };

    /**
     * @brief Timer control interface
     */
    class TimerControl
    {
    public:
        // Motor state enumeration
        enum class MotorState
        {
            IDLE,
            RUNNING,
            ERROR
        };

        // Initialization and cleanup
        static void init();

        // Control functions
        static void start(Stepper *stepper);
        static void stop();
        static bool isTargetPositionReached();
        static void emergencyStopRequest();

        // Timer management
        static bool checkTargetPosition();

        // Public members for callbacks and status
        static HardwareTimer *htim;
        static volatile bool running;
        static Stepper *currentStepper;

        // Get current state
        static MotorState getCurrentState() { return currentState; }

    private:
        // Private helper methods
        static void configurePWM();

        // State tracking
        static volatile MotorState currentState;
        static volatile bool emergencyStop;
        static volatile bool positionReached;
    };

} // namespace STM32Step
