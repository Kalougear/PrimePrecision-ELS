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
            static constexpr uint8_t DEFAULT_FILTER = 10;         // Adjusted from 12 back towards 8
            static constexpr uint32_t MIN_RPM_DELTA_TIME_MS = 10; // Minimum time delta (ms) for reliable RPM calculation

            // NEW: Added quadrature multiplier
            static constexpr uint16_t QUADRATURE_MULT = 4;
        };

        // EXISTING: Stepper limits and defaults
        struct Stepper
        {
            static constexpr uint32_t MIN_SPEED = 1;     // 100 Hz
            static constexpr uint32_t MAX_SPEED = 20000; // 20 kHz
            static constexpr uint32_t DEFAULT_SPEED = 1000;
            static constexpr uint32_t DEFAULT_MICROSTEPS = 16; // Changed from 8 to match 3200 pulses/rev for 200 step motor
            static constexpr uint16_t STEPS_PER_REV = 200;

            // Timing parameters moved from TimingConfig
            static constexpr uint32_t CYCLE_TIME_US = 5;   // Base cycle time (μs)
            static constexpr uint32_t PULSE_WIDTH_US = 5;  // Step pulse width (μs)
            static constexpr uint32_t DIR_SETUP_US = 6;    // Direction setup time (μs)
            static constexpr uint32_t ENABLE_SETUP_US = 5; // Enable setup time (μs)
        };

        // NEW: General system limits and defaults
        struct General
        {
            static constexpr bool DEFAULT_MEASUREMENT_UNIT_IS_METRIC = true;   // true for MM, false for Inches (System-wide display/input default)
            static constexpr bool DEFAULT_ELS_FEED_RATE_UNIT_IS_METRIC = true; // true for MM/rev, false for Inches/rev (ELS operations default)
            static constexpr bool DEFAULT_JOG_SYSTEM_ENABLED = true;           // Jog system enabled by default
            static constexpr uint8_t DEFAULT_JOG_SPEED_INDEX = 0;              // Default index for the predefined jog speeds list
        };

        // NEW: Spindle/Encoder specific limits (complementary to Encoder struct)
        struct Spindle
        {
            static constexpr uint16_t DEFAULT_CHUCK_PULLEY_TEETH = 60;
            static constexpr uint16_t DEFAULT_ENCODER_PULLEY_TEETH = 60;
        };

        // EXISTING: Motion control limits and defaults
        struct Motion
        {
            static constexpr uint32_t MIN_SYNC_FREQ = 1000;
            static constexpr uint32_t MAX_SYNC_FREQ = 100000;
            static constexpr uint32_t DEFAULT_SYNC_FREQ = 50000;
            static constexpr float DEFAULT_THREAD_PITCH = 1.0f;
            // DEFAULT_LEADSCREW_PITCH will move to Z_Axis limits
        };

        // NEW: Z-Axis limits and defaults
        struct Z_Axis
        {
            static constexpr bool DEFAULT_INVERT_DIRECTION = false;
            static constexpr uint16_t DEFAULT_MOTOR_PULLEY_TEETH = 20;
            static constexpr uint16_t DEFAULT_LEAD_SCREW_PULLEY_TEETH = 40;
            static constexpr float DEFAULT_LEAD_SCREW_PITCH = 2.0f;            // mm per revolution of leadscrew
            static constexpr uint32_t DEFAULT_DRIVER_PULSES_PER_REV = 3200;    // Motor steps * microsteps for Z-axis motor
            static constexpr float DEFAULT_MAX_FEED_RATE = 1000.0f;            // mm/min for Z-axis rapids (general ELS)
            static constexpr float DEFAULT_MAX_JOG_SPEED_MM_PER_MIN = 600.0f;  // mm/min specifically for jogging
            static constexpr float DEFAULT_ACCELERATION = 10.0f;               // mm/s^2 for Z-axis (Drastically reduced for testing)
            static constexpr float DEFAULT_BACKLASH_COMPENSATION = 0.02f;      // mm for Z-axis
            static constexpr bool DEFAULT_LEADSCREW_STANDARD_IS_METRIC = true; // true for MM, false for Inches
            static constexpr bool DEFAULT_ENABLE_POLARITY_ACTIVE_HIGH = true;  // true for active high, false for active low
            static constexpr uint32_t DEFAULT_MIN_STEP_PULSE_US = 5;           // Minimum step pulse width in microseconds
            static constexpr uint16_t DEFAULT_DIR_SETUP_TIME_US = 6;           // Direction setup time in microseconds
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
            static float thread_pitch; // Current thread pitch (mm) for threading mode
            // leadscrew_pitch moved to Z_Axis struct
            static uint32_t sync_frequency; // Sync update rate (Hz)
            static bool sync_enabled;       // Synchronization enable flag
        };

        // NEW: Z-Axis runtime parameters
        struct Z_Axis
        {
            static volatile bool invert_direction; // Added volatile
            static uint16_t motor_pulley_teeth;
            static uint16_t lead_screw_pulley_teeth;
            static float lead_screw_pitch;            // Actual leadscrew pitch (e.g., 2.0 mm)
            static uint32_t driver_pulses_per_rev;    // For Z-axis motor
            static float max_feed_rate;               // Max rapid traverse speed for Z, e.g., mm/min (general ELS)
            static float max_jog_speed_mm_per_min;    // Max speed specifically for jogging
            static float acceleration;                // Z-axis acceleration, e.g., mm/s^2
            static float backlash_compensation;       // mm
            static bool leadscrew_standard_is_metric; // true for MM, false for Inches
            static bool enable_polarity_active_high;  // true for active high, false for active low
            static uint32_t min_step_pulse_us;        // Minimum step pulse width in microseconds
            static uint16_t dir_setup_time_us;        // Direction setup time in microseconds
        };

        // NEW: General system runtime parameters
        struct System
        {
            static bool measurement_unit_is_metric; // true for MM, false for Inches
            static bool els_default_feed_rate_unit_is_metric;
            static bool jog_system_enabled;         // Master enable/disable for jog functionality
            static uint8_t default_jog_speed_index; // Stores the last selected/default jog speed index
        };

        // NEW: Spindle/Encoder runtime parameters (complementary to Encoder struct)
        struct Spindle
        {
            static uint16_t chuck_pulley_teeth;
            static uint16_t encoder_pulley_teeth;
        };
    };

    /**
     * @brief Flags to track if a runtime config parameter has changed
     */
    struct RuntimeConfigDirtyFlags
    {
        struct Encoder
        {
            static bool ppr;
            static bool max_rpm;
            static bool filter_level;
            static bool invert_direction;
        };
        struct Stepper
        {
            static bool microsteps;
            static bool max_speed;
            static bool invert_direction;
            static bool invert_enable;
        };
        struct Motion
        {
            static bool thread_pitch;
            static bool sync_frequency;
            static bool sync_enabled;
        };
        struct Z_Axis
        {
            static bool invert_direction;
            static bool motor_pulley_teeth;
            static bool lead_screw_pulley_teeth;
            static bool lead_screw_pitch;
            static bool driver_pulses_per_rev;
            static bool max_feed_rate;
            static bool max_jog_speed_mm_per_min;
            static bool acceleration;
            static bool backlash_compensation;
            static bool leadscrew_standard_is_metric;
            static bool enable_polarity_active_high;
            // min_step_pulse_us and dir_setup_time_us are not included as they are considered fixed for now
        };
        struct System
        {
            static bool measurement_unit_is_metric;
            static bool els_default_feed_rate_unit_is_metric;
            static bool jog_system_enabled;
            static bool default_jog_speed_index;
        };
        struct Spindle
        {
            static bool chuck_pulley_teeth;
            static bool encoder_pulley_teeth;
        };
    };

    // HMI interface parameter names
    struct HmiParameters
    {
        static constexpr size_t MAX_HMI_STRING_LENGTH = 40; // Maximum length for HMI string data

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

        // NEW HMI Parameters
        static const char *ELS_FEED_UNIT;
        static const char *CHUCK_TEETH;
        static const char *ENCODER_TEETH;
        static const char *LS_STD_METRIC;  // Leadscrew Standard Metric/Inch
        static const char *Z_ENABLE_POL;   // Z-Axis Enable Polarity
        static const char *Z_MIN_STEP_US;  // Z-Axis Min STEP Pulse (us)
        static const char *Z_DIR_SETUP_US; // Z-Axis DIR Setup Time (us)
    };

    // EXISTING: Configuration management class
    class ConfigManager
    {
    public:
        static bool initialize();      // Initializes EEPROM and loads settings
        static bool loadAllSettings(); // Loads all settings from EEPROM
        static bool saveAllSettings(); // Saves all settings to EEPROM

        static bool updateConfig(const char *param, uint32_t value); // For individual HMI updates
        static uint32_t readConfig(const char *param);               // For individual HMI reads
        static void resetToDefaults();                               // Resets all RuntimeConfig to Limits defaults (and optionally saves)

    private:
        static bool validateParameter(const char *param, uint32_t value);
        // Future: Helper functions for type conversion and EEPROM R/W can be added here
        // e.g., static bool read_u16_from_eeprom(uint16_t virt_addr, uint16_t& data_out);
        // e.g., static bool write_u16_to_eeprom(uint16_t virt_addr, uint16_t data_in);
    };
};
