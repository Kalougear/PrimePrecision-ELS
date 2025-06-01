#include "Config/Hmi/ThreadingPageOptions.h"

namespace HmiThreadingPageOptions
{

    // Lumen Packet Definitions
    lumen_packet_t bool_prev_butt_thread_catPacket = {bool_prev_butt_thread_catAddress, kBool};
    lumen_packet_t bool_next_butt_thread_catPacket = {bool_next_butt_thread_catAddress, kBool};
    lumen_packet_t string_thread_catPacket = {string_thread_catAddress, kString};

    // Lumen Packet Definitions for Pitch Selection
    lumen_packet_t string_thread_pitchPacket = {string_thread_pitchAddress, kString};
    lumen_packet_t bool_prev_thread_pitchPacket = {bool_prev_thread_pitchAddress, kBool};
    lumen_packet_t bool_next_thread_pitchPacket = {bool_next_thread_pitchAddress, kBool};

} // namespace HmiThreadingPageOptions
