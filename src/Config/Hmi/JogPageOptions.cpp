#include "../../../include/Config/Hmi/JogPageOptions.h"

namespace HmiJogPageOptions
{

    // Define the extern packet variables
    lumen_packet_t int_prev_next_jog_speedPacket = {HmiJogPageOptions::int_prev_next_jog_speedAddress, kS32};
    lumen_packet_t string_display_jog_current_speed_valuePacket = {HmiJogPageOptions::string_display_jog_current_speed_valueAddress, kString};

    // Define the extern display buffer
    char jog_speed_display_buffer[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH];

    // Define the extern packet for Jog System Enable
    lumen_packet_t bool_jog_system_enablePacket = {HmiJogPageOptions::bool_jog_system_enableAddress, kBool};

    // (Future: Define extern packet for Max Jog Speed if added)
    // lumen_packet_t string_max_jog_speed_inputPacket = { HmiJogPageOptions::string_max_jog_speed_inputAddress, kString };

} // namespace HmiJogPageOptions
