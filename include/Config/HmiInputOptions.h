#pragma once
#include <stdint.h>

namespace HmiInputOptions
{
    // Encoder PPR Options
    // Comprehensive list as discussed
    constexpr uint16_t pprList[] = {100, 200, 256, 360, 400, 500, 512, 600, 1000, 1024, 2000, 2048, 2500, 3600, 4096};
    constexpr uint8_t pprListSize = sizeof(pprList) / sizeof(pprList[0]);
    // HMI Addresses for Encoder PPR
    constexpr uint16_t HMI_ADDR_PPR_PULSE = 164;   // Original: bool_ppr_pulse_for_stm32Address
    constexpr uint16_t HMI_ADDR_PPR_DISPLAY = 165; // Original: string_ppr_value_list_receive_from_stm32Address

    // Z-Axis Leadscrew Pitch Options
    // From main.cpp
    constexpr float zLeadscrewPitchList[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f};
    constexpr uint8_t zLeadscrewPitchListSize = sizeof(zLeadscrewPitchList) / sizeof(zLeadscrewPitchList[0]);
    // HMI Addresses for Z-Axis Leadscrew Pitch
    constexpr uint16_t HMI_ADDR_LEADSCREW_PITCH_PULSE = 167;   // Original: bool_z_leadscrew_pitch_pulse_for_stm32Address
    constexpr uint16_t HMI_ADDR_LEADSCREW_PITCH_DISPLAY = 166; // Original: string_z_leadscrew_pitch_list_reseave_from_stm32Address

    // Z-Axis Driver Microstepping Options
    // From main.cpp
    constexpr uint32_t zDriverMicrosteppingList[] = {400, 800, 1600, 2000, 3200, 4000, 5000, 6400, 10000, 12800, 20000, 25600};
    constexpr uint8_t zDriverMicrosteppingListSize = sizeof(zDriverMicrosteppingList) / sizeof(zDriverMicrosteppingList[0]);
    // HMI Addresses for Z-Axis Driver Microstepping
    constexpr uint16_t HMI_ADDR_MICROSTEP_PULSE = 168;   // Original: bool_z_driver_microstepping_pulse_for_stm32Address
    constexpr uint16_t HMI_ADDR_MICROSTEP_DISPLAY = 169; // Original: string_driver_microsteping_value_receive_from_stm32Address

    // Turning Tab mm/inch Selector Addresses
    // User presses button on Turning Tab, HMI sends kS32 (or kBool) to MCU on this address
    constexpr uint16_t ADDR_TURNING_MM_INCH_INPUT_FROM_HMI = 124; // bool_mmInch_Selection_To_Stm32Address
    // MCU sends kBool to this address to update the visual state of the mm/inch button on Turning Tab
    constexpr uint16_t ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI = 191; // bool_mmInch_Selection_From_Stm32Address

    // Turning Tab Prev/Next Feed Rate Button
    // HMI sends S32 (1 for prev, 2 for next) to MCU on this address
    constexpr uint16_t ADDR_TURNING_PREV_NEXT_BUTTON = 133; // Matches prevNEXTFEEDRATEVALUEAddress from main.cpp

    // Turning Tab Feed Rate Display (Value and Description/Category)
    constexpr uint16_t ADDR_TURNING_FEED_RATE_VALUE_DISPLAY = 132; // Matches actualFEEDRATEAddress from main.cpp
    constexpr uint16_t ADDR_TURNING_FEED_RATE_DESC_DISPLAY = 134;  // Matches actualFEEDRATEDESCRIPTIONAddress from main.cpp

    // Turning Tab Motor Enable/Disable Button
    // HMI sends kBool (true for enable request, false for disable request) to MCU.
    // MCU also sends kBool back to this address to update visual state.
    constexpr uint16_t ADDR_TURNING_MOTOR_ENABLE_TOGGLE = 149; // User provided: bool_motor_enDisAddress

    // Turning Tab Feed Direction Select Button (e.g., Towards/Away from Chuck)
    // HMI sends kBool (e.g., false for Away/Normal state, true for Towards/Reversed state).
    // MCU also sends kBool back to this address to update visual state.
    constexpr uint16_t ADDR_TURNING_FEED_DIRECTION_SELECT = 129; // User provided: bool_dir_selectionAddress, runtime log shows kBool

} // namespace HmiInputOptions
