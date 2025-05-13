#pragma once
#include <Arduino.h>

namespace SystemConfig
{
    /**
     * @brief System limits and default values
     * These define the valid ranges and default values for configurable parameters
     */
    struct Limits
    {
        // EXISTING: Encoder limits and defaults
        struct Encoder
        {
            static constexpr uint16_t MIN_PPR = 100;
            static constexpr uint16_t MAX_PPR = 10000;
            static constexpr uint16_t DEFAULT_PPR = 1024;

            static constexpr uint16_t MIN_RPM = 0;
            static constexpr uint16_t MAX_RPM = 3000;
            static constexpr uint16_t DEFAULT_RPM = 2000;

            static constexpr uint8_t MIN_FILTER = 0;
            static constexpr uint8_t MAX_FILTER = 15;
            static constexpr uint8_t DEFAULT_FILTER = 8;

            // NEW: Added quadrature multiplier
            static constexpr uint16_t QUADRATURE_MULT = 4;
        };

        // EXISTING: Stepper limits and defaults
        struct Stepper
        {
            static constexpr uint32_t MIN_SPEED = 1;     // 100 Hz
            static constexpr uint32_t MAX_SPEED = 20000; // 20 kHz
            static constexpr uint32_t DEFAULT_SPEED = 1000;
            static constexpr uint32_t DEFAULT_MICROSTEPS = 8;
            static constexpr uint16_t STEPS_PER_REV = 200;

            // Timing parameters moved from TimingConfig
            static constexpr uint32_t CYCLE_TIME_US = 5;   // Base cycle time (μs)
            static constexpr uint32_t PULSE_WIDTH_US = 5;  // Step pulse width (μs)
            static constexpr uint32_t DIR_SETUP_US = 6;    // Direction setup time (μs)
            static constexpr uint32_t ENABLE_SETUP_US = 5; // Enable setup time (μs)
        };

        // EXISTING: Motion control limits and defaults
        struct Motion
        {
            static constexpr uint32_t MIN_SYNC_FREQ = 1000;
            static constexpr uint32_t MAX_SYNC_FREQ = 100000;
            static constexpr uint32_t DEFAULT_SYNC_FREQ = 50000;
            static constexpr float DEFAULT_THREAD_PITCH = 1.0f;
            static constexpr float DEFAULT_LEADSCREW_PITCH = 2.0f;
        };
    };

    /**
     * @brief Runtime configuration parameters
     * These values can be modified during operation
     */
    struct RuntimeConfig
    {
        // EXISTING: Encoder runtime parameters
        struct Encoder
        {
            static uint16_t ppr;          // Pulses per revolution
            static uint16_t max_rpm;      // Maximum RPM limit
            static uint8_t filter_level;  // Digital filter level
            static bool invert_direction; // Invert encoder direction
        };

        // EXISTING: Stepper runtime parameters
        struct Stepper
        {
            static uint32_t microsteps;   // Microstepping setting
            static uint32_t max_speed;    // Maximum speed (Hz)
            static bool invert_direction; // Invert stepper direction
            static bool invert_enable;    // Invert enable signal
        };

        // EXISTING: Motion control runtime parameters
        struct Motion
        {
            static float thread_pitch;      // Current thread pitch (mm)
            static float leadscrew_pitch;   // Leadscrew pitch (mm)
            static uint32_t sync_frequency; // Sync update rate (Hz)
            static bool sync_enabled;       // Synchronization enable flag
        };
    };

    // EXISTING: Nextion interface parameter names
    struct NextionParameters
    {
        static const char *PPR;
        static const char *MAX_RPM;
        static const char *FILTER_LEVEL;
        static const char *ENCODER_DIR;
        static const char *MICROSTEPS;
        static const char *MAX_SPEED;
        static const char *STEPPER_DIR;
        static const char *INVERT_ENABLE;
        static const char *THREAD_PITCH;
        static const char *SYNC_FREQ;
        static const char *SYNC_ENABLE;
    };

    // EXISTING: Configuration management class
    class ConfigManager
    {
    public:
        static bool updateConfig(const char *param, uint32_t value);
        static uint32_t readConfig(const char *param);
        static void resetToDefaults();

    private:
        static bool validateParameter(const char *param, uint32_t value);
    };
};