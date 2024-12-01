#include <Arduino.h>
#include "STM32Step.h"
#include "Config/serial_debug.h"
#include "Core/system_clock.h"
#include "Core/encoder_timer.h"
#include <HardwareTimer.h>

using namespace STM32Step;

// Serial for debugging
HardwareSerial SerialDebug(PA3, PA2);

// Pin definitions
static const uint8_t k_step_pin = PinConfig::StepPin::PIN;
static const uint8_t k_dir_pin = PinConfig::DirPin::PIN;
static const uint8_t k_enable_pin = PinConfig::EnablePin::PIN;

// Components
Stepper *stepper = nullptr;
EncoderTimer encoderTimer;
HardwareTimer *timer6;

// Tracking variables
volatile uint32_t syncCount = 0;
volatile int32_t previousEncoderPosition = 0;
const uint32_t ENCODER_MAX_COUNT = 0x00ffffff; // 24-bit counter

// Stepper and encoder parameters
const uint16_t ENCODER_PPR = 1024;  // Encoder pulses per revolution
const uint16_t STEPPER_SPR = 200;   // Stepper steps per revolution
const uint16_t MICROSTEPS = 8;      // Match DIP switch setting of 1600/200 = 8
const uint16_t QUADRATURE_MULT = 4; // Encoder quadrature multiplier

// Calculate steps per encoder count for 1:1 ratio
// We want (STEPPER_SPR * MICROSTEPS) steps to equal (ENCODER_PPR * QUADRATURE_MULT) encoder counts
// So each encoder count should result in (STEPPER_SPR * MICROSTEPS) / (ENCODER_PPR * QUADRATURE_MULT) steps
const float STEPS_PER_ENCODER_COUNT = ((float)STEPPER_SPR * MICROSTEPS) / (ENCODER_PPR * QUADRATURE_MULT);

int32_t encoderToStepperPosition(int32_t encoderPos)
{
    return (int32_t)(encoderPos * STEPS_PER_ENCODER_COUNT);
}

void timerCallback()
{
    // Read current encoder position
    EncoderTimer::Position encoderPos = encoderTimer.getPosition();

    if (encoderPos.valid)
    {
        int32_t currentPosition = encoderPos.count;

        // Update stepper target position
        stepper->setTargetPosition(encoderToStepperPosition(currentPosition));

        // Handle encoder overflow/underflow
        if (currentPosition < previousEncoderPosition &&
            (previousEncoderPosition - currentPosition) > ENCODER_MAX_COUNT / 2)
        {
            int32_t stepperAdjustment = encoderToStepperPosition(ENCODER_MAX_COUNT);
            stepper->incrementCurrentPosition(stepperAdjustment);
        }

        if (currentPosition > previousEncoderPosition &&
            (currentPosition - previousEncoderPosition) > ENCODER_MAX_COUNT / 2)
        {
            int32_t stepperAdjustment = encoderToStepperPosition(-ENCODER_MAX_COUNT);
            stepper->incrementCurrentPosition(stepperAdjustment);
        }

        previousEncoderPosition = currentPosition;
        syncCount++;
    }
}

void setup()
{
    SerialDebug.begin(115200);
    delay(1000);
    SerialDebug.println("\n=== Clough42 ELS Implementation Test ===");

    // Initialize system clock
    if (!SystemClock::GetInstance().initialize())
    {
        SerialDebug.println("ERROR: Clock initialization failed!");
        while (1)
        {
            delay(1000);
        }
    }

    // Configure runtime parameters
    RuntimeConfig::current_pulse_width = STEPPER_CYCLE_US; // Match Clough42 timing
    RuntimeConfig::current_microsteps = MICROSTEPS;
    RuntimeConfig::current_max_speed = 50000; // High max speed to allow fast catch-up

    // Initialize encoder
    SerialDebug.println("Initializing encoder...");
    if (!encoderTimer.begin())
    {
        SerialDebug.println("ERROR: Encoder initialization failed!");
        while (1)
        {
            delay(1000);
        }
    }
    SerialDebug.println("Encoder initialized");

    // Initialize timer control
    TimerControl::init();
    SerialDebug.println("Timer control initialized");

    // Create and configure stepper
    stepper = new Stepper(k_step_pin, k_dir_pin, k_enable_pin);
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
    stepper->enable();
    SerialDebug.println("Stepper initialized");

    // Create and configure Timer6 for position updates
    SerialDebug.println("Creating Timer6...");
    timer6 = new HardwareTimer(TIM6);
    if (!timer6)
    {
        SerialDebug.println("ERROR: Timer6 creation failed!");
        while (1)
        {
            delay(1000);
        }
    }

    // Configure Timer6 for 1kHz position updates
    timer6->setPrescaleFactor(1);
    timer6->setOverflow(1000, HERTZ_FORMAT);
    timer6->attachInterrupt(timerCallback);
    timer6->resume();
    SerialDebug.println("Timer6 initialized at 1kHz");

    // Initialize tracking variables
    previousEncoderPosition = encoderTimer.getPosition().count;

    SerialDebug.println("System initialization complete");
    SerialDebug.print("Using ratio: ");
    SerialDebug.print(STEPS_PER_ENCODER_COUNT, 4);
    SerialDebug.println(" stepper steps per encoder count");
    SerialDebug.print("Stepper microsteps: ");
    SerialDebug.println(MICROSTEPS);
}

void loop()
{
    static uint32_t lastPrint = 0;
    const uint32_t PRINT_INTERVAL = 1000;

    if (millis() - lastPrint >= PRINT_INTERVAL)
    {
        EncoderTimer::Position pos = encoderTimer.getPosition();
        auto status = stepper->getStatus();

        SerialDebug.print("Status - Encoder: ");
        SerialDebug.print(pos.count);
        SerialDebug.print(" Stepper: ");
        SerialDebug.print(status.currentPosition);
        SerialDebug.print(" Running: ");
        SerialDebug.print(status.running ? "Yes" : "No");
        SerialDebug.print(" RPM: ");
        SerialDebug.println(pos.rpm);

        // Reset counters
        syncCount = 0;
        lastPrint = millis();
    }

    delay(1);
}
