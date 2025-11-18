#pragma once

#include "LumenProtocol.h"
#include "Config/Hmi/ThreadingPageOptions.h"
#include "Motion/ThreadingMode.h" // Will be needed for more advanced thread selection later
#include "Motion/MotionControl.h" // Added for motor control
#include "UI/DisplayComm.h"       // For sending data to HMI
#include "Config/ThreadTable.h"   // For ThreadTable::ThreadData

class ThreadingPageHandler
{
public:
    // Static methods for interaction
    static void init(DisplayComm *displayComm, ThreadingMode *threadingMode, MotionControl *motionControl); // Added motionControl
    static void onEnterPage();
    static void onExitPage();
    static void handlePacket(const lumen_packet_t *packet);
    static void update();

    // Public getter for the selected pitch data
    static const ThreadTable::ThreadData &getSelectedPitchData();

private:
    // Static members for dependencies and state
    static DisplayComm *_displayComm;
    static ThreadingMode *_threadingMode;
    static MotionControl *_motionControl; // Added for motor control
    static uint32_t _lastDROUpdateTime;   // For timed DRO updates
    static uint8_t _currentCategoryIndex;
    static uint8_t _currentPitchIndex;
    static ThreadTable::ThreadData _selectedPitchData; // Stores the currently selected pitch details

    // New members for managing active pitch list based on category
    static const ThreadTable::ThreadData *_activePitchList;
    static size_t _activePitchListSize;

    // Static helper methods
    static void selectNextCategory();
    static void selectPreviousCategory();
    static void updateCategoryDisplay();

    static void selectNextPitch();
    static void selectPreviousPitch();
    static void updatePitchDisplay();
    static void loadPitchesForCurrentCategoryAndSetDefault();
    static void updateDRO(); // For Z-Position display
    static void updateAutoStopTargetDisplay();
    static void checkAndHandleAutoStopCompletionFlash();

    // Structure to manage timed flashing of HMI elements
    struct Flasher
    {
        bool active = false;
        uint32_t startTime = 0;
        uint8_t count = 0;
        const uint16_t address = 0;
        const char *message;
        const uint32_t onTime;
        const uint32_t offTime;

        Flasher(uint16_t addr, const char *msg, uint32_t on, uint32_t off)
            : address(addr), message(msg), onTime(on), offTime(off) {}

        void start()
        {
            active = true;
            startTime = millis();
            count = 0;
        }

        void stop()
        {
            active = false;
        }

        void update(); // Implementation in .cpp
    };

    static Flasher _autoStopCompletionFlasher;

    // Constructor and destructor are not needed for a static class
    ThreadingPageHandler() = delete;
    ~ThreadingPageHandler() = delete;
};
