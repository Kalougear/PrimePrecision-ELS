#pragma once

#include <Arduino.h>
#include "Motion/MotionControl.h"
#include "Motion/Positioning.h"
// Forward declare ThreadingPageHandler to avoid circular dependency if full include is problematic
// class ThreadingPageHandler; // Not strictly needed here if we include its header in .cpp

class ThreadingMode
{
public:
    // Thread units
    enum class Units
    {
        METRIC,
        IMPERIAL
    };

    // Thread type
    enum class ThreadType
    {
        STANDARD,
        CUSTOM
    };

    // Thread data structure
    struct ThreadData
    {
        float pitch;     // Thread pitch in mm (metric) or TPI (imperial)
        uint8_t starts;  // Number of thread starts (default = 1)
        Units units;     // Thread units (metric/imperial)
        ThreadType type; // Standard or custom thread
        bool valid;      // Data validation flag
    };

    // Position configuration (same as TurningMode)
    struct Position
    {
        float start_position; // Starting Z position in mm
        float end_position;   // Ending Z position in mm
        bool valid;           // Position data valid flag
    };

    ThreadingMode();
    ~ThreadingMode();

    // Configuration
    void setThreadData(const ThreadData &thread_data);
    void setPositions(const Position &positions);
    void enableMultiStart(bool enable);

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
    float getCurrentPosition() const; // Will be updated to use offset
    float getEffectivePitch() const;

    // Getters
    const ThreadData &getThreadData() const { return _threadData; }

    // Z-Axis Zeroing
    void setZeroPosition();
    void resetZAxisZeroOffset(); // Call in activate() or onEnterPage()

    // Feed Direction Control
    void setFeedDirection(bool isTowardsChuck);
    bool isFeedDirectionTowardsChuck() const;

    // Update from HMI
    void updatePitchFromHmiSelection();

    // Mode activation/deactivation
    void activate();   // Call when entering Threading Tab if motor is on
    void deactivate(); // Call when leaving Threading Tab

private:
    MotionControl *_motionControl;
    int32_t _z_axis_zero_offset_steps;  // For Z-axis zeroing
    bool m_feedDirectionIsTowardsChuck; // Added for feed direction
    Positioning *_positioning;

    ThreadData _threadData;
    Position _positions;

    bool _running;
    bool _error;
    const char *_errorMsg;

    // Helper methods
    void configureThreading();
    float convertToMetricPitch(float imperial_tpi) const;
    void handleError(const char *msg);
};
