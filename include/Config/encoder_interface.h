#pragma once
#include "encoder_config.h"
#include <Arduino.h>

namespace EncoderConfig
{
    class Interface
    {
    public:
        // Update configuration from Nextion
        static bool updateConfig(const char *param, uint32_t value);

        // Read current configuration
        static uint32_t readConfig(const char *param);

        // Configuration management
        static bool saveConfig();
        static bool loadConfig();
        static void resetToDefaults();

        // Validation helpers
        static bool validatePPR(uint16_t value);
        static bool validateRPM(uint16_t value);
        static bool validateFilter(uint8_t value);
        static bool validateSyncRatio(uint16_t value);

    private:
        static SaveableConfig current_config;
        static uint32_t calculateChecksum(const SaveableConfig &config);
    };
}