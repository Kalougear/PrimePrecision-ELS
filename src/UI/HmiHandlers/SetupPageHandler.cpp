#include "UI/HmiHandlers/SetupPageHandler.h"
#include "Config/Hmi/SetupPageOptions.h" // All HMI addresses and lists for Setup Page
#include "Config/SystemConfig.h"
#include <HardwareSerial.h> // Required for HardwareSerial type
#include "LumenProtocol.h"
#include <Arduino.h>                // For snprintf, dtostrf, atoi, atof
#include <string.h>                 // For strncpy, memcpy
#include "Motion/FeedRateManager.h" // For FeedRateManager
#include "stm32h7xx_hal.h"          // For HAL_FLASH_Unlock/Lock

extern HardwareSerial SerialDebug;      // Declare SerialDebug as extern
extern FeedRateManager feedRateManager; // Declare global feedRateManager

// Buffer for formatting strings to send to HMI
// Ensure this is large enough for any formatted string, e.g., "123.45 mm"
static char hmiDisplayStringBuffer[40];

// Current index for cyclable lists - specific to Setup Page state
static uint8_t currentPprIndex = 0;
static uint8_t currentZLeadscrewPitchIndex = 0;
static uint8_t currentZDriverMicrosteppingIndex = 0;

// Helper (already in main.cpp, can be made static here or put in a common util if used elsewhere)
template <typename T>
static uint8_t find_initial_index(const T *list, uint8_t list_size, T target_value)
{
    for (uint8_t i = 0; i < list_size; ++i)
    {
        if (list[i] == target_value)
            return i;
    }
    return 0; // Default to first item if not found
}

