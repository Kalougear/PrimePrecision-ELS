/**
 * @file HmiDebouncer.cpp
 * @brief Implementation of HMI button debouncing utility
 */

#include "UI/HmiDebouncer.h"

// Static member definitions
HmiDebouncer::ButtonHistory HmiDebouncer::_buttonHistory[MAX_TRACKED_BUTTONS];
uint8_t HmiDebouncer::_nextSlot = 0;

bool HmiDebouncer::shouldProcessButtonPress(uint16_t buttonAddress, uint32_t currentTimeMs, uint32_t debounceDelayMs)
{
    ButtonHistory *history = findOrCreateButtonHistory(buttonAddress);
    if (!history)
    {
        // If we can't track this button (all slots full), allow the press
        return true;
    }

    // Check if enough time has passed since the last press
    if (currentTimeMs - history->lastPressTimeMs >= debounceDelayMs)
    {
        history->lastPressTimeMs = currentTimeMs;
        return true; // Allow the button press
    }

    // Too soon since last press, ignore this one
    return false;
}

void HmiDebouncer::clearAll()
{
    for (uint8_t i = 0; i < MAX_TRACKED_BUTTONS; i++)
    {
        _buttonHistory[i].isActive = false;
        _buttonHistory[i].address = 0;
        _buttonHistory[i].lastPressTimeMs = 0;
    }
    _nextSlot = 0;
}

HmiDebouncer::ButtonHistory *HmiDebouncer::findOrCreateButtonHistory(uint16_t buttonAddress)
{
    // First, try to find existing history for this button
    for (uint8_t i = 0; i < MAX_TRACKED_BUTTONS; i++)
    {
        if (_buttonHistory[i].isActive && _buttonHistory[i].address == buttonAddress)
        {
            return &_buttonHistory[i];
        }
    }

    // Not found, try to create a new entry
    for (uint8_t i = 0; i < MAX_TRACKED_BUTTONS; i++)
    {
        if (!_buttonHistory[i].isActive)
        {
            _buttonHistory[i].isActive = true;
            _buttonHistory[i].address = buttonAddress;
            _buttonHistory[i].lastPressTimeMs = 0;
            return &_buttonHistory[i];
        }
    }

    // All slots are full, return nullptr
    return nullptr;
}
