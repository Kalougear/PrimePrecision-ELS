#include <Arduino.h>
#include "Config/serial_debug.h"
#include "Core/system_clock.h" // Add this!

HardwareSerial SerialDebug(PA3, PA2);

void setup()
{
    // Initialize system clock first!
    if (!SystemClock::GetInstance().initialize())
    {
        while (1)
        {
        } // Halt on failure
    }

    // Wait for clock stability
    if (!SystemClock::GetInstance().IsClockStable())
    {
        while (1)
        {
        } // Halt if unstable
    }

    // Now it's safe to initialize serial
    SerialDebug.begin(115200);
    delay(1000);
    SerialDebug.println("Hello World");
}

void loop()
{
    SerialDebug.println("Test message");
    delay(1000);
}

int main()
{
    // Initialize HAL
    HAL_Init();

    // Initialize Arduino framework
    init();

    setup();

    while (1)
    {
        loop();
    }
    return 0;
}