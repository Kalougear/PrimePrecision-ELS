#pragma once

#include "../stepper.h"
#include "../timer_base.h"
#include "../acceleration.h"
#include "../config.h"

namespace STM32Step
{

    class HBS57Test
    {
    public:
        // Pin assignments for your hardware
        static constexpr uint8_t PIN_STEP = 8; // PE8
        static constexpr uint8_t PIN_DIR = 9;  // PE9
        static constexpr uint8_t PIN_ENA = 7;  // PE7

        static bool runVerification();
        static void printTimingRequirements();

    private:
        static bool testPulseTiming();
        static bool testDirectionChanges();
        static bool testEmergencyStop();
        static bool testAcceleration();
        static bool testMicrostepRange();
    };

} // namespace STM32Step