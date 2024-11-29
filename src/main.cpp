#include <Arduino.h>
#include "STM32Step.h"
#include "Config/serial_debug.h"
#include "Core/system_clock.h"
#include "Core/encoder_timer.h"

HardwareSerial SerialDebug(PA3, PA2);

// Pin definitions
static const uint8_t k_step_pin = STM32Step::PinConfig::StepPin::PIN;
static const uint8_t k_dir_pin = STM32Step::PinConfig::DirPin::PIN;
static const uint8_t k_enable_pin = STM32Step::PinConfig::EnablePin::PIN;

STM32Step::Stepper *stepper = nullptr;
EncoderTimer *encoder = nullptr;

// Default settings
const uint32_t DEFAULT_SPEED = 1000;          // Hz
const uint32_t MAX_SPEED = 20000;             // Maximum speed in Hz
const uint32_t MICROSTEPS = 1600;             // Physical microsteps per revolution
const uint32_t ENCODER_PPR = 1024;            // Encoder pulses per revolution
const uint32_t ENCODER_CPR = ENCODER_PPR * 4; // Counts per revolution (quadrature)

// Sync state
bool syncActive = false;
int32_t lastEncoderCount = 0;
int32_t lastStepperPos = 0;

// Convert encoder position to stepper position
int32_t encoderToStepperPos(int32_t encoderCount)
{
    // Convert to float for accurate calculation
    float ratio = (float)MICROSTEPS / ENCODER_CPR;
    float pos = encoderCount * ratio;
    return (int32_t)pos;
}

void printStatus()
{
    auto stepper_status = stepper->getStatus();
    auto encoder_pos = encoder->getPosition();

    SerialDebug.println("\n=== System Status ===");

    SerialDebug.print("Encoder Position: ");
    SerialDebug.print(encoder_pos.count);
    SerialDebug.print(" counts (");
    SerialDebug.print((float)encoder_pos.count / ENCODER_CPR, 3);
    SerialDebug.print(" revolutions) RPM: ");
    SerialDebug.println(encoder_pos.rpm);

    SerialDebug.print("Stepper Position: ");
    SerialDebug.print(stepper_status.currentPosition);
    SerialDebug.print(" steps (");
    SerialDebug.print((float)stepper_status.currentPosition / MICROSTEPS, 3);
    SerialDebug.println(" revolutions)");

    SerialDebug.print("Sync Active: ");
    SerialDebug.println(syncActive ? "Yes" : "No");

    SerialDebug.println("=====================");
}

void setup()
{
    SerialDebug.begin(115200);
    delay(1000);
    SerialDebug.println("\n=== STM32Step Encoder Sync Test ===");

    // Initialize system clock
    if (!SystemClock::GetInstance().initialize())
    {
        SerialDebug.println("ERROR: Clock initialization failed!");
        while (1)
        {
            delay(1000);
        }
    }

    // Initialize encoder
    encoder = new EncoderTimer();
    if (!encoder->begin())
    {
        SerialDebug.println("ERROR: Encoder initialization failed!");
        while (1)
        {
            delay(1000);
        }
    }

    // Configure encoder sync parameters
    EncoderTimer::SyncConfig syncConfig;
    syncConfig.thread_pitch = 1.0f;           // 1mm thread pitch
    syncConfig.leadscrew_pitch = 1.0f;        // 1mm leadscrew pitch
    syncConfig.stepper_steps = 200;           // 200 steps per revolution
    syncConfig.microsteps = MICROSTEPS / 200; // Microsteps per full step
    syncConfig.reverse_sync = false;          // Direction matching
    encoder->setSyncConfig(syncConfig);

    // Configure runtime parameters
    STM32Step::RuntimeConfig::current_pulse_width = 10;        // 5μs pulse
    STM32Step::RuntimeConfig::current_microsteps = MICROSTEPS; // Match physical setup
    STM32Step::RuntimeConfig::current_max_speed = MAX_SPEED;   // Max speed

    // Initialize timer control
    STM32Step::TimerControl::init();

    // Create and configure stepper
    stepper = new STM32Step::Stepper(k_step_pin, k_dir_pin, k_enable_pin);
    if (!stepper)
    {
        SerialDebug.println("ERROR: Stepper initialization failed!");
        while (1)
        {
            delay(1000);
        }
    }

    // Configure stepper
    stepper->setMicrosteps(MICROSTEPS);
    stepper->setMaxSpeed(MAX_SPEED);
    stepper->setSpeed(DEFAULT_SPEED);
    stepper->enable();

    SerialDebug.println("\nAvailable Commands:");
    SerialDebug.println("s - Start/Stop sync");
    SerialDebug.println("r - Reset positions");
    SerialDebug.println("? - Print status");
    SerialDebug.println("h - Show this help");

    // Print initial status
    printStatus();
}

void processCommand(String cmd)
{
    if (cmd == "s")
    {
        if (syncActive)
        {
            syncActive = false;
            stepper->stop();
            lastStepperPos = stepper->getCurrentPosition();
            lastEncoderCount = encoder->getPosition().count;
            SerialDebug.println("Sync disabled");
        }
        else
        {
            syncActive = true;
            lastStepperPos = stepper->getCurrentPosition();
            lastEncoderCount = encoder->getPosition().count;
            SerialDebug.println("Sync enabled");
        }
        printStatus();
    }
    else if (cmd == "r")
    {
        encoder->reset();
        stepper->stop();
        lastStepperPos = 0;
        lastEncoderCount = 0;
        SerialDebug.println("Positions reset");
        printStatus();
    }
    else if (cmd == "?")
    {
        printStatus();
    }
    else if (cmd == "h")
    {
        SerialDebug.println("\nAvailable Commands:");
        SerialDebug.println("s - Start/Stop sync");
        SerialDebug.println("r - Reset positions");
        SerialDebug.println("? - Print status");
        SerialDebug.println("h - Show this help");
    }
    else
    {
        SerialDebug.println("Unknown command. Use 'h' for help.");
    }
}

void loop()
{
    static uint32_t lastUpdate = 0;
    const uint32_t UPDATE_INTERVAL = 1; // 1ms update interval for immediate response

    // Process serial commands
    static String cmdBuffer = "";
    while (SerialDebug.available())
    {
        char c = SerialDebug.read();
        if (c == '\n' || c == '\r')
        {
            if (cmdBuffer.length() > 0)
            {
                processCommand(cmdBuffer);
                cmdBuffer = "";
            }
        }
        else
        {
            cmdBuffer += c;
        }
    }

    // Update sync position
    if (syncActive && millis() - lastUpdate >= UPDATE_INTERVAL)
    {
        auto encoder_pos = encoder->getPosition();
        if (encoder_pos.valid)
        {
            // Calculate encoder movement
            int32_t encoderDelta = encoder_pos.count - lastEncoderCount;

            if (encoderDelta != 0)
            {
                // Calculate required steps
                int32_t targetPos = encoderToStepperPos(encoder_pos.count);
                int32_t stepDelta = targetPos - lastStepperPos;

                if (stepDelta != 0)
                {
                    // Set speed directly from encoder RPM
                    int16_t rpm = abs(encoder_pos.rpm);
                    uint32_t speed = (rpm * MICROSTEPS) / 60; // Convert RPM to Hz
                    if (speed < 100)
                        speed = 100; // Minimum speed to prevent stalling
                    stepper->setSpeed(speed);

                    // Move stepper without waiting
                    stepper->moveSteps(stepDelta, false);
                    lastStepperPos = targetPos;
                }

                lastEncoderCount = encoder_pos.count;
            }
        }
        lastUpdate = millis();
    }
}