void SetupPageHandler::init()
{
    // Initialize current indices based on SystemConfig values
    currentPprIndex = find_initial_index(HmiSetupPageOptions::pprList,
                                         HmiSetupPageOptions::pprListSize,
                                         SystemConfig::RuntimeConfig::Encoder::ppr);

    // Initialize currentZLeadscrewPitchIndex based on current standard
    if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
    {
        currentZLeadscrewPitchIndex = find_initial_index(HmiSetupPageOptions::zLeadscrewMetricPitchList,
                                                         HmiSetupPageOptions::zLeadscrewMetricPitchListSize,
                                                         SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch);
    }
    else // Imperial
    {
        // For imperial, we store pitch in inches. We need to find the TPI in the list that matches.
        // (1.0f / pitch_in_inches) = TPI. find_initial_index expects exact match.
        // A simpler init: default to the first TPI value if no exact match for current pitch.
        // Or, upon loading, if imperial, convert stored pitch to TPI and find closest.
        // For now, find_initial_index will default to 0 if no exact match.
        // This means if the stored value isn't perfectly 1.0/TPI_from_list, it will show the first TPI.
        float targetTpi = 0.0f;
        if (SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch != 0)
        { // Avoid division by zero
            targetTpi = 1.0f / SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
        }
        currentZLeadscrewPitchIndex = find_initial_index(HmiSetupPageOptions::zLeadscrewImperialTpiList,
                                                         HmiSetupPageOptions::zLeadscrewImperialTpiListSize,
                                                         targetTpi);
        // If not found, it defaults to index 0. Let's ensure the stored pitch matches this default if so.
        // This part is tricky because find_initial_index needs an exact match.
        // A robust init would find the *closest* TPI, or simply set to the first TPI on init.
        // For now, we rely on the fact that if it's imperial, the saved lead_screw_pitch should correspond to one of the 1.0f/TPI values.
    }

    currentZDriverMicrosteppingIndex = find_initial_index(HmiSetupPageOptions::zDriverMicrosteppingList,
                                                          HmiSetupPageOptions::zDriverMicrosteppingListSize,
                                                          SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    SerialDebug.println("SetupPageHandler initialized.");
}

void SetupPageHandler::onEnterPage()
{
    SerialDebug.println("*** SetupPageHandler::onEnterPage() CALLED! ***"); // Diagnostic print
    SerialDebug.flush();                                                    // Ensure it's sent

    SerialDebug.println("SetupPageHandler: onEnterPage - Sending initial HMI values.");
    lumen_packet_t packet; // Reusable packet structure

    // 1. Measurement Unit Default (ADDR_MEASUREMENT_UNIT_DEFAULT_TOGGLE)
    packet.address = HmiSetupPageOptions::ADDR_MEASUREMENT_UNIT_DEFAULT_TOGGLE;
    packet.type = kBool;
    // SystemConfig bool is true for Metric, false for Imperial.
    // HMI expects 0 for Metric, 1 for Imperial for its visual state.
    packet.data._bool = !SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;
    lumen_write_packet(&packet);

    // 2. ELS Default Feed Unit (ADDR_ELS_FEED_UNIT_DEFAULT_TOGGLE)
    packet.address = HmiSetupPageOptions::ADDR_ELS_FEED_UNIT_DEFAULT_TOGGLE; // This is 154
    packet.type = kBool;
    // SystemConfig bool is true for mm/rev (metric), false for in/rev (imperial).
    // For THIS HMI toggle's visual state (address 154):
    // - To show "mm/rev" selected, HMI expects to receive 0.
    // - To show "in/rev" selected, HMI expects to receive 1.
    // This is inverted compared to ADDR_MEASUREMENT_UNIT_DEFAULT_TOGGLE (151) for visual state,
    // but consistent with how HMI sends events (0 for mm/rev, 1 for in/rev).
    packet.data._bool = !SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric; // Send 0 if metric (true), 1 if imperial (false)
    lumen_write_packet(&packet);

    // 3. Spindle Chuck Pulley Teeth (ADDR_SPINDLE_CHUCK_TEETH_STRING)
    packet.address = HmiSetupPageOptions::ADDR_SPINDLE_CHUCK_TEETH_STRING;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 4. Spindle Encoder Pulley Teeth (ADDR_SPINDLE_ENCODER_TEETH_STRING)
    packet.address = HmiSetupPageOptions::ADDR_SPINDLE_ENCODER_TEETH_STRING;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 5. Encoder PPR Display (ADDR_PPR_DISPLAY)
    packet.address = HmiSetupPageOptions::ADDR_PPR_DISPLAY;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Encoder::ppr);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 6. Z Leadscrew Pitch Display (ADDR_LEADSCREW_PITCH_DISPLAY)
    packet.address = HmiSetupPageOptions::ADDR_LEADSCREW_PITCH_DISPLAY;
    packet.type = kString;
    // Format with units based on current standard and selected pitch/TPI
    if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
    {
        dtostrf(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch, 4, 2, hmiDisplayStringBuffer); // Display direct metric pitch
        strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    else // Imperial
    {
        // Display TPI value from the list
        // Ensure currentZLeadscrewPitchIndex is valid for zLeadscrewImperialTpiList
        if (currentZLeadscrewPitchIndex >= HmiSetupPageOptions::zLeadscrewImperialTpiListSize)
        {
            currentZLeadscrewPitchIndex = 0; // Safety reset
        }
        // Use dtostrf for TPI value as it's float, e.g. "10.0 TPI" or "4.0 TPI"
        dtostrf(HmiSetupPageOptions::zLeadscrewImperialTpiList[currentZLeadscrewPitchIndex], 4, 1, hmiDisplayStringBuffer);
        strncat(hmiDisplayStringBuffer, " TPI", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 7. Z Motor Pulley Teeth (ADDR_Z_MOTOR_PULLEY_TEETH_STRING)
    packet.address = HmiSetupPageOptions::ADDR_Z_MOTOR_PULLEY_TEETH_STRING;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 8. Z Leadscrew Pulley Teeth (ADDR_Z_LEADSCREW_PULLEY_TEETH_STRING)
    packet.address = HmiSetupPageOptions::ADDR_Z_LEADSCREW_PULLEY_TEETH_STRING;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 9. Z Driver Microstepping Display (ADDR_MICROSTEP_DISPLAY)
    packet.address = HmiSetupPageOptions::ADDR_MICROSTEP_DISPLAY;
    packet.type = kString;
    snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%lu", SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 10. Z Invert Direction (ADDR_Z_INVERT_DIR_TOGGLE) - No longer sending initial state to HMI
    // packet.address = HmiSetupPageOptions::ADDR_Z_INVERT_DIR_TOGGLE;
    // packet.type = kBool;
    // packet.data._bool = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
    // lumen_write_packet(&packet);

    // 11. Motor Enable Polarity (ADDR_Z_MOTOR_ENABLE_POL_TOGGLE)
    packet.address = HmiSetupPageOptions::ADDR_Z_MOTOR_ENABLE_POL_TOGGLE;
    packet.type = kBool;
    packet.data._bool = SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high;
    lumen_write_packet(&packet);

    // 12. Z Max Jog Speed (ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING)
    packet.address = HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING; // Use new DISPLAY address
    packet.type = kString;
    dtostrf(SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min, 7, 2, hmiDisplayStringBuffer); // Corrected variable
    if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
    { // Assuming Z-axis specific parameters follow leadscrew standard for units
        strncat(hmiDisplayStringBuffer, " mm/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    else
    {
        strncat(hmiDisplayStringBuffer, " in/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    SerialDebug.print("onEnterPage: Sending Z Max Jog Speed (to HMI addr ");
    SerialDebug.print(HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING);
    SerialDebug.print("): '");
    SerialDebug.print(hmiDisplayStringBuffer);
    SerialDebug.println("'");
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 13. Z Jog Acceleration (ADDR_Z_JOG_ACCEL_DISPLAY_STRING)
    packet.address = HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_DISPLAY_STRING; // Use new DISPLAY address
    packet.type = kString;
    dtostrf(SystemConfig::RuntimeConfig::Z_Axis::acceleration, 6, 1, hmiDisplayStringBuffer);
    if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
    {
        strncat(hmiDisplayStringBuffer, " mm/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    else
    {
        strncat(hmiDisplayStringBuffer, " in/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    SerialDebug.print("onEnterPage: Sending Z Jog Accel (to HMI addr ");
    SerialDebug.print(HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_DISPLAY_STRING);
    SerialDebug.print("): '");
    SerialDebug.print(hmiDisplayStringBuffer);
    SerialDebug.println("'");
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 14. Z Backlash Compensation (ADDR_Z_BACKLASH_COMP_DISPLAY_STRING)
    packet.address = HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_DISPLAY_STRING; // Use new DISPLAY address
    packet.type = kString;
    dtostrf(SystemConfig::RuntimeConfig::Z_Axis::backlash_compensation, 4, 2, hmiDisplayStringBuffer);
    if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
    {
        strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    else
    {
        strncat(hmiDisplayStringBuffer, " in", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
    }
    SerialDebug.print("onEnterPage: Sending Z Backlash Comp (to HMI addr ");
    SerialDebug.print(HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_DISPLAY_STRING);
    SerialDebug.print("): '");
    SerialDebug.print(hmiDisplayStringBuffer);
    SerialDebug.println("'");
    strncpy(packet.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    // 15. Z Leadscrew Standard (ADDR_Z_LEADSCREW_STANDARD_TOGGLE)
    packet.address = HmiSetupPageOptions::ADDR_Z_LEADSCREW_STANDARD_TOGGLE;
    packet.type = kBool;
    packet.data._bool = (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric == false); // 0=Metric, 1=Imperial
    lumen_write_packet(&packet);
}

void SetupPageHandler::handlePacket(const lumen_packet_t *packet)
{
    if (!packet)
        return;

    // Temporary packet for sending updates back to HMI
    lumen_packet_t responsePacket;
    responsePacket.type = kString; // Most display updates are strings

    // --- Column 1: General System & Spindle/Encoder ---
    if (packet->address == HmiSetupPageOptions::ADDR_MEASUREMENT_UNIT_DEFAULT_TOGGLE)
    {
        // HMI sends 0 for Metric, 1 for Imperial.
        // SystemConfig::RuntimeConfig::System::measurement_unit_is_metric is true for Metric, false for Imperial.
        bool new_value_is_metric = (packet->data._bool == 0);
        if (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric != new_value_is_metric)
        {
            SystemConfig::RuntimeConfig::System::measurement_unit_is_metric = new_value_is_metric; // true if HMI sent 0 (Metric)
            SystemConfig::RuntimeConfigDirtyFlags::System::measurement_unit_is_metric = true;
            SerialDebug.print("SetupHandler: SystemConfig Meas. Unit is_metric (T=Metric,F=Imperial) set to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric);
            // feedRateManager.setMetric() expects true for Metric.
            feedRateManager.setMetric(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric); // Pass directly
            SerialDebug.println("SetupHandler: FeedRateManager metric status updated.");
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_ELS_FEED_UNIT_DEFAULT_TOGGLE)
    {
        // HMI sends 0 for mm/rev, 1 for in/rev.
        // SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric is true for mm/rev, false for in/rev.
        bool new_value_is_metric = (packet->data._bool == 0);
        if (SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric != new_value_is_metric)
        {
            SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric = new_value_is_metric; // true if HMI sent 0 (mm/rev)
            SystemConfig::RuntimeConfigDirtyFlags::System::els_default_feed_rate_unit_is_metric = true;
            SerialDebug.print("SetupHandler: SystemConfig ELS Feed Unit is_metric (T=mm/rev,F=in/rev) set to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_SPINDLE_CHUCK_TEETH_STRING)
    {
        uint16_t value = atoi(packet->data._string);
        if (SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth != value)
        {
            SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth = value;
            SystemConfig::RuntimeConfigDirtyFlags::Spindle::chuck_pulley_teeth = true;
            SerialDebug.print("SetupHandler: Spindle Chuck Pulley Teeth set to: ");
            SerialDebug.println(value);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_SPINDLE_ENCODER_TEETH_STRING)
    {
        uint16_t value = atoi(packet->data._string);
        if (SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth != value)
        {
            SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth = value;
            SystemConfig::RuntimeConfigDirtyFlags::Spindle::encoder_pulley_teeth = true;
            SerialDebug.print("SetupHandler: Spindle Encoder Pulley Teeth set to: ");
            SerialDebug.println(value);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_PPR_PULSE)
    {
        if (packet->data._bool)
        { // Pulse to cycle
            currentPprIndex = (currentPprIndex + 1) % HmiSetupPageOptions::pprListSize;
            uint16_t new_ppr = HmiSetupPageOptions::pprList[currentPprIndex];
            if (SystemConfig::RuntimeConfig::Encoder::ppr != new_ppr)
            {
                SystemConfig::RuntimeConfig::Encoder::ppr = new_ppr;
                SystemConfig::RuntimeConfigDirtyFlags::Encoder::ppr = true;
            }

            responsePacket.address = HmiSetupPageOptions::ADDR_PPR_DISPLAY;
            snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%u", SystemConfig::RuntimeConfig::Encoder::ppr);
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: PPR cycled to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::Encoder::ppr);
        }
    }
    // --- Column 2: Z Mechanical & Gearing, Z Motor & Driver ---
    else if (packet->address == HmiSetupPageOptions::ADDR_LEADSCREW_PITCH_PULSE)
    {
        if (packet->data._bool) // Pulse to cycle
        {
            float old_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
            if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
            {
                currentZLeadscrewPitchIndex = (currentZLeadscrewPitchIndex + 1) % HmiSetupPageOptions::zLeadscrewMetricPitchListSize;
                SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = HmiSetupPageOptions::zLeadscrewMetricPitchList[currentZLeadscrewPitchIndex];
                dtostrf(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch, 4, 2, hmiDisplayStringBuffer);
                strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            else // Imperial
            {
                currentZLeadscrewPitchIndex = (currentZLeadscrewPitchIndex + 1) % HmiSetupPageOptions::zLeadscrewImperialTpiListSize;
                float currentTpi = HmiSetupPageOptions::zLeadscrewImperialTpiList[currentZLeadscrewPitchIndex];
                if (currentTpi != 0)
                { // Avoid division by zero
                    SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = 1.0f / currentTpi;
                }
                else
                {
                    SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = 0.0f; // Or some error state
                }
                dtostrf(currentTpi, 4, 1, hmiDisplayStringBuffer); // Display TPI value
                strncat(hmiDisplayStringBuffer, " TPI", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }

            if (SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch != old_pitch)
            {
                SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = true;
            }

            responsePacket.address = HmiSetupPageOptions::ADDR_LEADSCREW_PITCH_DISPLAY;
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Leadscrew Pitch cycled to: ");
            SerialDebug.println(hmiDisplayStringBuffer);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_MOTOR_PULLEY_TEETH_STRING)
    {
        uint16_t value = atoi(packet->data._string);
        if (SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth != value)
        {
            SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth = value;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::motor_pulley_teeth = true;
            SerialDebug.print("SetupHandler: Z Motor Pulley Teeth set to: ");
            SerialDebug.println(value);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_LEADSCREW_PULLEY_TEETH_STRING)
    {
        uint16_t value = atoi(packet->data._string);
        if (SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth != value)
        {
            SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth = value;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pulley_teeth = true;
            SerialDebug.print("SetupHandler: Z Leadscrew Pulley Teeth set to: ");
            SerialDebug.println(value);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_MICROSTEP_PULSE)
    {
        if (packet->data._bool)
        { // Pulse to cycle
            currentZDriverMicrosteppingIndex = (currentZDriverMicrosteppingIndex + 1) % HmiSetupPageOptions::zDriverMicrosteppingListSize;
            uint32_t new_driver_pulses = HmiSetupPageOptions::zDriverMicrosteppingList[currentZDriverMicrosteppingIndex];
            if (SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev != new_driver_pulses)
            {
                SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev = new_driver_pulses;
                SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::driver_pulses_per_rev = true;
            }

            responsePacket.address = HmiSetupPageOptions::ADDR_MICROSTEP_DISPLAY;
            snprintf(hmiDisplayStringBuffer, sizeof(hmiDisplayStringBuffer), "%lu", SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Microstepping cycled to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_INVERT_DIR_TOGGLE)
    {
        if (SystemConfig::RuntimeConfig::Z_Axis::invert_direction != packet->data._bool)
        {
            SystemConfig::RuntimeConfig::Z_Axis::invert_direction = packet->data._bool;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::invert_direction = true;
            SerialDebug.print("SetupHandler: Z Invert Dir set to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::invert_direction);
        }
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_MOTOR_ENABLE_POL_TOGGLE)
    {
        if (SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high != packet->data._bool)
        {
            SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high = packet->data._bool;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::enable_polarity_active_high = true;
            SerialDebug.print("SetupHandler: Z Enable Polarity Active High set to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high);
        }
    }
    // --- Column 3: Z Performance & Tuning, Z Calibration & Info ---
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_INPUT_STRING) // Use new INPUT address
    {
        float value = atof(packet->data._string);
        SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min = value;          // Corrected variable
        SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::max_jog_speed_mm_per_min = true; // Set dirty flag
        SerialDebug.print("SetupHandler: Z Max Jog Speed (from HMI addr ");
        SerialDebug.print(HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_INPUT_STRING);
        SerialDebug.print(") set to: ");
        SerialDebug.println(value);

        // Update HMI display in real-time
        responsePacket.address = HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING;
        // responsePacket.type is already kString
        dtostrf(SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min, 7, 2, hmiDisplayStringBuffer); // Corrected variable
        if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
        {
            strncat(hmiDisplayStringBuffer, " mm/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        else
        {
            strncat(hmiDisplayStringBuffer, " in/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
        responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&responsePacket);
        SerialDebug.print("SetupHandler: Updated Z Max Jog Speed display to: ");
        SerialDebug.println(hmiDisplayStringBuffer);
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_INPUT_STRING) // Use new INPUT address
    {
        float value = atof(packet->data._string);
        if (SystemConfig::RuntimeConfig::Z_Axis::acceleration != value)
        {
            SystemConfig::RuntimeConfig::Z_Axis::acceleration = value;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::acceleration = true;
        }
        SerialDebug.print("SetupHandler: Z Acceleration (Jog) (from HMI addr ");
        SerialDebug.print(HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_INPUT_STRING);
        SerialDebug.print(") set to: ");
        SerialDebug.println(value);

        // Update HMI display in real-time
        responsePacket.address = HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_DISPLAY_STRING;
        // responsePacket.type is already kString
        dtostrf(SystemConfig::RuntimeConfig::Z_Axis::acceleration, 6, 1, hmiDisplayStringBuffer);
        if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
        {
            strncat(hmiDisplayStringBuffer, " mm/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        else
        {
            strncat(hmiDisplayStringBuffer, " in/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
        responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&responsePacket);
        SerialDebug.print("SetupHandler: Updated Z Jog Accel display to: ");
        SerialDebug.println(hmiDisplayStringBuffer);
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_INPUT_STRING) // Use new INPUT address
    {
        float value = atof(packet->data._string);
        if (SystemConfig::RuntimeConfig::Z_Axis::backlash_compensation != value)
        {
            SystemConfig::RuntimeConfig::Z_Axis::backlash_compensation = value;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::backlash_compensation = true;
        }
        SerialDebug.print("SetupHandler: Z Backlash Comp (from HMI addr ");
        SerialDebug.print(HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_INPUT_STRING);
        SerialDebug.print(") set to: ");
        SerialDebug.println(value);

        // Update HMI display in real-time
        responsePacket.address = HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_DISPLAY_STRING;
        // responsePacket.type is already kString
        dtostrf(SystemConfig::RuntimeConfig::Z_Axis::backlash_compensation, 4, 2, hmiDisplayStringBuffer);
        if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
        {
            strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        else
        {
            strncat(hmiDisplayStringBuffer, " in", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
        }
        strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
        responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&responsePacket);
        SerialDebug.print("SetupHandler: Updated Z Backlash Comp display to: ");
        SerialDebug.println(hmiDisplayStringBuffer);
    }
    else if (packet->address == HmiSetupPageOptions::ADDR_Z_LEADSCREW_STANDARD_TOGGLE)
    {
        bool new_standard_is_metric = (packet->data._bool == 0); // 0 = Metric
        if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric != new_standard_is_metric)
        {
            SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric = new_standard_is_metric;
            SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::leadscrew_standard_is_metric = true;
            SerialDebug.print("SetupHandler: Z Leadscrew Standard Metric set to: ");
            SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric);

            // If unit system changed, reset pitch index and update stored pitch to first of new list, then update all relevant displays
            // This also means the lead_screw_pitch value changed, so mark it dirty.
            currentZLeadscrewPitchIndex = 0; // Reset index
            if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
            {
                SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = HmiSetupPageOptions::zLeadscrewMetricPitchList[0];
                SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = true;
                dtostrf(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch, 4, 2, hmiDisplayStringBuffer);
                strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            else // Imperial
            {
                float firstTpi = HmiSetupPageOptions::zLeadscrewImperialTpiList[0];
                if (firstTpi != 0)
                {
                    SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = 1.0f / firstTpi;
                }
                else
                {
                    SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch = 0.0f;
                }
                SystemConfig::RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = true;
                dtostrf(firstTpi, 4, 1, hmiDisplayStringBuffer);
                strncat(hmiDisplayStringBuffer, " TPI", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            responsePacket.address = HmiSetupPageOptions::ADDR_LEADSCREW_PITCH_DISPLAY;
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Leadscrew Pitch display updated due to unit change: ");
            SerialDebug.println(hmiDisplayStringBuffer);

            // Also update units for Max Speed, Accel, and Backlash Comp
            // Z Max Jog Speed
            responsePacket.address = HmiSetupPageOptions::ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING;
            dtostrf(SystemConfig::RuntimeConfig::Z_Axis::max_feed_rate, 7, 2, hmiDisplayStringBuffer);
            if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
            {
                strncat(hmiDisplayStringBuffer, " mm/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            else
            {
                strncat(hmiDisplayStringBuffer, " in/min", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Z Max Jog Speed display updated due to unit change: ");
            SerialDebug.println(hmiDisplayStringBuffer);

            // Z Jog Acceleration
            responsePacket.address = HmiSetupPageOptions::ADDR_Z_JOG_ACCEL_DISPLAY_STRING;
            dtostrf(SystemConfig::RuntimeConfig::Z_Axis::acceleration, 6, 1, hmiDisplayStringBuffer);
            if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
            {
                strncat(hmiDisplayStringBuffer, " mm/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            else
            {
                strncat(hmiDisplayStringBuffer, " in/s2", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Z Jog Accel display updated due to unit change: ");
            SerialDebug.println(hmiDisplayStringBuffer);

            // Z Backlash Compensation
            responsePacket.address = HmiSetupPageOptions::ADDR_Z_BACKLASH_COMP_DISPLAY_STRING;
            dtostrf(SystemConfig::RuntimeConfig::Z_Axis::backlash_compensation, 4, 2, hmiDisplayStringBuffer);
            if (SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
            {
                strncat(hmiDisplayStringBuffer, " mm", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            else
            {
                strncat(hmiDisplayStringBuffer, " in", sizeof(hmiDisplayStringBuffer) - strlen(hmiDisplayStringBuffer) - 1);
            }
            strncpy(responsePacket.data._string, hmiDisplayStringBuffer, MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
            SerialDebug.print("SetupHandler: Z Backlash Comp display updated due to unit change: ");
            SerialDebug.println(hmiDisplayStringBuffer);
        }
    }
    // --- Action Buttons ---
    else if (packet->address == HmiSetupPageOptions::ADDR_SAVE_ALL_PARAMS_PULSE)
    {
        if (packet->data._bool)
        { // Pulse to save
            SerialDebug.println("SetupHandler: Save All Parameters button pressed. Attempting to save all settings to EEPROM...");
            HAL_Delay(500); // User requested 500ms delay
            // HAL_FLASH_Unlock() and HAL_FLASH_Lock() are handled by SystemConfig::ConfigManager::saveAllSettings()
            if (SystemConfig::ConfigManager::saveAllSettings())
            {
                SerialDebug.println("SetupHandler: All settings saved successfully.");
            }
            else
            {
                SerialDebug.println("SetupHandler: ERROR saving settings to EEPROM!");
            }
        }
    }
    // else {
    // Optional: Log unhandled packets if this handler is exclusively for Setup Page
    // SerialDebug.print("SetupHandler: Unhandled packet address: "); SerialDebug.println(packet->address);
    // }
}
