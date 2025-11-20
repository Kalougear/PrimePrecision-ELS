#include "Motion/FeedRateManager.h"
#include <Arduino.h> // For String class
#include <stdio.h>
#include <stdlib.h>

extern HardwareSerial SerialDebug; // Use SerialDebug directly

// Feed rate tables
const FeedRateManager::FeedRate FeedRateManager::metricFeedRates[] = {
    {0.02, 20, 1000, "Polishing / Ultra-Fine", false},
    {0.05, 50, 1000, "Fine Finish", false},
    {0.08, 80, 1000, "Standard Finish", false},
    {0.10, 100, 1000, "General Turning", false},
    {0.15, 150, 1000, "Semi-Roughing", false},
    {0.20, 200, 1000, "Roughing (Light)", false},
    {0.25, 250, 1000, "Roughing (Medium)", false},
    {0.30, 300, 1000, "Roughing (Heavy)", true},
    {0.40, 400, 1000, "Roughing (Heavy)", true},
    {0.50, 500, 1000, "Roughing (Heavy)", true}};

const FeedRateManager::FeedRate FeedRateManager::imperialFeedRates[] = {
    {0.0010, 10, 10000, "Polishing", false},
    {0.0020, 20, 10000, "Fine Finish", false},
    {0.0030, 30, 10000, "Standard Finish", false},
    {0.0040, 40, 10000, "General Turning", false},
    {0.0060, 60, 10000, "Productivity", false},
    {0.0080, 80, 10000, "Roughing", true},
    {0.0100, 100, 10000, "Heavy Roughing", true},
    {0.0120, 120, 10000, "Max Removal", true}};

const size_t FeedRateManager::metricFeedRatesCount = sizeof(metricFeedRates) / sizeof(metricFeedRates[0]);
const size_t FeedRateManager::imperialFeedRatesCount = sizeof(imperialFeedRates) / sizeof(imperialFeedRates[0]);

// Helper function to find index of a value in a FeedRate table
size_t FeedRateManager::findFeedRateIndex(const FeedRate *table, size_t count, double targetValue) // Changed FeedRateManager::FeedRate to FeedRate
{
    for (size_t i = 0; i < count; ++i)
    {
        // Compare doubles with a small tolerance, or assume exact match for literals
        if (table[i].value == targetValue) // Direct comparison for known literals
        {
            return i;
        }
    }
    return 0; // Fallback to first index if not found
}

FeedRateManager::FeedRateManager() : isMetric(true), currentIndex(0)
{
    // Set default metric feed rate
    this->currentIndex = findFeedRateIndex(metricFeedRates, metricFeedRatesCount, 0.10);
    // SerialDebug.print("FeedRateManager Constructor: Initial isMetric=true, currentIndex set to: "); // Removed: Too early for SerialDebug
    // SerialDebug.println(this->currentIndex); // Removed: Too early for SerialDebug
}

void FeedRateManager::handlePrevNextValue(int32_t value)
{
    if (value == 0)
        return; // No button pressed

    size_t tableSize = getCurrentTableSize();
    if (tableSize == 0)
        return;

    if (value == 1)
    { // Previous
        if (currentIndex == 0)
        {
            currentIndex = tableSize - 1;
        }
        else
        {
            currentIndex--;
        }
    }
    else if (value == 2)
    { // Next
        currentIndex++;
        if (currentIndex >= tableSize)
        {
            currentIndex = 0;
        }
    }
}

