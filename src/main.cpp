// File: src/main.cpp (Main Application: ELS with HMI Integration)

#include <Arduino.h>
#include "Config/serial_debug.h"    // For SerialDebug output
#include "Hardware/SystemClock.h"   // For SystemClock initialization
#include "Config/SystemConfig.h"    // For system defaults and runtime configs
#include <STM32Step.h>              // For STM32Step::PinConfig
#include "Hardware/EncoderTimer.h"  // For globalEncoderTimerInstance
#include "Motion/MotionControl.h"   // For motionCtrl instance
#include "LumenProtocol.h"          // For HMI communication
#include "Motion/FeedRateManager.h" // For managing feed rates

// Define actual hardware pins for the stepper motor from STM32Step's PinConfig
static const uint8_t ACTUAL_STEP_PIN = STM32Step::PinConfig::StepPin::PIN;
static const uint8_t ACTUAL_DIR_PIN = STM32Step::PinConfig::DirPin::PIN;
static const uint8_t ACTUAL_ENABLE_PIN = STM32Step::PinConfig::EnablePin::PIN;

// Serial port for debugging (e.g., to PC via ST-Link VCP)
HardwareSerial SerialDebug(PA3, PA2); // USART2_RX, USART2_TX

// Serial port for HMI communication (e.g., to Proculus display)
HardwareSerial SerialDisplay(PA10, PA9); // USART1_RX, USART1_TX (Ensure this matches your board's UART1 pins)

// HMI Variable Addresses (as defined in the Proculus HMI project)
const uint16_t mmInchSelectorAddress = 124;            // For MM/Inch unit selection
const uint16_t directionSelectorAddress = 129;         // For feed/thread direction (TODO: implement handling)
const uint16_t startSTOPFEEDAddress = 130;             // For Start/Stop motion command (TODO: implement handling)
const uint16_t rmpAddress = 131;                       // For displaying current RPM
const uint16_t actualFEEDRATEAddress = 132;            // For displaying current feed rate value and unit string
const uint16_t prevNEXTFEEDRATEVALUEAddress = 133;     // For Prev/Next feed rate selection buttons
const uint16_t actualFEEDRATEDESCRIPTIONAddress = 134; // For displaying current feed rate category string

// Lumen Protocol packet structures for HMI communication
lumen_packet_t mmInchSelectorPacket = {mmInchSelectorAddress, kS32};
lumen_packet_t directionSelectorPacket = {directionSelectorAddress, kS32}; // Placeholder
lumen_packet_t startSTOPFEEDPacket = {startSTOPFEEDAddress, kBool};        // Placeholder
lumen_packet_t rmpPacket = {rmpAddress, kS32};
lumen_packet_t actualFEEDRATEPacket = {actualFEEDRATEAddress, kString};
lumen_packet_t prevNEXTFEEDRATEVALUEPacket = {prevNEXTFEEDRATEVALUEAddress, kS32};
lumen_packet_t actualFEEDRATEDESCRIPTIONPacket = {actualFEEDRATEDESCRIPTIONAddress, kString};

// Global core component instances
EncoderTimer globalEncoderTimerInstance; // Global EncoderTimer (TIM2) - must be initialized before MotionControl if TIM1 is used by Stepper
MotionControl motionCtrl(MotionControl::MotionPins{ACTUAL_STEP_PIN, ACTUAL_DIR_PIN, ACTUAL_ENABLE_PIN});
FeedRateManager feedRateManager; // Manages feed rate tables and selection logic
char feedRateBuffer[40];         // Buffer for formatting strings to send to HMI

/**
 * @brief Lumen Protocol callback: Writes bytes to the HMI display serial port.
 * @param data Pointer to the byte array to write.
 * @param length Number of bytes to write.
 */
extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length)
{
    SerialDisplay.write(data, length);
}

/**
 * @brief Lumen Protocol callback: Reads a byte from the HMI display serial port.
 * @return The byte read, or DATA_NULL if no data is available.
 */
extern "C" uint16_t lumen_get_byte()
{
    if (SerialDisplay.available())
    {
        return SerialDisplay.read();
    }
    return DATA_NULL;
}

/**
 * @brief Setup function, runs once at startup.
 * Initializes serial ports, system clock, core motion components (EncoderTimer, MotionControl),
 * configures initial motion parameters, and sends initial data to the HMI.
 */
