#include "Motion/Positioning.h"
#include "Config/serial_debug.h"

Positioning::Positioning()
    : _startPosition(0.0f),
      _endPosition(0.0f),
      _backlashAmount(0.0f),
      _backlashMode(BacklashMode::NONE),
      _directionChanged(false),
      _lastMovingPositive(true)
{
}

void Positioning::setEndPosition(float position)
{
    _endPosition = position;
}

void Positioning::setStartPosition(float position)
{
    _startPosition = position;
}

void Positioning::setBacklashAmount(float amount)
{
    _backlashAmount = amount;
}

void Positioning::setBacklashMode(BacklashMode mode)
{
    _backlashMode = mode;
}

void Positioning::reset()
{
    _directionChanged = false;
    _lastMovingPositive = true;
}

void Positioning::update(float currentPosition)
{
    static float lastPosition = currentPosition;
    
    // Detect movement direction
    bool movingPositive = (currentPosition > lastPosition);
    
    // Detect direction change
    if (movingPositive != _lastMovingPositive) {
        _directionChanged = true;
        _lastMovingPositive = movingPositive;
        
        if (_backlashMode == BacklashMode::AUTOMATIC) {
            SerialDebug.println("Direction changed, applying backlash compensation");
        }
    } else {
        _directionChanged = false;
    }
    
    lastPosition = currentPosition;
}

bool Positioning::hasReachedEndPosition(float currentPosition) const
{
    // When moving positive, check if we've reached or exceeded end position
    if (_lastMovingPositive && currentPosition >= _endPosition) {
        return true;
    }
    
    // When moving negative, check if we've reached or gone below end position
    if (!_lastMovingPositive && currentPosition <= _endPosition) {
        return true;
    }
    
    return false;
}

float Positioning::getDistanceToEnd(float currentPosition) const
{
    return _endPosition - currentPosition;
}

float Positioning::getCompensatedPosition(float position, bool movingPositive) const
{
    if (_backlashMode == BacklashMode::NONE || _backlashAmount <= 0.0f) {
        return position;
    }
    
    // Apply backlash compensation on direction change
    if (_directionChanged) {
        if (movingPositive) {
            return position + _backlashAmount;
        } else {
            return position - _backlashAmount;
        }
    }
    
    return position;
}