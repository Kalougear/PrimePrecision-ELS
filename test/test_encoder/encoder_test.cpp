#include <Arduino.h>
#include "Core/encoder_timer.h"

// Debug serial setup
const uint8_t k_debug_tx = PA2;
const uint8_t k_debug_rx = PA3;
HardwareSerial SerialDebug(k_debug_rx, k_debug_tx);

// Global encoder instance
EncoderTimer encoder;

void setup()
{
    // Initialize debug serial
    SerialDebug.begin(115200);
    delay(100);
    SerialDebug.println("\nELS Encoder Test Starting...");

    // Initialize encoder
    if (!encoder.begin())
    {
        SerialDebug.println("Encoder initialization failed!");
        while (1)
            ; // Halt if encoder init fails
    }
    SerialDebug.println("Encoder initialized successfully");
}

void loop()
{
    static uint32_t last_print = 0;
    static int32_t last_count = 0;
    const uint32_t PRINT_INTERVAL = 100;

    if (millis() - last_print >= PRINT_INTERVAL)
    {
        // Read directly from timer
        int32_t timer_cnt = TIM2->CNT;
        int32_t current_count = encoder.getCount();
        int32_t delta = current_count - last_count;

        SerialDebug.print("Timer CNT: ");
        SerialDebug.print(timer_cnt);
        SerialDebug.print(" Count: ");
        SerialDebug.print(current_count);
        SerialDebug.print(" Delta: ");
        SerialDebug.print(delta);
        SerialDebug.print(" Pins A:");
        SerialDebug.print(digitalRead(PA0));
        SerialDebug.print(" B:");
        SerialDebug.print(digitalRead(PA1));
        SerialDebug.print(" CR1:0x");
        SerialDebug.print(TIM2->CR1, HEX);
        SerialDebug.print(" SMCR:0x");
        SerialDebug.println(TIM2->SMCR, HEX);

        last_count = current_count;
        last_print = millis();
    }
}