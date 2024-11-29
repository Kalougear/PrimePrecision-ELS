#pragma once

#include "config.h"
#include <HardwareTimer.h>
#include "Core/system_clock.h"

namespace STM32Step
{
    class Stepper;

    class TimerControl
    {
    public:
        // Motor state enumeration
        enum class MotorState
        {
            IDLE,
            RUNNING,
            STOPPING,
            ERROR
        };

        // Initialization and cleanup
        static void init();
        static void cleanup();

        // Control functions
        static void start(Stepper *stepper);
        static void stop();
        static bool isTargetPositionReached();
        static int32_t getRemainingSteps();
        static void emergencyStopRequest();

        // Timer management
        static void updateTimerFrequency(uint32_t freq);
        static void updatePosition();
        static bool checkTargetPosition();
        static void printTimerStatus();

        // Public members for callbacks and status
        static HardwareTimer *htim;
        static volatile bool running;
        static Stepper *currentStepper;
        static uint32_t getLastUpdateTime() { return lastUpdateTime; }

        // Get current state
        static MotorState getCurrentState() { return currentState; }

    private:
        // Private helper methods
        static void configurePWM(uint32_t freq);
        static uint32_t constrainFrequency(uint32_t freq);

        // State tracking
        static volatile MotorState currentState;
        static volatile bool emergencyStop;
        static volatile bool positionReached;
        static volatile int32_t stepsRemaining;
        static volatile uint32_t lastUpdateTime;
    };

} // namespace STM32Step
