#pragma once

#include "LumenProtocol.h"        // For lumen_packet_t
#include "Motion/TurningMode.h"   // Explicitly include TurningMode definition
#include "Motion/MotionControl.h" // Added full include for MotionControl

// Forward declarations
class DisplayComm;
// class TurningMode; // No longer needed as forward declaration, full include above
class MenuSystem; // Forward declare MenuSystem if passing it, or use TurningMode directly
// class MotionControl; // No longer needed as forward declaration, full include above

class TurningPageHandler
{
public:
    // Public Interface
    static void init(TurningMode *turningMode, DisplayComm *displayComm, MotionControl *motionControl);
    static void onEnterPage();
    static void onExitPage();
    static void handlePacket(const lumen_packet_t *packet);
    static void update();

private:
    // Dependencies
    static TurningMode *_turningMode;
    static DisplayComm *_displayComm;
    static MotionControl *_motionControl;

    // Helper Methods
    static void updateDRO();
    static void sendTurningPageFeedDisplays();
    static void flashCompleteMessage();

    // Make class non-instantiable
    TurningPageHandler() = delete;
    ~TurningPageHandler() = delete;
};
