#ifndef FEED_RATE_MANAGER_H
#define FEED_RATE_MANAGER_H

#include <stdint.h>
#include <string>

class FeedRateManager
{
private:
    struct FeedRate
    {
        double value;         // Display value
        int32_t numerator;    // For ratio calculation
        int32_t denominator;  // For ratio calculation
        const char *category; // Category name
        bool warning;         // True if this is a cautious feed rate
    };

    // Feed rate tables
    static const FeedRate metricFeedRates[];
    static const FeedRate imperialFeedRates[];
    static const size_t metricFeedRatesCount;
    static const size_t imperialFeedRatesCount;

    // Current state
    bool isMetric;       // true = mm/rev, false = inch/rev
    size_t currentIndex; // Current position in feed rate array

    // Get current feed rate table
    const FeedRate *getCurrentTable() const;
    size_t getCurrentTableSize() const;

    // Helper to find index for a default value
    static size_t findFeedRateIndex(const FeedRate *table, size_t count, double targetValue);

public:
    FeedRateManager();

    // Feed rate navigation
    void handlePrevNextValue(int32_t value); // Handle 0=none, 1=prev, 2=next
    void setMetric(bool metric);             // Set unit system (true=metric)

    // Current feed rate access
    double getCurrentValue() const;                         // Get current feed rate value
    const char *getCurrentCategory() const;                 // Get current category name
    void getCurrentRatio(int32_t &num, int32_t &den) const; // Get ratio for calculations
    bool getCurrentWarning() const;                         // Get warning status of current feed rate
    bool getIsMetric() const { return isMetric; }           // Get current unit system

    // Display formatting
    void getDisplayString(char *buffer, size_t size) const; // Get formatted display string
};

#endif // FEED_RATE_MANAGER_H
