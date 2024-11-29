#include <Arduino.h>
#include "encoder_timer_test.h"

// Debug serial
const uint8_t k_debug_tx = PA2;
const uint8_t k_debug_rx = PA3;
HardwareSerial SerialDebug(k_debug_rx, k_debug_tx);

// Global encoder instance
EncoderTimer encoder;

void setup()
{
SerialDebug.begin(115200);
delay(100);
SerialDebug.println("\nStarting Encoder Test");

    if (!encoder.begin())
    {
        SerialDebug.println("Failed to initialize encoder!");
        while (1)
            ;
    }

    SerialDebug.println("Encoder initialized successfully");

}

void loop()
{
static uint32_t last_print = 0;
static int32_t last_count = 0;

    // Print every 500ms
    if (millis() - last_print >= 500)
    {
        int32_t current_count = encoder.get_count();
        int32_t delta = current_count - last_count;

        // Calculate RPM (for 1024 PPR encoder)
        float rpm = (delta * 60.0f * 2.0f) / (1024.0f * 4.0f); // *2 because we're sampling every 500ms

        SerialDebug.print("Position: ");
        SerialDebug.print(current_count);
        SerialDebug.print(" Delta: ");
        SerialDebug.print(delta);
        SerialDebug.print(" RPM: ");
        SerialDebug.println(rpm, 1);

        last_count = current_count;
        last_print = millis();
    }

}
