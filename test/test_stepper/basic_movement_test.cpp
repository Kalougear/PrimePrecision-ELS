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

// Default settings
const uint32_t DEFAULT_SPEED = 2000; // Hz
const uint32_t MICROSTEPS = 1600;    // Physical microsteps per revolution

void printStatus()
{
    auto status = stepper->getStatus();
    SerialDebug.println("\n=== Stepper Status ===");
    SerialDebug.print("Position: ");
    SerialDebug.print(status.currentPosition);
    SerialDebug.print(" steps (");
    SerialDebug.print((float)status.currentPosition / MICROSTEPS, 3);
    SerialDebug.println(" revolutions)");

    SerialDebug.print("Speed: ");
    SerialDebug.print(status.currentSpeed);
    SerialDebug.println(" Hz");

    SerialDebug.print("Motor Enabled: ");
    SerialDebug.println(status.enabled ? "Yes" : "No");
    SerialDebug.println("=====================");
}

void setup()
{
    SerialDebug.begin(115200);
    delay(1000);
    SerialDebug.println("\n=== STM32Step Position Control ===");

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
    STM32Step::RuntimeConfig::current_pulse_width = 10;  // 5μs pulse
    STM32Step::RuntimeConfig::current_microsteps = 1600; // Match physical setup
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

    // Configure stepper - Set all parameters before enabling
    stepper->setMicrosteps(MICROSTEPS); // Match physical setup
    stepper->setMaxSpeed(20000);
    stepper->setSpeed(DEFAULT_SPEED);

    // Print initial configuration
    SerialDebug.print("Initial speed set to: ");
    SerialDebug.println(DEFAULT_SPEED);
    SerialDebug.print("Microsteps set to: ");
    SerialDebug.println(MICROSTEPS);

    // Enable motor after configuration
    stepper->enable();

    SerialDebug.println("\nAvailable Commands:");
    SerialDebug.println("p[number] - Move to absolute position in steps (e.g., p1600 for 1 revolution)");
    SerialDebug.println("r[number] - Move relative number of steps (e.g., r800 for 1/2 revolution)");
    SerialDebug.println("s[number] - Set speed in Hz (e.g., s2000)");
    SerialDebug.println("e - Enable/Disable motor");
    SerialDebug.println("? - Print status");
    SerialDebug.println("h - Show this help");

    // Print initial status
    printStatus();
}

void processCommand(String cmd)
{
    if (cmd.startsWith("p"))
    {
        // Absolute position command
        int32_t pos = cmd.substring(1).toInt();
        SerialDebug.print("Moving to position: ");
        SerialDebug.print(pos);
        SerialDebug.print(" steps (");
        SerialDebug.print((float)pos / MICROSTEPS, 3);
        SerialDebug.println(" revolutions)");
        stepper->moveSteps(pos - stepper->getCurrentPosition());
    }
    else if (cmd.startsWith("r"))
    {
        // Relative position command
        int32_t steps = cmd.substring(1).toInt();
        SerialDebug.print("Moving relative: ");
        SerialDebug.print(steps);
        SerialDebug.print(" steps (");
        SerialDebug.print((float)steps / MICROSTEPS, 3);
        SerialDebug.println(" revolutions)");
        stepper->moveSteps(steps);
    }
    else if (cmd.startsWith("s"))
    {
        // Speed command
        uint32_t speed = cmd.substring(1).toInt();
        if (speed < 100)
            speed = 100; // Enforce minimum speed
        SerialDebug.print("Setting speed to: ");
        SerialDebug.print(speed);
        SerialDebug.println(" Hz");
        stepper->setSpeed(speed);
        printStatus();
    }
    else if (cmd == "e")
    {
        if (stepper->isEnabled())
        {
            stepper->disable();
            SerialDebug.println("Motor disabled");
        }
        else
        {
            stepper->enable();
            SerialDebug.println("Motor enabled");
        }
        printStatus();
    }
    else if (cmd == "?")
    {
        printStatus();
    }
    else if (cmd == "h")
    {
        SerialDebug.println("\nAvailable Commands:");
        SerialDebug.println("p[number] - Move to absolute position in steps (e.g., p1600 for 1 revolution)");
        SerialDebug.println("r[number] - Move relative number of steps (e.g., r800 for 1/2 revolution)");
        SerialDebug.println("s[number] - Set speed in Hz (e.g., s2000)");
        SerialDebug.println("e - Enable/Disable motor");
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

    delay(1); // Short delay to prevent overwhelming the serial port
}
