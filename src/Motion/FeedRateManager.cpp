#include "Motion/FeedRateManager.h"
#include <Arduino.h> // For String class
#include <stdio.h>
#include <stdlib.h>

extern HardwareSerial SerialDebug; // Use SerialDebug directly

// Feed rate tables
const FeedRateManager::FeedRate FeedRateManager::metricFeedRates[] = {
    // Superfine Finishing
    {0.02, 20, 1000, "Superfine Finishing", false},
    {0.03, 30, 1000, "Superfine Finishing", false},
    {0.04, 40, 1000, "Superfine Finishing", false},

    // Fine Finishing
    {0.05, 50, 1000, "Fine Finishing", false},
    {0.06, 60, 1000, "Fine Finishing", false},
    {0.07, 70, 1000, "Fine Finishing", false},
    {0.08, 80, 1000, "Fine Finishing", false},
    {0.09, 90, 1000, "Fine Finishing", false},

    // Light Turning
    {0.10, 100, 1000, "Light Turning", false},
    {0.11, 110, 1000, "Light Turning", false},
    {0.12, 120, 1000, "Light Turning", false},
    {0.13, 130, 1000, "Light Turning", false},
    {0.14, 140, 1000, "Light Turning", false},

    // Medium Turning
    {0.15, 150, 1000, "Medium Turning", false},
    {0.16, 160, 1000, "Medium Turning", false},
    {0.18, 180, 1000, "Medium Turning", false},
    {0.20, 200, 1000, "Medium Turning", false},
    {0.22, 220, 1000, "Medium Turning", false},

    // Roughing
    {0.25, 250, 1000, "Roughing", false},
    {0.28, 280, 1000, "Roughing", false},
    {0.30, 300, 1000, "Roughing", false},
    {0.32, 320, 1000, "Roughing", true},
    {0.35, 350, 1000, "Roughing", true},
    {0.38, 380, 1000, "Roughing", true},
    {0.40, 400, 1000, "Roughing", true}};

const FeedRateManager::FeedRate FeedRateManager::imperialFeedRates[] = {
    // Superfine Finishing
    {0.0005, 5, 10000, "Superfine Finishing", false},
    {0.0008, 8, 10000, "Superfine Finishing", false},

    // Fine Finishing
    {0.0010, 10, 10000, "Fine Finishing", false},
    {0.0012, 12, 10000, "Fine Finishing", false},
    {0.0015, 15, 10000, "Fine Finishing", false},
    {0.0018, 18, 10000, "Fine Finishing", false},

    // Light Turning
    {0.0020, 20, 10000, "Light Turning", false},
    {0.0022, 22, 10000, "Light Turning", false},
    {0.0025, 25, 10000, "Light Turning", false},
    {0.0028, 28, 10000, "Light Turning", false},

    // Medium Turning
    {0.0030, 30, 10000, "Medium Turning", false},
    {0.0035, 35, 10000, "Medium Turning", false},
    {0.0040, 40, 10000, "Medium Turning", false},
    {0.0045, 45, 10000, "Medium Turning", false},

    // Roughing
    {0.0050, 50, 10000, "Roughing", false},
    {0.0055, 55, 10000, "Roughing", false},
    {0.0060, 60, 10000, "Roughing", false},
    {0.0070, 70, 10000, "Roughing", true},
    {0.0080, 80, 10000, "Roughing", true},
    {0.0090, 90, 10000, "Roughing", true},
    {0.0100, 100, 10000, "Roughing", true}};

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
    const char *unit = this->isMetric ? "mm/rev" : "inch/rev";
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
        snprintf(buffer, size, "%s %s",       // Output: "VALUE UNIT"
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
