#pragma once

#include <Arduino.h>

class Positioning {
public:
    // Backlash compensation type
    enum class BacklashMode {
        NONE,        // No backlash compensation
        AUTOMATIC,   // Automatic backlash compensation
        MANUAL       // Manual backlash settings
    };
    
    Positioning();
    ~Positioning() = default;
    
    // Configuration
    void setEndPosition(float position);
    void setStartPosition(float position);
    void setBacklashAmount(float amount);
    void setBacklashMode(BacklashMode mode);
    
    // Operations
    void reset();
    void update(float currentPosition);
    bool hasReachedEndPosition(float currentPosition) const;
    
    // Position data
    float getStartPosition() const { return _startPosition; }
    float getEndPosition() const { return _endPosition; }
    float getDistanceToEnd(float currentPosition) const;
    float getCompensatedPosition(float position, bool movingPositive) const;
    
private:
    float _startPosition;         // Start position in mm
    float _endPosition;           // End position in mm
    float _backlashAmount;        // Backlash amount in mm
    BacklashMode _backlashMode;   // Current backlash compensation mode
    bool _directionChanged;       // Direction change flag for backlash comp
    bool _lastMovingPositive;     // Last movement direction
};