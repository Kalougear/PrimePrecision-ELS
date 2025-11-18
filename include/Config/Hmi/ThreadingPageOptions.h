#pragma once

#include "LumenProtocol.h" // For lumen_packet_t and kDataTypes
#include <Arduino.h>       // For String (if used for category names directly)

namespace HmiThreadingPageOptions
{

    // HMI Addresses for Thread Category Selection
    const uint16_t bool_prev_butt_thread_catAddress = 199;
    const uint16_t bool_next_butt_thread_catAddress = 200;
    const uint16_t string_thread_catAddress = 203;

    // Lumen Packet Declarations (defined in .cpp file)
    extern lumen_packet_t bool_prev_butt_thread_catPacket;
    extern lumen_packet_t bool_next_butt_thread_catPacket;
    extern lumen_packet_t string_thread_catPacket;

    // HMI Addresses for Thread Pitch Selection
    const uint16_t string_thread_pitchAddress = 204;
    const uint16_t bool_prev_thread_pitchAddress = 205;
    const uint16_t bool_next_thread_pitchAddress = 206;

    // Lumen Packet Declarations for Pitch Selection (defined in .cpp file)
    extern lumen_packet_t string_thread_pitchPacket;
    extern lumen_packet_t bool_prev_thread_pitchPacket;
    extern lumen_packet_t bool_next_thread_pitchPacket;

    // Thread Categories
    // Note: The order here defines the cycling order
    const char *const THREAD_CATEGORIES[] = {
        "Metric (mm)",
        "Imperial (TPI)"};

    const size_t NUM_THREAD_CATEGORIES = sizeof(THREAD_CATEGORIES) / sizeof(THREAD_CATEGORIES[0]); // Should be 2

    // Default selected category index
    const uint8_t DEFAULT_THREAD_CATEGORY_INDEX = 0; // "Metric (mm)"

    // Z-Axis Auto-Stop Feature HMI Addresses (for Threading Tab)
    const uint16_t bool_auto_stop_enDisAddress = 211;
    const uint16_t string_set_stop_disp_value_to_stm32Address = 212;   // HMI keyboard input to STM32
    const uint16_t string_set_stop_disp_value_from_stm32Address = 213; // STM32 display to HMI
    const uint16_t bool_grab_zAddress = 214;                           // "Use Current Z" button

    // Lumen Packet Declarations for Auto-Stop (defined in .cpp file)
    extern lumen_packet_t bool_auto_stop_enDisPacket;
    extern lumen_packet_t string_set_stop_disp_value_to_stm32Packet;
    extern lumen_packet_t string_set_stop_disp_value_from_stm32Packet;
    extern lumen_packet_t bool_grab_zPacket;

} // namespace HmiThreadingPageOptions
