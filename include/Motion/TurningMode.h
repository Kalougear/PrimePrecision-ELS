#pragma once

#include <Arduino.h>
#include "Motion/MotionControl.h"
#include "Motion/Positioning.h"
#include "Motion/FeedRateManager.h" // Added include

class TurningMode
{
public:
    // Mode options
    enum class Mode
    {
        MANUAL,   // Manual turning without auto stop
        SEMI_AUTO // Semi-auto with automatic stop at end position
    };

    // Position configuration
    struct Position
    {
        float start_position; // Starting Z position in mm
        float end_position;   // Ending Z position in mm
        bool valid;           // Position data valid flag
    };

    TurningMode();
    ~TurningMode();

    // Configuration
    void selectNextFeedRate();
    void selectPreviousFeedRate();
    void setFeedRateMetric(bool isMetric);
    void setMode(Mode mode);
    void setPositions(const Position &positions);

    // Operation
    bool begin(MotionControl *motion_control);
    void end();
    void start();
    void stop();
    void update();

    // Status
    bool isRunning() const { return _running; }
    bool hasError() const { return _error; }
    const char *getErrorMessage() const { return _errorMsg; }
    float getCurrentPosition() const;
    float getFeedRateValue() const;
    bool isMotorEnabled() const; // Added for HMI motor enable/disable

    // Getters
    Mode getMode() const { return _mode; }
    // FeedRate getFeedRate() const; // Removed, use FeedRateManager directly or specific getters
    bool getCurrentFeedRateWarning() const;    // Check if current feed rate has a warning
    bool getFeedRateIsMetric() const;          // Added to check current unit system of feedrate
    const char *getFeedRateCategory() const;   // Added to get current feed rate category string
    FeedRateManager &getFeedRateManager();     // Added getter for the internal FeedRateManager
    bool getFeedDirectionTowardsChuck() const; // Getter for current feed direction
    void setZeroPosition();                    // New method to set current Z as zero
    void requestMotorEnable();                 // Added for HMI motor enable
    void requestMotorDisable();                // Added for HMI motor disable
    void setFeedDirection(bool towardsChuck);  // Setter for feed direction
    void configureFeedRate();                  // Moved to public for PageHandler access

    // --- Z-Axis Auto-Stop Feature ---
    void resetAutoStopRuntimeSettings();                              // Clears auto-stop state
    void setUiAutoStopEnabled(bool enabled);                          // Enables/disables the feature via UI
    bool isUiAutoStopEnabled() const;                                 // Checks if UI has enabled auto-stop
    void setUiAutoStopTargetPositionFromString(const char *valueStr); // Sets target from HMI string
    void grabCurrentZAsUiAutoStopTarget();                            // Sets current Z as target
    String getFormattedUiAutoStopTarget() const;                      // Gets target as formatted string for HMI
    bool checkAndHandleAutoStopCompletion();                          // Called in update() to check if MC stopped at target
    bool isAutoStopCompletionPendingHmiSignal() const;                // Check if HMI signal is pending
    void clearAutoStopCompletionHmiSignal();                          // Clear the HMI signal flag

    // Mode activation/deactivation
    void activate();   // Call when entering Turning Tab
    void deactivate(); // Call when leaving Turning Tab

private:
    MotionControl *_motionControl;
    Positioning *_positioning;
    FeedRateManager _feedRateManager; // Added FeedRateManager instance

    // FeedRate _feedRate; // Removed, managed by _feedRateManager
    Mode _mode;
    Position _positions;
    int32_t _z_axis_zero_offset_steps; // For Z-axis zeroing
    bool _feedDirectionTowardsChuck;   // True if feeding towards chuck, false if away (default)

    // Z-Axis Auto-Stop State (UI-level)
    bool _ui_autoStopEnabled;            // Reflects the HMI toggle for this mode's auto-stop
    int32_t _ui_targetStopAbsoluteSteps; // Target stop position in absolute machine steps
    bool _ui_targetStopIsSet;            // True if a target has been set by the UI

    bool _running;
    bool _error;
    const char *_errorMsg;

    // Auto-stop HMI signaling
    bool _autoStopCompletionPendingHmiSignal;

    // Helper methods
    // void configureFeedRate(); // Moved to public
    void handleEndPosition();
    void handleError(const char *msg);
};
