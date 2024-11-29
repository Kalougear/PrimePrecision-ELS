#pragma once
#include <Arduino.h>

namespace EncoderConfig
{
    // Configuration parameters
    struct RuntimeConfig
    {
        static uint16_t ppr;          // Pulses per revolution
        static uint16_t max_rpm;      // Maximum RPM
        static uint8_t filter_level;  // Digital filter level
        static bool invert_direction; // Direction inversion
        static bool enable_sync;      // Sync with spindle
        static uint16_t sync_ratio;   // Sync ratio (for threading)
    };

    // Configuration limits and defaults
    struct Limits
    {
        // PPR limits
        static constexpr uint16_t MIN_PPR = 100;
        static constexpr uint16_t MAX_PPR = 10000;
        static constexpr uint16_t DEFAULT_PPR = 1024;

        // RPM limits
        static constexpr uint16_t MIN_RPM = 60;
        static constexpr uint16_t MAX_RPM = 3000;
        static constexpr uint16_t DEFAULT_RPM = 2000;

        // Filter settings
        static constexpr uint8_t MIN_FILTER = 0;
        static constexpr uint8_t MAX_FILTER = 15;
        static constexpr uint8_t DEFAULT_FILTER = 8;

        // Sync ratio limits (for threading operations)
        static constexpr uint16_t MIN_SYNC_RATIO = 1;
        static constexpr uint16_t MAX_SYNC_RATIO = 1000;
        static constexpr uint16_t DEFAULT_SYNC_RATIO = 100;
    };

    // Parameter identifiers for Nextion communication
    struct Parameters
    {
        static const char *PPR;
        static const char *MAX_RPM;
        static const char *FILTER_LEVEL;
        static const char *INVERT_DIR;
        static const char *ENABLE_SYNC;
        static const char *SYNC_RATIO;
    };

    // Saveable configuration structure
    struct SaveableConfig
    {
        uint16_t ppr;
        uint16_t max_rpm;
        uint8_t filter_level;
        bool invert_direction;
        bool enable_sync;
        uint16_t sync_ratio;
        uint32_t checksum;
    };
}