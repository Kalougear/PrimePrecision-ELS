#pragma once

#include <Arduino.h>
#include "UI/DisplayComm.h"
#include "Motion/TurningMode.h"
#include "Motion/ThreadingMode.h"
#include "Motion/MotionControl.h"

// Menu state enumeration
enum class MenuState
{
    MAIN,
    TURNING,
    THREADING,
    SETUP
};

class MenuSystem
{
public:
    MenuSystem();
    ~MenuSystem();

    // Initialization
    bool begin(DisplayComm *display, MotionControl *motion_control);
    void end();

    // Menu navigation
    void showMainMenu();
    void showTurningMenu();
    void showThreadingMenu();
    void showSetupMenu();

    // Status updates
    void updateStatus();

    // Button handling
    void handleButtonPress(uint8_t button_id);

    // Get current state
    MenuState getCurrentState() const { return _currentState; }

    // Getters for internal components
    DisplayComm *getDisplayComm() const { return _display; }
    TurningMode *getTurningMode() const { return _turningMode; }
    ThreadingMode *getThreadingMode() const { return _threadingMode; } // Added getter for ThreadingMode
    MotionControl *getMotionControl() const { return _motionControl; } // Added for completeness

private:
    DisplayComm *_display;
    MotionControl *_motionControl;
    TurningMode *_turningMode;
    ThreadingMode *_threadingMode;

    MenuState _currentState;

    // Helper methods for turning screen
    void handleTurningButtons(uint8_t button_id);
    void updateTurningScreen();
    void cycleTurningFeedRate(bool increase);

    // Helper methods for threading screen
    void handleThreadingButtons(uint8_t button_id);
    void updateThreadingScreen();
    void cycleThreadPitch(bool increase);
    void toggleThreadUnits();
    void toggleMultiStart();

    // Helper methods for setup screen
    void handleSetupButtons(uint8_t button_id);
    void updateSetupScreen();

    // Static packet handler (was staticButtonHandler)
    static void staticPacketHandler(const lumen_packet_t *packet); // Signature changed
    static MenuSystem *s_instance;
};
