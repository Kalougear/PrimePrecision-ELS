#pragma once

#include "config.h"
#include <Arduino.h>

namespace STM32Step
{
    // Structure to hold configuration data that can be saved/loaded
    struct SaveableConfig
    {
        uint32_t pulse_width;
        uint32_t microsteps;
        uint32_t max_speed;
        bool invert_direction;
        bool invert_enable;
        uint32_t checksum; // For future use with flash storage
    };

    class ConfigInterface
    {
    public:
        // Configuration parameter identifiers
        static const char *PARAM_PULSE_WIDTH;
        static const char *PARAM_MICROSTEPS;
        static const char *PARAM_MAX_SPEED;
        static const char *PARAM_INVERT_DIR;
        static const char *PARAM_INVERT_ENABLE;

        // Update configuration from Nextion
        static bool updateConfig(const char *param, uint32_t value);

        // Read current configuration value
        static uint32_t readConfig(const char *param);

        // Placeholder methods for future flash implementation
        static bool saveConfig();
        static bool loadConfig();

        // Reset to default values
        static void resetToDefaults();

    private:
        static SaveableConfig current_config;
    };

} // namespace STM32Step