void setup()
{
    SerialDebug.begin(115200);
    delay(1000); // Allow time for serial monitor to connect
    SerialDebug.println("=== ELS Application Start: Feed Rate Test Mode ===");

    SerialDisplay.begin(115200, SERIAL_8N1); // Initialize HMI serial port
    delay(100);                              // Short delay for HMI serial

    // Initialize system clock (critical for all HAL timings)
    if (!SystemClock::GetInstance().initialize())
    {
        SerialDebug.println("CRITICAL ERROR: SystemClock initialization failed!");
        while (1)
            ; // Halt on critical error
    }

    // Initialize global EncoderTimer (TIM2)
    // IMPORTANT: This should be done before MotionControl::begin() if MotionControl's
    // Stepper/TimerControl uses TIM1, due to potential HAL shared resource initialization.
    if (!globalEncoderTimerInstance.begin())
    {
        SerialDebug.println("CRITICAL ERROR: globalEncoderTimerInstance.begin() failed!");
        // Optionally halt: while(1);
    }

    // Initialize MotionControl system (includes Stepper, STM32Step::TimerControl for TIM1, SyncTimer for TIM6)
    if (!motionCtrl.begin())
    {
        SerialDebug.println("CRITICAL ERROR: MotionControl begin failed!");
        while (1)
            ; // Halt on critical error
    }
    SerialDebug.println("Core Systems Initialized.");

    // Configure initial motion parameters
    MotionControl::Config cfg;
    cfg.thread_pitch = 0.5f;                                                    // Default feed rate (e.g., 0.5 mm/rev)
    cfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch; // Actual leadscrew pitch (e.g., 2.0 mm)
    cfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    cfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps; // Ensure HBS57 DIPs match or are "internal default"
    cfg.reverse_direction = false;
    cfg.sync_frequency = 10000; // SyncTimer ISR frequency (10kHz found to be smooth)

    motionCtrl.setConfig(cfg);
    SerialDebug.println("MotionControl Configured for Initial Feed Rate.");
    SerialDebug.print("  Initial Feed (thread_pitch): ");
    SerialDebug.print(cfg.thread_pitch, 4);
    SerialDebug.print(", Leadscrew Pitch: ");
    SerialDebug.println(cfg.leadscrew_pitch, 4);
    SerialDebug.print("  Microsteps: ");
    SerialDebug.println(cfg.microsteps);
    SerialDebug.print("  SyncTimer Freq (TIM6 ISR): ");
    SerialDebug.println(cfg.sync_frequency);

    motionCtrl.setMode(MotionControl::Mode::TURNING); // Set initial mode (e.g., TURNING or FEEDING)
    SerialDebug.println("MotionControl Mode Set to TURNING/FEEDING.");

    // Initialize HMI displays with default values
    rmpPacket.data._s32 = 0; // Initial RPM
    lumen_write_packet(&rmpPacket);

    feedRateManager.setMetric(true);                                          // Default to metric units
    feedRateManager.getDisplayString(feedRateBuffer, sizeof(feedRateBuffer)); // Format "value unit" string
    memcpy(actualFEEDRATEPacket.data._string, feedRateBuffer, MAX_STRING_SIZE - 1);
    actualFEEDRATEPacket.data._string[MAX_STRING_SIZE - 1] = '\0'; // Ensure null termination
    lumen_write_packet(&actualFEEDRATEPacket);

    const char *initialCategory = feedRateManager.getCurrentCategory(); // Format category string
    strncpy(actualFEEDRATEDESCRIPTIONPacket.data._string, initialCategory, MAX_STRING_SIZE - 1);
    actualFEEDRATEDESCRIPTIONPacket.data._string[MAX_STRING_SIZE - 1] = '\0'; // Ensure null termination
    lumen_write_packet(&actualFEEDRATEDESCRIPTIONPacket);
    SerialDebug.println("Initial HMI displays sent.");

    // Start the motion system (enables SyncTimer ISR and Stepper)
    motionCtrl.startMotion();
    SerialDebug.println("Motion System Started. Turn encoder & use HMI.");
}

/**
 * @brief Main application loop.
 * Handles periodic HMI updates (RPM) and processes incoming HMI commands.
 * Also prints status to SerialDebug periodically.
 * Note: Core motion synchronization is handled by SyncTimer's ISR, not directly in this loop.
 */
