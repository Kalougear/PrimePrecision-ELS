#pragma once

#include "LumenProtocol.h" // Include the full definition

namespace SetupPageHandler
{
    /**
     * @brief Initializes any necessary states or variables for the Setup Page handler.
     * Called once during system startup.
     */
    void init();

    /**
     * @brief Called when the Setup Page becomes active.
     * Responsible for sending the initial state of all HMI elements on the Setup Page.
     */
    void onEnterPage();

    /**
     * @brief Processes an incoming Lumen packet if it's relevant to the Setup Page.
     * @param packet Pointer to the received lumen_packet_t.
     */
    void handlePacket(const lumen_packet_t *packet);

} // namespace SetupPageHandler
