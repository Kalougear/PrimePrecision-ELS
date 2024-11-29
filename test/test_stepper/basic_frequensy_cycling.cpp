#include <Arduino.h>
#include "STM32Step.h"
#include "Config/serial_debug.h"
#include "Core/system_clock.h"

HardwareSerial SerialDebug(PA3, PA2);

// Pin definitions
static const uint8_t k_step_pin = STM32Step::PinConfig::StepPin::PIN;
static const uint8_t k_dir_pin = STM32Step::PinConfig::DirPin::PIN;
static const uint8_t k_enable_pin = STM32Step::PinConfig::EnablePin::PIN;

STM32Step::Stepper *stepper = nullptr;

// Test frequencies (Hz)
const uint32_t TEST_FREQUENCIES[] = {
    500, 1000, 2000, 5000, 7500, 10000, 12500, 15000, 17500, 20000};
const uint8_t NUM_FREQUENCIES = sizeof(TEST_FREQUENCIES) / sizeof(TEST_FREQUENCIES[0]);

uint8_t current_freq_index = 0;
uint32_t last_freq_change = 0;

void setNewFrequency(uint32_t freq)
{
    SerialDebug.print("\nChanging frequency to ");
    SerialDebug.print(freq);
    SerialDebug.println(" Hz");

    // Stop current motion
    STM32Step::TimerControl::stop();

    // Set new speed
    stepper->setMaxSpeed(freq);
    stepper->setSpeed(freq);

    // Restart motion with new frequency
    STM32Step::TimerControl::start(stepper);

    SerialDebug.println("Frequency change complete");
}

void setup()
{
    SerialDebug.begin(115200);
    delay(1000);
    SerialDebug.println("\n=== STM32Step Frequency Test ===");

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
    STM32Step::RuntimeConfig::current_pulse_width = 5;   // 5μs pulse
    STM32Step::RuntimeConfig::current_microsteps = 1600; // Full steps
    STM32Step::RuntimeConfig::current_max_speed = 20000; // Up to 20kHz

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
    stepper->setMicrosteps(1);
    stepper->setMaxSpeed(20000);
    stepper->enable();

    // Start with first frequency
    SerialDebug.println("Starting frequency cycling test...");
    setNewFrequency(TEST_FREQUENCIES[0]);
    last_freq_change = millis();
}

void loop()
{
    // Check if it's time to change frequency (every 3 seconds)
    if (millis() - last_freq_change >= 3000)
    {
        current_freq_index = (current_freq_index + 1) % NUM_FREQUENCIES;
        setNewFrequency(TEST_FREQUENCIES[current_freq_index]);
        last_freq_change = millis();
    }

    delay(1);
}
