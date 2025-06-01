#pragma once

#include <Arduino.h>
#include <math.h> // For fabs

namespace ThreadTable
{

    // Thread data structure
    struct ThreadData
    {
        const char *name; // Thread name/description (e.g., "1.25 mm" or "20 TPI")
        float pitch;      // Thread pitch in mm or TPI
        bool metric;      // true = metric (mm), false = imperial (TPI)
    };

    // --- New Simplified Metric Pitches ---
    namespace MetricPitches
    {
        static constexpr ThreadData Threads[] = {
            {"0.25 mm", 0.25f, true},
            {"0.3 mm", 0.3f, true},
            {"0.35 mm", 0.35f, true},
            {"0.4 mm", 0.4f, true},
            {"0.45 mm", 0.45f, true},
            {"0.5 mm", 0.5f, true},
            {"0.6 mm", 0.6f, true},
            {"0.7 mm", 0.7f, true},
            {"0.75 mm", 0.75f, true},
            {"0.8 mm", 0.8f, true},
            {"1.0 mm", 1.0f, true},
            {"1.25 mm", 1.25f, true},
            {"1.5 mm", 1.5f, true},
            {"1.75 mm", 1.75f, true},
            {"2.0 mm", 2.0f, true},
            {"2.5 mm", 2.5f, true},
            {"3.0 mm", 3.0f, true},
            {"3.5 mm", 3.5f, true},
            {"4.0 mm", 4.0f, true},
            {"4.5 mm", 4.5f, true},
            {"5.0 mm", 5.0f, true},
            {"5.5 mm", 5.5f, true},
            {"6.0 mm", 6.0f, true}};
        static constexpr size_t COUNT = sizeof(Threads) / sizeof(ThreadData);
    } // namespace MetricPitches

    // --- New Simplified Imperial Pitches (TPI) ---
    namespace ImperialPitches
    {
        static constexpr ThreadData Threads[] = {
            {"80 TPI", 80.0f, false},
            {"72 TPI", 72.0f, false},
            {"64 TPI", 64.0f, false},
            {"60 TPI", 60.0f, false}, // Added common ME
            {"56 TPI", 56.0f, false},
            {"48 TPI", 48.0f, false},
            {"44 TPI", 44.0f, false},
            {"40 TPI", 40.0f, false},
            {"36 TPI", 36.0f, false},
            {"32 TPI", 32.0f, false},
            {"28 TPI", 28.0f, false},
            {"26 TPI", 26.0f, false}, // Added common ME/BA
            {"24 TPI", 24.0f, false},
            {"20 TPI", 20.0f, false},
            {"19 TPI", 19.0f, false}, // Common BSF
            {"18 TPI", 18.0f, false},
            {"16 TPI", 16.0f, false},
            {"14 TPI", 14.0f, false},
            {"13 TPI", 13.0f, false},
            {"12 TPI", 12.0f, false},
            {"11 TPI", 11.0f, false},
            {"10 TPI", 10.0f, false},
            {"9 TPI", 9.0f, false},
            {"8 TPI", 8.0f, false},
            {"7 TPI", 7.0f, false}, // Larger BSW/UNC
            {"6 TPI", 6.0f, false}, // Larger BSW/UNC
            {"5 TPI", 5.0f, false}, // Larger BSW/UNC
            {"4 TPI", 4.0f, false}  // Larger BSW/UNC
        };
        static constexpr size_t COUNT = sizeof(Threads) / sizeof(ThreadData);
    } // namespace ImperialPitches

} // namespace ThreadTable
