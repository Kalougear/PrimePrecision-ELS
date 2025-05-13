#pragma once

#include <Arduino.h>

namespace ThreadTable {

    // Thread data structure
    struct ThreadData {
        const char* name;  // Thread name/description
        float pitch;       // Thread pitch in mm or TPI
        bool metric;       // true = metric (mm), false = imperial (TPI)
    };
    
    // Common metric thread pitches (mm)
    namespace Metric {
        static constexpr ThreadData Threads[] = {
            {"M1 x 0.25", 0.25f, true},
            {"M1.2 x 0.25", 0.25f, true},
            {"M1.6 x 0.35", 0.35f, true},
            {"M2 x 0.4", 0.4f, true},
            {"M2.5 x 0.45", 0.45f, true},
            {"M3 x 0.5", 0.5f, true},
            {"M4 x 0.7", 0.7f, true},
            {"M5 x 0.8", 0.8f, true},
            {"M6 x 1.0", 1.0f, true},
            {"M8 x 1.25", 1.25f, true},
            {"M10 x 1.5", 1.5f, true},
            {"M12 x 1.75", 1.75f, true},
            {"M14 x 2.0", 2.0f, true},
            {"M16 x 2.0", 2.0f, true},
            {"M20 x 2.5", 2.5f, true},
            {"M24 x 3.0", 3.0f, true},
            {"M30 x 3.5", 3.5f, true},
            {"M36 x 4.0", 4.0f, true},
            {"M42 x 4.5", 4.5f, true},
            {"M48 x 5.0", 5.0f, true},
            {"M56 x 5.5", 5.5f, true},
            {"M64 x 6.0", 6.0f, true}
        };
        
        static constexpr size_t COUNT = sizeof(Threads) / sizeof(ThreadData);
    }
    
    // Common imperial thread pitches (TPI - threads per inch)
    namespace Imperial {
        static constexpr ThreadData Threads[] = {
            {"UNC #1 (64 TPI)", 64.0f, false},
            {"UNC #2 (56 TPI)", 56.0f, false},
            {"UNC #4 (40 TPI)", 40.0f, false},
            {"UNC #6 (32 TPI)", 32.0f, false},
            {"UNC #8 (32 TPI)", 32.0f, false},
            {"UNC #10 (24 TPI)", 24.0f, false},
            {"UNC #12 (24 TPI)", 24.0f, false},
            {"UNC 1/4 (20 TPI)", 20.0f, false},
            {"UNC 5/16 (18 TPI)", 18.0f, false},
            {"UNC 3/8 (16 TPI)", 16.0f, false},
            {"UNC 7/16 (14 TPI)", 14.0f, false},
            {"UNC 1/2 (13 TPI)", 13.0f, false},
            {"UNC 9/16 (12 TPI)", 12.0f, false},
            {"UNC 5/8 (11 TPI)", 11.0f, false},
            {"UNC 3/4 (10 TPI)", 10.0f, false},
            {"UNC 7/8 (9 TPI)", 9.0f, false},
            {"UNC 1 (8 TPI)", 8.0f, false},
            {"UNF #0 (80 TPI)", 80.0f, false},
            {"UNF #1 (72 TPI)", 72.0f, false},
            {"UNF #2 (64 TPI)", 64.0f, false},
            {"UNF #3 (56 TPI)", 56.0f, false},
            {"UNF #4 (48 TPI)", 48.0f, false},
            {"UNF #5 (44 TPI)", 44.0f, false},
            {"UNF #6 (40 TPI)", 40.0f, false},
            {"UNF #8 (36 TPI)", 36.0f, false},
            {"UNF #10 (32 TPI)", 32.0f, false},
            {"UNF #12 (28 TPI)", 28.0f, false},
            {"UNF 1/4 (28 TPI)", 28.0f, false},
            {"UNF 5/16 (24 TPI)", 24.0f, false},
            {"UNF 3/8 (24 TPI)", 24.0f, false},
            {"UNF 7/16 (20 TPI)", 20.0f, false},
            {"UNF 1/2 (20 TPI)", 20.0f, false},
            {"UNF 9/16 (18 TPI)", 18.0f, false},
            {"UNF 5/8 (18 TPI)", 18.0f, false},
            {"UNF 3/4 (16 TPI)", 16.0f, false},
            {"UNF 7/8 (14 TPI)", 14.0f, false},
            {"UNF 1 (12 TPI)", 12.0f, false},
            {"Whitworth 1/8 (40 TPI)", 40.0f, false},
            {"Whitworth 1/4 (20 TPI)", 20.0f, false},
            {"Whitworth 3/8 (16 TPI)", 16.0f, false},
            {"Whitworth 1/2 (12 TPI)", 12.0f, false},
            {"Whitworth 5/8 (11 TPI)", 11.0f, false},
            {"Whitworth 3/4 (10 TPI)", 10.0f, false},
            {"Whitworth 7/8 (9 TPI)", 9.0f, false},
            {"Whitworth 1 (8 TPI)", 8.0f, false}
        };
        
        static constexpr size_t COUNT = sizeof(Threads) / sizeof(ThreadData);
    }
    
    // Utility functions
    inline const ThreadData* findMetricThread(float pitch) {
        for (size_t i = 0; i < Metric::COUNT; i++) {
            if (fabs(Metric::Threads[i].pitch - pitch) < 0.01f) {
                return &Metric::Threads[i];
            }
        }
        return nullptr;
    }
    
    inline const ThreadData* findImperialThread(float tpi) {
        for (size_t i = 0; i < Imperial::COUNT; i++) {
            if (fabs(Imperial::Threads[i].pitch - tpi) < 0.1f) {
                return &Imperial::Threads[i];
            }
        }
        return nullptr;
    }

} // namespace ThreadTable