void FeedRateManager::setMetric(bool metric_param) // Renamed to avoid confusion with member
{
    // SerialDebug.print("FeedRateManager::setMetric called with: "); // Removed
    // SerialDebug.println(metric_param ? "true (metric)" : "false (imperial)"); // Removed
    // SerialDebug.print("  Current this->isMetric before change: "); // Removed
    // SerialDebug.println(this->isMetric ? "true (metric)" : "false (imperial)"); // Removed

    if (this->isMetric != metric_param || metric_param != this->isMetric) // Ensure change or re-application of default
    {
        this->isMetric = metric_param;
        if (this->isMetric)
        {
            this->currentIndex = findFeedRateIndex(metricFeedRates, metricFeedRatesCount, 0.10);
            // SerialDebug.println("  Switched to METRIC, default 0.10 mm/rev selected."); // Removed
        }
        else
        {
            this->currentIndex = findFeedRateIndex(imperialFeedRates, imperialFeedRatesCount, 0.0020);
            // SerialDebug.println("  Switched to IMPERIAL, default 0.0020 inch/rev selected."); // Removed
        }
        // SerialDebug.print("  New currentIndex: "); // Removed
        // SerialDebug.println(this->currentIndex); // Removed
    }
    // else // Removed
    // { // Removed
    // SerialDebug.println("  this->isMetric NOT changed (already same value)."); // Removed
    // } // Removed
    // SerialDebug.print("  Current this->isMetric after logic: "); // Removed
    // SerialDebug.println(this->isMetric ? "true (metric)" : "false (imperial)"); // Removed
}

double FeedRateManager::getCurrentValue() const
{
    const FeedRate *table = getCurrentTable();
    if (!table || currentIndex >= getCurrentTableSize())
        return 0.0;
    return table[currentIndex].value;
}

const char *FeedRateManager::getCurrentCategory() const
{
    const FeedRate *table = getCurrentTable();
    if (!table || currentIndex >= getCurrentTableSize())
        return "";
    return table[currentIndex].category;
}

void FeedRateManager::getCurrentRatio(int32_t &num, int32_t &den) const
{
    const FeedRate *table = getCurrentTable();
    if (!table || currentIndex >= getCurrentTableSize())
    {
        num = 0;
        den = 1;
        return;
    }
    num = table[currentIndex].numerator;
    den = table[currentIndex].denominator;
}

bool FeedRateManager::getCurrentWarning() const
{
    const FeedRate *table = getCurrentTable();
    if (!table || currentIndex >= getCurrentTableSize())
        return false; // Default to no warning if out of bounds
    return table[currentIndex].warning;
}

void FeedRateManager::getDisplayString(char *buffer, size_t size) const
{
    const FeedRate *table = getCurrentTable();
    if (!table || currentIndex >= getCurrentTableSize())
    {
        snprintf(buffer, size, "Select Feed Rate");
        return;
    }

    const FeedRate &current = table[currentIndex];
    // SerialDebug.print("FeedRateManager::getDisplayString - current this->isMetric: "); // Removed
    // SerialDebug.println(this->isMetric ? "true (metric)" : "false (imperial)"); // Removed
    const char *unit = this->isMetric ? "mm/rev" : "in/rev";
    String float_str; // Arduino String object

    // Format with appropriate precision based on unit system
    if (isMetric)
    {
        float_str = String(current.value, 2); // Display all metric values with 2 decimal places
        snprintf(buffer, size, "%s %s",       // Output: "VALUE UNIT"
                 float_str.c_str(), unit);
    }
    else // Imperial
    {
        float_str = String(current.value, 4); // Display with 4 decimal places
        // Trim trailing zeros
        while (float_str.length() > 1 &&
               float_str.charAt(float_str.length() - 1) == '0' &&
               float_str.charAt(float_str.length() - 2) != '.')
        {
            float_str.remove(float_str.length() - 1);
        }
        snprintf(buffer, size, "%s %s", // Output: "VALUE UNIT"
                 float_str.c_str(), unit);
    }
}

const FeedRateManager::FeedRate *FeedRateManager::getCurrentTable() const
{
    return isMetric ? metricFeedRates : imperialFeedRates;
}

size_t FeedRateManager::getCurrentTableSize() const
{
    return isMetric ? metricFeedRatesCount : imperialFeedRatesCount;
}
