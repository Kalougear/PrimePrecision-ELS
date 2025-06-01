#ifndef JOG_PAGE_HANDLER_H
#define JOG_PAGE_HANDLER_H

#include "LumenProtocol.h" // For lumen_packet_t

// Forward declarations
class MotionControl;
// No need to forward declare TurningMode here unless it's directly used by JogPageHandler's interface.

namespace JogPageHandler
{
    /**
     * @brief Initializes the JogPageHandler.
     * @param mcInstance Pointer to the global MotionControl instance.
     */
    void init(MotionControl *mcInstance);

    /**
     * @brief Called when the Jog Page is entered.
     * Sends initial HMI display values for the Jog Page (e.g., current jog speed).
     */
    void onEnterPage();

    /**
     * @brief Handles incoming Lumen packets when the Jog Page is active.
     * @param packet Pointer to the received lumen_packet_t.
     */
    void handlePacket(const lumen_packet_t *packet);

    /**
     * @brief Called when the Jog Page is exited.
     * Used to save any persistent settings from the Jog Page, like the last selected jog speed index.
     */
    void onExitPage();

    // Future: Add functions for updating specific Jog Page displays if needed,
    // similar to TurningPageHandler::updateDRO, e.g., for max jog speed display.

} // namespace JogPageHandler

#endif // JOG_PAGE_HANDLER_H
