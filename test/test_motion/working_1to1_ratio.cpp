#include <Arduino.h>
#include "STM32Step.h"
#include "Config/serial_debug.h"
#include "Hardware/SystemClock.h"
#include "Hardware/EncoderTimer.h"
#include "Config/SystemConfig.h"
#include <HardwareTimer.h>

HardwareSerial SerialDebug(PA3, PA2);

using namespace STM32Step;
using namespace SystemConfig;

// Pin definitions from STM32Step PinConfig
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
const uint32_t ENCODER_MAX_COUNT = 0xFFFFFFFF; // 32-bit counter

float calculateMaxRPM(uint32_t syncFreq)
{
    // Calculate max steps per second at current sync frequency
    float maxStepsPerSecond = syncFreq;

    // Calculate revolutions per second
    // Steps needed for one rev = STEPS_PER_REV * microsteps * 2 (mechanical ratio)
    float stepsPerRev = Limits::Stepper::STEPS_PER_REV * RuntimeConfig::Stepper::microsteps * 2;
    float maxRevsPerSecond = maxStepsPerSecond / stepsPerRev;

    // Convert to RPM
    return maxRevsPerSecond * 60;
}

float calculateStepsPerEncoderCount()
{
    return ((float)Limits::Stepper::STEPS_PER_REV * RuntimeConfig::Stepper::microsteps) /
           (RuntimeConfig::Encoder::ppr * Limits::Encoder::QUADRATURE_MULT);
}

int32_t encoderToStepperPosition(int32_t encoderPos)
{
    return (int32_t)(encoderPos * calculateStepsPerEncoderCount());
}

void updateSyncFrequency(uint32_t newFreq)
{
    RuntimeConfig::Motion::sync_frequency = newFreq;

    if (timer6)
    {
        timer6->setPrescaleFactor(1);
        timer6->setOverflow(newFreq, HERTZ_FORMAT);
        SerialDebug.print("Sync frequency updated to: ");
        SerialDebug.print(newFreq);
        SerialDebug.println(" Hz");
    }
}

void timerCallback()
{
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
    SerialDebug.println("Commands:");
    SerialDebug.println("1 - Set 1kHz sync frequency");
    SerialDebug.println("2 - Set 2kHz sync frequency");
    SerialDebug.println("3 - Set 5kHz sync frequency");
    SerialDebug.println("4 - Set 10kHz sync frequency");
    SerialDebug.println("5 - Set 20kHz sync frequency");
    SerialDebug.println("6 - Set 30kHz sync frequency");
    SerialDebug.println("7 - Set 50kHz sync frequency");
    SerialDebug.println();

    // Set initial configuration values
    RuntimeConfig::Encoder::ppr = Limits::Encoder::DEFAULT_PPR;
    RuntimeConfig::Stepper::microsteps = Limits::Stepper::DEFAULT_MICROSTEPS;
    RuntimeConfig::Motion::sync_frequency = Limits::Motion::DEFAULT_SYNC_FREQ;

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

    // Configure stepper using system config values
    stepper->setMicrosteps(RuntimeConfig::Stepper::microsteps);
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

    // Configure Timer6 using system config frequency
    updateSyncFrequency(RuntimeConfig::Motion::sync_frequency);
    timer6->attachInterrupt(timerCallback);
    timer6->resume();

    // Initialize tracking variables
    previousEncoderPosition = encoderTimer.getPosition().count;

    // Print configuration summary
    SerialDebug.println("\nSystem Configuration:");
    SerialDebug.print("Encoder PPR: ");
    SerialDebug.println(RuntimeConfig::Encoder::ppr);
    SerialDebug.print("Stepper Steps/Rev: ");
    SerialDebug.println(Limits::Stepper::STEPS_PER_REV);
    SerialDebug.print("Microsteps: ");
    SerialDebug.println(RuntimeConfig::Stepper::microsteps);
    SerialDebug.print("Steps per encoder count: ");
    SerialDebug.println(calculateStepsPerEncoderCount(), 4);
    SerialDebug.print("Initial sync frequency: ");
    SerialDebug.print(RuntimeConfig::Motion::sync_frequency);
    SerialDebug.println(" Hz");
    SerialDebug.println("\nSystem ready!\n");
}

void loop()
{
    static uint32_t lastPrint = 0;
    const uint32_t PRINT_INTERVAL = 1000;

    // Check for serial input
    if (SerialDebug.available())
    {
        char cmd = SerialDebug.read();
        switch (cmd)
        {
        case '1':
            updateSyncFrequency(1000); // 1kHz
            break;
        case '2':
            updateSyncFrequency(2000); // 2kHz
            break;
        case '3':
            updateSyncFrequency(5000); // 5kHz
            break;
        case '4':
            updateSyncFrequency(10000); // 10kHz
            break;
        case '5':
            updateSyncFrequency(20000); // 10kHz
            break;
        case '6':
            updateSyncFrequency(30000); // 10kHz
            break;
        case '7':
            updateSyncFrequency(50000); // 10kHz
            break;
        }
    }

    // Status update
    if (millis() - lastPrint >= PRINT_INTERVAL)
    {
        EncoderTimer::Position pos = encoderTimer.getPosition();
        auto status = stepper->getStatus();

        // Calculate theoretical max RPM for current sync frequency
        float maxRPM = calculateMaxRPM(RuntimeConfig::Motion::sync_frequency);

        SerialDebug.print("Status - Encoder: ");
        SerialDebug.print(pos.count);
        SerialDebug.print(" Stepper: ");
        SerialDebug.print(status.currentPosition);
        SerialDebug.print(" Running: ");
        SerialDebug.print(status.running ? "Yes" : "No");
        SerialDebug.print(" Encoder RPM: ");
        SerialDebug.print(pos.rpm);
        SerialDebug.print(" Max RPM at current sync: ");
        SerialDebug.print(maxRPM, 1); // Print with 1 decimal place
        SerialDebug.print(" Sync Freq: ");
        SerialDebug.print(RuntimeConfig::Motion::sync_frequency);
        SerialDebug.println(" Hz");

        syncCount = 0;
        lastPrint = millis();
    }

    delay(1);
}