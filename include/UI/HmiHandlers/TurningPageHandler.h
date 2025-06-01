#pragma once

#include "LumenProtocol.h"        // For lumen_packet_t
#include "Motion/TurningMode.h"   // Explicitly include TurningMode definition
#include "Motion/MotionControl.h" // Added full include for MotionControl

// Forward declarations
class DisplayComm;
// class TurningMode; // No longer needed as forward declaration, full include above
class MenuSystem; // Forward declare MenuSystem if passing it, or use TurningMode directly
// class MotionControl; // No longer needed as forward declaration, full include above

namespace TurningPageHandler
{
    void init(TurningMode *turningModeInstance);        // Pass TurningMode instance
    void onEnterPage(TurningMode *turningModeInstance); // To set initial displays based on its state
    void onExitPage(TurningMode *turningModeInstance);  // Added for deactivation
    // void handlePacket(const lumen_packet_t *packet); // Old signature
    void handlePacket(const lumen_packet_t *packet, TurningMode *turningMode, DisplayComm *display, MotionControl *motionControl); // Kept MotionControl for other uses
    void updateDRO(DisplayComm *display, TurningMode *turningMode);                                                                // DRO specific, no RPM
    void flashCompleteMessage(DisplayComm *display, TurningMode *turningMode);                                                     // For auto-stop completion feedback
    void update(TurningMode *turningMode, DisplayComm *display, MotionControl *motionControl);                                     // Expects MotionControl for RPM

} // namespace TurningPageHandler