void loop()
{
    // motionCtrl.update(); // This call is no longer needed as SyncTimer logic is fully ISR-driven.
    // Kept commented for clarity on architectural change.

    // --- HMI RPM Update (Fast Interval) ---
    static uint32_t lastRpmHmiUpdateTime = 0;
    const uint32_t RPM_HMI_UPDATE_INTERVAL = 100; // Update HMI RPM every 100ms (10 Hz)

    if (millis() - lastRpmHmiUpdateTime >= RPM_HMI_UPDATE_INTERVAL)
    {
        MotionControl::Status mcStatus = motionCtrl.getStatus(); // Get current motion status
        rmpPacket.data._s32 = mcStatus.spindle_rpm;              // Prepare RPM packet
        lumen_write_packet(&rmpPacket);                          // Send to HMI
        lastRpmHmiUpdateTime = millis();
    }

    // --- Process Incoming HMI Packets ---
    // This loop runs as fast as possible to check for HMI input.
    // lumen_available() is non-blocking.
    while (lumen_available() > 0)
    {
        lumen_packet_t *packet = lumen_get_first_packet();
        if (packet != NULL)
        {
            // Optional: Less frequent debug for HMI RX:
            // static uint32_t lastHmiRxPrint = 0;
            // if(millis() - lastHmiRxPrint > 500) { SerialDebug.print("HMI RX Addr: "); SerialDebug.println(packet->address); lastHmiRxPrint = millis(); }

            if (packet->address == mmInchSelectorAddress)
            { // Unit selection (Metric/Imperial)
                bool isMetric = packet->data._s32 == 1;
                SerialDebug.print("HMI: Unit set to ");
                SerialDebug.println(isMetric ? "Metric" : "Imperial");
                feedRateManager.setMetric(isMetric);

                // Update MotionControl config with the new feed rate value from FeedRateManager
                MotionControl::Config currentCfg = motionCtrl.getConfig();
                currentCfg.thread_pitch = static_cast<float>(feedRateManager.getCurrentValue());
                motionCtrl.setConfig(currentCfg); // This will reconfigure SyncTimer
                SerialDebug.print("  MotionControl updated with new feed rate: ");
                SerialDebug.println(currentCfg.thread_pitch, 4);
            }
            else if (packet->address == prevNEXTFEEDRATEVALUEAddress)
            {                                            // Prev/Next feed rate buttons
                int32_t buttonValue = packet->data._s32; // 1 for PREV, 2 for NEXT
                SerialDebug.print("HMI: FeedRate button ");
                SerialDebug.println(buttonValue == 1 ? "PREV" : "NEXT");
                feedRateManager.handlePrevNextValue(buttonValue);

                // Update MotionControl config with the new feed rate value from FeedRateManager
                MotionControl::Config currentCfg = motionCtrl.getConfig();
                currentCfg.thread_pitch = static_cast<float>(feedRateManager.getCurrentValue());
                motionCtrl.setConfig(currentCfg); // This will reconfigure SyncTimer
                SerialDebug.print("  MotionControl updated with new feed rate: ");
                SerialDebug.println(currentCfg.thread_pitch, 4);
            }
            // TODO: Handle other HMI inputs:
            // - directionSelectorAddress: Would modify motionCtrl._config.reverse_direction and call setConfig.
            // - startSTOPFEEDAddress: Would call motionCtrl.startMotion() or motionCtrl.stopMotion().

            // After any FeedRateManager change, update HMI display for feed rate value and category
            feedRateManager.getDisplayString(feedRateBuffer, sizeof(feedRateBuffer));
            memcpy(actualFEEDRATEPacket.data._string, feedRateBuffer, MAX_STRING_SIZE - 1);
            actualFEEDRATEPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&actualFEEDRATEPacket);

            const char *categoryStr = feedRateManager.getCurrentCategory();
            strncpy(actualFEEDRATEDESCRIPTIONPacket.data._string, categoryStr, MAX_STRING_SIZE - 1);
            actualFEEDRATEDESCRIPTIONPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&actualFEEDRATEDESCRIPTIONPacket);
        }
    }

    // --- SerialDebug Status Print (Moderate Interval) ---
    static uint32_t lastDebugPrintTime = 0;
    const uint32_t DEBUG_PRINT_INTERVAL = 500; // Print status to SerialDebug every 500ms

    if (millis() - lastDebugPrintTime >= DEBUG_PRINT_INTERVAL)
    {
        MotionControl::Status status = motionCtrl.getStatus();
        SerialDebug.print("Status - Enc: ");
        SerialDebug.print(status.encoder_position);
        SerialDebug.print(" Step: ");
        SerialDebug.print(status.stepper_position);
        SerialDebug.print(" RPM: ");
        SerialDebug.println(status.spindle_rpm);
        if (status.error)
        {
            SerialDebug.print("MotionControl ERROR: ");
            SerialDebug.println(status.error_message);
        }
        lastDebugPrintTime = millis();
    }

    // delay(1); // Optional: Small delay if main loop becomes too tight and starves other potential tasks.
    // With periodic HMI/debug prints, it might not be necessary.
    // If HMI communication is very fast and non-blocking, and no other work,
    // a small delay can prevent 100% CPU usage on some microcontrollers,
    // though for STM32, WFI/WFE in idle is better if FreeRTOS or similar is used.
    // For bare-metal polling like this, it's a trade-off.
}
