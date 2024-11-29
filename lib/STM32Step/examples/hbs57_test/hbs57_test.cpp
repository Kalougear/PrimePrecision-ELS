#include "hbs57_test.h"

namespace STM32Step
{

    bool HBS57Test::runVerification()
    {
        printf("Starting HBS57 Driver Verification\n");

        // Initialize hardware
        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        if (!TimerControl::begin())
        {
            printf("Failed to initialize timer system\n");
            return false;
        }

        // Configure for HBS57 specifications
        stepper.setMaxSpeed(MAX_STEP_FREQ); // 200kHz maximum from HBS57 manual
        stepper.setAcceleration(10000);     // Conservative acceleration
        stepper.setMicrosteps(3200);        // Common microstep setting for HBS57

        bool success = true;
        success &= testPulseTiming();
        success &= testDirectionChanges();
        success &= testEmergencyStop();
        success &= testAcceleration();
        success &= testMicrostepRange();

        printf("\nVerification %s\n", success ? "PASSED" : "FAILED");
        return success;
    }

    bool HBS57Test::testPulseTiming()
    {
        printf("\nTest 1: Signal Timing Verification\n");
        printf("Oscilloscope Check Points:\n");
        printf("1. Step Pulse Width: Should be >= 2.5µs\n");
        printf("2. Direction Setup: Should be >= 5µs before step\n");

        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        stepper.enable();
        HAL_Delay(ENABLE_SETUP_TIME / 1000 + 1);

        const uint32_t test_speeds[] = {
            1000,  // 1kHz - Easy to view
            50000, // 50kHz - Mid range
            200000 // 200kHz - Maximum rated
        };

        for (uint32_t speed : test_speeds)
        {
            printf("Testing at %lu Hz\n", speed);
            stepper.setSpeed(speed);
            stepper.setTargetRelative(1000);
            HAL_Delay(100);
            stepper.stop();
            HAL_Delay(100);
        }

        return true;
    }

    bool HBS57Test::testDirectionChanges()
    {
        printf("\nTest 2: Direction Change Verification\n");
        printf("Verify 5µs setup time between DIR and STEP\n");

        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        stepper.setSpeed(10000); // 10kHz for visible direction changes

        for (int i = 0; i < 5; i++)
        {
            stepper.setTargetRelative(1000);
            HAL_Delay(100);
            stepper.setTargetRelative(-1000);
            HAL_Delay(100);
        }

        return true;
    }

    bool HBS57Test::testEmergencyStop()
    {
        printf("\nTest 3: Emergency Stop Test\n");

        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        stepper.setSpeed(50000);
        stepper.setTargetRelative(10000);
        HAL_Delay(10);
        stepper.emergencyStop();

        return true;
    }

    bool HBS57Test::testAcceleration()
    {
        printf("\nTest 4: Acceleration Profile\n");

        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        stepper.setAcceleration(5000);
        stepper.setSpeed(100000);
        stepper.setTargetRelative(50000);
        HAL_Delay(1000);

        return true;
    }

    bool HBS57Test::testMicrostepRange()
    {
        printf("\nTest 5: Microstep Configuration Test\n");

        Stepper stepper(PIN_STEP, PIN_DIR, PIN_ENA);
        const uint32_t microstep_settings[] = {800, 1600, 3200, 6400, 12800, 25600, 51200};

        for (uint32_t ms : microstep_settings)
        {
            printf("Testing microstep setting: %lu\n", ms);
            stepper.setMicrosteps(ms);
            stepper.setSpeed(1000);
            stepper.setTargetRelative(ms);
            HAL_Delay(1000);
        }

        return true;
    }

    void HBS57Test::printTimingRequirements()
    {
        printf("\nHBS57 Timing Requirements:\n");
        printf("- Step Pulse Width: >= 2.5µs\n");
        printf("- Direction Setup Time: >= 5µs\n");
        printf("- Enable Setup Time: >= 5µs\n");
        printf("- Maximum Step Frequency: 200kHz\n");
        printf("- Microstep Settings: 800-51200 steps/rev\n");
    }

} // namespace STM32Step