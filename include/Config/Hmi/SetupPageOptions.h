#pragma once
#include <stdint.h>

namespace HmiSetupPageOptions // Renamed namespace for clarity
{
    // --- Cyclable Lists & Their Display/Trigger Addresses ---

    // Encoder PPR Options
    constexpr uint16_t pprList[] = {100, 200, 256, 360, 400, 500, 512, 600, 1000, 1024, 2000, 2048, 2500, 3600, 4096};
    constexpr uint8_t pprListSize = sizeof(pprList) / sizeof(pprList[0]);
    constexpr uint16_t ADDR_PPR_PULSE = 164;   // HMI sends pulse to cycle
    constexpr uint16_t ADDR_PPR_DISPLAY = 165; // STM32 sends current value string

    // Z-Axis Leadscrew Pitch Options
    // Metric list (direct pitch in mm)
    constexpr float zLeadscrewMetricPitchList[] = {1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f};
    constexpr uint8_t zLeadscrewMetricPitchListSize = sizeof(zLeadscrewMetricPitchList) / sizeof(zLeadscrewMetricPitchList[0]);

    // Imperial list (TPI values)
    constexpr float zLeadscrewImperialTpiList[] = {4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f};
    constexpr uint8_t zLeadscrewImperialTpiListSize = sizeof(zLeadscrewImperialTpiList) / sizeof(zLeadscrewImperialTpiList[0]);

    // HMI Addresses for Leadscrew Pitch
    constexpr uint16_t ADDR_LEADSCREW_PITCH_PULSE = 167;   // HMI sends pulse to cycle
    constexpr uint16_t ADDR_LEADSCREW_PITCH_DISPLAY = 166; // STM32 sends current value string (e.g., "2.0 mm" or "10 TPI")

    // Z-Axis Driver Microstepping Options
    constexpr uint32_t zDriverMicrosteppingList[] = {400, 800, 1600, 2000, 3200, 4000, 5000, 6400, 10000, 12800, 20000, 25600};
    constexpr uint8_t zDriverMicrosteppingListSize = sizeof(zDriverMicrosteppingList) / sizeof(zDriverMicrosteppingList[0]);
    constexpr uint16_t ADDR_MICROSTEP_PULSE = 168;   // HMI sends pulse to cycle
    constexpr uint16_t ADDR_MICROSTEP_DISPLAY = 169; // STM32 sends current value string

    // --- Direct Input & Toggle Addresses for Setup Tab ---

    // Column 1: General System & Spindle/Encoder
    constexpr uint16_t ADDR_MEASUREMENT_UNIT_DEFAULT_TOGGLE = 151; // bool: 0=Metric, 1=Imperial
    constexpr uint16_t ADDR_ELS_FEED_UNIT_DEFAULT_TOGGLE = 154;    // bool: 0=mm/rev, 1=in/rev
    constexpr uint16_t ADDR_SPINDLE_CHUCK_TEETH_STRING = 160;      // string: HMI sends value
    constexpr uint16_t ADDR_SPINDLE_ENCODER_TEETH_STRING = 162;    // string: HMI sends value

    // Column 2: Z Mechanical & Gearing, Z Motor & Driver
    constexpr uint16_t ADDR_Z_MOTOR_PULLEY_TEETH_STRING = 171;     // string: HMI sends value
    constexpr uint16_t ADDR_Z_LEADSCREW_PULLEY_TEETH_STRING = 172; // string: HMI sends value
    constexpr uint16_t ADDR_Z_INVERT_DIR_TOGGLE = 181;             // bool
    constexpr uint16_t ADDR_Z_MOTOR_ENABLE_POL_TOGGLE = 184;       // bool

    // Column 3: Z Performance & Tuning, Z Calibration & Info
    // Renamed original addresses to DISPLAY, new addresses for INPUT from HMI
    constexpr uint16_t ADDR_Z_MAX_JOG_SPEED_DISPLAY_STRING = 174; // string: STM32 sends value FOR DISPLAY
    constexpr uint16_t ADDR_Z_MAX_JOG_SPEED_INPUT_STRING = 188;   // string: HMI sends value TO STM32

    constexpr uint16_t ADDR_Z_JOG_ACCEL_DISPLAY_STRING = 177; // string: STM32 sends value FOR DISPLAY
    constexpr uint16_t ADDR_Z_JOG_ACCEL_INPUT_STRING = 189;   // string: HMI sends value TO STM32

    constexpr uint16_t ADDR_Z_BACKLASH_COMP_DISPLAY_STRING = 179; // string: STM32 sends value FOR DISPLAY
    constexpr uint16_t ADDR_Z_BACKLASH_COMP_INPUT_STRING = 190;   // string: HMI sends value TO STM32

    constexpr uint16_t ADDR_Z_LEADSCREW_STANDARD_TOGGLE = 157; // bool: 0=Metric, 1=Imperial

    // --- Action Buttons ---
    constexpr uint16_t ADDR_SAVE_ALL_PARAMS_PULSE = 180; // bool: HMI sends pulse to save

} // namespace HmiSetupPageOptions
