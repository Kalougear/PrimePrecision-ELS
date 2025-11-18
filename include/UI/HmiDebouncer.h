/**
 * @file HmiDebouncer.h
 * @brief Simple debouncing utility for HMI button inputs
 *
 * This utility prevents multiple rapid button presses from being processed,
 * which can happen when the HMI sends multiple packets for a single physical
 * button press or when users accidentally double-click.
 */

#ifndef HMI_DEBOUNCER_H
#define HMI_DEBOUNCER_H

#include <stdint.h>

class HmiDebouncer
{
public:
    /**
     * @brief Check if a button press should be processed based on debouncing rules
     * @param buttonAddress The HMI address of the button
     * @param currentTimeMs Current time in milliseconds (from millis())
     * @param debounceDelayMs Debounce delay in milliseconds (default: 100ms)
     * @return true if the button press should be processed, false if it should be ignored
     */
    static bool shouldProcessButtonPress(uint16_t buttonAddress, uint32_t currentTimeMs, uint32_t debounceDelayMs = 100);

    /**
     * @brief Clear all debounce history (useful for page changes)
     */
    static void clearAll();

private:
    // Maximum number of buttons we can track simultaneously
    static const uint8_t MAX_TRACKED_BUTTONS = 16;

    // Structure to track button press history
    struct ButtonHistory
    {
        uint16_t address;
        uint32_t lastPressTimeMs;
        bool isActive;
    };

    static ButtonHistory _buttonHistory[MAX_TRACKED_BUTTONS];
    static uint8_t _nextSlot;

    // Find existing button history or create new one
    static ButtonHistory *findOrCreateButtonHistory(uint16_t buttonAddress);
};

#endif // HMI_DEBOUNCER_H
