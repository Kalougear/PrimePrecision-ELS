#ifndef TURNING_PAGE_OPTIONS_H
#define TURNING_PAGE_OPTIONS_H

#include <stdint.h>
#include "../../lib/Lumen_Protocol/src/c/LumenProtocol.h" // For lumen_packet_t and kDataType
#include "../SystemConfig.h"                              // For SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH

namespace HmiTurningPageOptions
{
    // Jog-related definitions have been moved to HmiJogPageOptions.h

    // Placeholder for any Turning Page specific HMI options if needed in the future.
    // For now, this file might become empty or be removed if all specific options
    // are handled by HmiInputOptions.h or other dedicated files.

    // Z-Axis Auto-Stop Feature HMI Addresses (for Turning Tab)
    const uint16_t bool_auto_stop_enDisAddress = 145;
    const uint16_t string_set_stop_disp_value_to_stm32Address = 148;   // HMI keyboard input to STM32
    const uint16_t string_set_stop_disp_value_from_stm32Address = 197; // STM32 display to HMI
    const uint16_t bool_grab_zAddress = 198;                           // "Use Current Z" button

    // Note: The "Jog Setup Tab selector number = 5" (now PAGE_JOG) implies that the main tab selector
    // (int_tab_selectionAddress = 136, currently in SetupPageOptions.h or a global HMI config)
    // will send '5' for the dedicated Jog Page.
    // Similarly, ensure the Turning Tab has a unique ID sent by this main tab selector.

} // namespace HmiTurningPageOptions

#endif // TURNING_PAGE_OPTIONS_H
