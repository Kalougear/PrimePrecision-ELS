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

} // namespace HmiThreadingPageOptions
