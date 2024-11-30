#include <Arduino.h>
#include "Application/MotionControl.h"
#include "Config/serial_debug.h"
#include <STM32Step.h>
#include "Core/system_clock.h"

HardwareSerial SerialDebug(PA2 /* RX */, PA3 /* TX */);
// Pin definitions for motion control
const MotionControl::MotionPins motionPins = {
    .step_pin = PE9,  // Your TIM1 PWM pin
    .dir_pin = PE8,   // Direction pin
    .enable_pin = PE7 // Enable pin
};

// Create motion control instance
MotionControl motionControl(motionPins);

void setup()
{
    // 1. Initialize system clock first
    // Initialize system clock
    SystemClock::GetInstance().initialize(); // Remove the if check to avoid serial dependency

    // 2. Initialize motion control
    motionControl.begin(); // Remove the if check to avoid serial dependency

    // Configure for 1:1 sync
    MotionControl::Config config;
    config.thread_pitch = 1.0f;       // 1mm thread pitch
    config.leadscrew_pitch = 1.0f;    // 1mm leadscrew pitch
    config.steps_per_rev = 200;       // Standard stepper steps/rev
    config.microsteps = 16;           // Microstep setting
    config.reverse_direction = false; // Normal direction
    config.sync_frequency = 1000;     // 1kHz sync updates

    motionControl.setConfig(config);
    motionControl.setMode(MotionControl::Mode::THREADING);

    // 3. Initialize serial last (optional now)
    SerialDebug.begin(115200);
    delay(100); // Short delay to let serial stabilize
}

void loop()
{
    static bool motionStarted = false;
    static uint32_t lastStatusUpdate = 0;
    const uint32_t STATUS_INTERVAL = 1000; // Status update every 1 second

    // Start motion after initialization
    if (!motionStarted)
    {
        motionControl.startMotion();
        motionStarted = true;
    }

    // Only print status if serial is actually available
    if (SerialDebug && (millis() - lastStatusUpdate >= STATUS_INTERVAL))
    {
        MotionControl::Status status = motionControl.getStatus();
        if (status.error)
        {
            motionControl.stopMotion();
            while (1)
            {
                delay(1000); // Add a delay in the error loop
            }
        }
        lastStatusUpdate = millis();
    }
}
