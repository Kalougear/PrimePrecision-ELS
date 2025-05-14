#include <Arduino.h>
#include "STM32Step.h"
#include "Config/serial_debug.h"
#include "Hardware/SystemClock.h"
#include "Hardware/EncoderTimer.h"
#include "Config/SystemConfig.h"
#include <HardwareTimer.h>
#include "LumenProtocol.h"

// Debug Serial on PA3/PA2
HardwareSerial SerialDebug(PA3, PA2);

// Display Serial on PA9/PA10 (UART1)
HardwareSerial SerialDisplay(PA10, PA9); // RX, TX for UART1

// Define the RPM packet
lumen_packet_t rpm_packet = {
    .address = 121,  // VP address
    .type = kDouble, // Using double type for RPM
    .data = {0}      // Initialize data to 0
};

using namespace STM32Step;
using namespace SystemConfig;

// Components
EncoderTimer encoderTimer;
HardwareTimer *timer6;

// Moving average filter for RPM
const int RPM_SAMPLES = 10;
double rpmSamples[RPM_SAMPLES] = {0};
int sampleIndex = 0;
double rpmSum = 0;

// Required C linkage functions for the Lumen Protocol
extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length)
{
    SerialDisplay.write(data, length);
}

extern "C" uint16_t lumen_get_byte()
{
    if (SerialDisplay.available())
    {
        return SerialDisplay.read();
    }
    return DATA_NULL;
}

// Calculate moving average
double updateMovingAverage(double newValue)
{
    // Subtract the oldest sample and add the new one to the sum
    rpmSum = rpmSum - rpmSamples[sampleIndex] + newValue;

    // Store the new sample
    rpmSamples[sampleIndex] = newValue;

    // Update index for next sample
    sampleIndex = (sampleIndex + 1) % RPM_SAMPLES;

    // Return the average
    return rpmSum / RPM_SAMPLES;
}

void setup()
{
    // Initialize debug serial
    SerialDebug.begin(115200, SERIAL_8N1);
    delay(1000);
    SerialDebug.println("\n=== Encoder RPM Display System ===");

    // Initialize display serial with 8N1 data frame
    SerialDisplay.begin(115200, SERIAL_8N1);
    delay(100);
    SerialDebug.println("Display Serial Initialized");

    // Initialize encoder
    encoderTimer.begin();
    SerialDebug.println("Encoder initialized");

    // Initialize moving average array
    for (int i = 0; i < RPM_SAMPLES; i++)
    {
        rpmSamples[i] = 0;
    }
    rpmSum = 0;

    // Initial display test
    rpm_packet.data._double = 0.0;
    if (lumen_write_packet(&rpm_packet))
    {
        SerialDebug.println("Display communication test successful");
    }
    else
    {
        SerialDebug.println("Display communication test failed");
    }

    SerialDebug.println("System ready");
}

void loop()
{
    static uint32_t lastDisplayUpdate = 0;
    const uint32_t DISPLAY_UPDATE_INTERVAL = 100; // Update every 100ms
    static double lastRpm = 0;

    // Update display at regular intervals
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
    {
        EncoderTimer::Position pos = encoderTimer.getPosition();

        if (pos.valid)
        {
            // Handle RPM conversion carefully
            int16_t rawRpm = pos.rpm;

            // Convert to double before taking absolute value
            double rpm = (rawRpm < 0) ? -static_cast<double>(rawRpm) : static_cast<double>(rawRpm);

            // Apply moving average
            double smoothedRpm = updateMovingAverage(rpm);

            // Only update if smoothed RPM has changed significantly (0.1 RPM threshold)
            if (abs(smoothedRpm - lastRpm) > 0.1)
            {
                rpm_packet.data._double = smoothedRpm;

                if (lumen_write_packet(&rpm_packet))
                {
                    SerialDebug.print("Raw RPM: ");
                    SerialDebug.print(rawRpm);
                    SerialDebug.print(" Converted RPM: ");
                    SerialDebug.print(rpm, 2);
                    SerialDebug.print(" Smoothed RPM: ");
                    SerialDebug.println(smoothedRpm, 2);
                }
                else
                {
                    SerialDebug.println("Failed to send RPM");
                }

                lastRpm = smoothedRpm;
            }
        }

        lastDisplayUpdate = millis();
    }

    // Process any incoming packets
    while (lumen_available() > 0)
    {
        lumen_packet_t *packet = lumen_get_first_packet();
        if (packet != NULL)
        {
            SerialDebug.print("Received packet from address: ");
            SerialDebug.println(packet->address);
        }
    }

    delay(1);
}
