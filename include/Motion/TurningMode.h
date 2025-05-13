#pragma once

#include <Arduino.h>
#include "Motion/MotionControl.h"
#include "Motion/Positioning.h"

class TurningMode {
public:
    // Feed rate options
    enum class FeedRate {
        F0_01,  // 0.01mm per revolution
        F0_02,  // 0.02mm per revolution
        F0_05,  // 0.05mm per revolution
        F0_10,  // 0.10mm per revolution
        F0_20,  // 0.20mm per revolution
        F0_50,  // 0.50mm per revolution
        F1_00   // 1.00mm per revolution
    };

    // Mode options
    enum class Mode {
        MANUAL,     // Manual turning without auto stop
        SEMI_AUTO   // Semi-auto with automatic stop at end position
    };

    // Position configuration
    struct Position {
        float start_position;  // Starting Z position in mm
        float end_position;    // Ending Z position in mm
        bool valid;            // Position data valid flag
    };

    TurningMode();
    ~TurningMode();

    // Configuration
    void setFeedRate(FeedRate rate);
    void setMode(Mode mode);
    void setPositions(const Position& positions);
    
    // Operation
    bool begin(MotionControl* motion_control);
    void end();
    void start();
    void stop();
    void update();
    
    // Status
    bool isRunning() const { return _running; }
    bool hasError() const { return _error; }
    const char* getErrorMessage() const { return _errorMsg; }
    float getCurrentPosition() const;
    float getFeedRateValue() const;
    
    // Getters
    Mode getMode() const { return _mode; }
    FeedRate getFeedRate() const { return _feedRate; }

private:
    MotionControl* _motionControl;
    Positioning* _positioning;
    
    FeedRate _feedRate;
    Mode _mode;
    Position _positions;
    
    bool _running;
    bool _error;
    const char* _errorMsg;
    
    // Helper methods
    void configureFeedRate();
    void handleEndPosition();
    void handleError(const char* msg);
};