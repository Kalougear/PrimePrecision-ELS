#ifndef JOG_PAGE_OPTIONS_H
#define JOG_PAGE_OPTIONS_H

#include <stdint.h>
#include "../../lib/Lumen_Protocol/src/c/LumenProtocol.h" // For lumen_packet_t and kDataType
#include "../SystemConfig.h"                              // For SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH

namespace HmiJogPageOptions
{
    // Jog Control HMI Addresses and Packets
    const uint16_t bool_jog_leftAddress = 185;  // Input: true=pressed, false=released
    const uint16_t bool_jog_rightAddress = 186; // Input: true=pressed, false=released
    // extern lumen_packet_t bool_jog_leftPacket; // Declaration if needed
    // extern lumen_packet_t bool_jog_rightPacket; // Declaration if needed

    const uint16_t int_prev_next_jog_speedAddress = 194; // Input: 0=None, 1=Prev, 2=Next
    extern lumen_packet_t int_prev_next_jog_speedPacket;

    const uint16_t string_display_jog_current_speed_valueAddress = 187; // Output: "speed_value units"
    extern lumen_packet_t string_display_jog_current_speed_valuePacket;
    extern char jog_speed_display_buffer[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH];

    const uint16_t bool_jog_system_enableAddress = 195; // Input: true = enable jog, false = disable jog
    extern lumen_packet_t bool_jog_system_enablePacket; // Optional: if we need to pre-define a packet struct

    // (Future: Add HMI address for Max Jog Speed configuration input if needed)
    // const uint16_t string_max_jog_speed_inputAddress = YYY;
    // extern lumen_packet_t string_max_jog_speed_inputPacket;

    // (Future: Add HMI address for Jog System Enable/Disable display/toggle if needed on this page)
    // const uint16_t bool_jog_system_enable_toggleAddress = ZZZ;
    // extern lumen_packet_t bool_jog_system_enable_togglePacket;

    // Jog Speed Selection Command Values from HMI (for int_prev_next_jog_speedAddress)
    enum JogSpeedCommandValue : int32_t
    {
        JOG_SPEED_CMD_NONE = 0,
        JOG_SPEED_CMD_PREV = 1,
        JOG_SPEED_CMD_NEXT = 2
    };

    // Predefined Jog Speeds (in mm/min) for the on-screen Prev/Next buttons
    // These are example values and can be adjusted.
    // The actual speed will be capped by a Max Jog Speed setting or SystemConfig::RuntimeConfig::Z_Axis::max_feed_rate_mm_per_min.
    // Reduced speeds for testing
    static const float JOG_SPEEDS_MM_PER_MIN[] = {
        30.0f,  // 0.5 mm/s
        60.0f,  // 1.0 mm/s
        120.0f, // 2.0 mm/s
        240.0f, // 4.0 mm/s
        300.0f  // 5.0 mm/s
    };
    static const int NUM_JOG_SPEEDS = sizeof(JOG_SPEEDS_MM_PER_MIN) / sizeof(JOG_SPEEDS_MM_PER_MIN[0]);

} // namespace HmiJogPageOptions

#endif // JOG_PAGE_OPTIONS_H
