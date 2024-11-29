#include "Config/encoder_interface.h"
#include "Config/serial_debug.h"

namespace EncoderConfig
{
    // Initialize static members
    SaveableConfig Interface::current_config;

    // Initialize parameter identifiers
    const char *Parameters::PPR = "enc_ppr";
    const char *Parameters::MAX_RPM = "enc_rpm";
    const char *Parameters::FILTER_LEVEL = "enc_filter";
    const char *Parameters::INVERT_DIR = "enc_invert";
    const char *Parameters::ENABLE_SYNC = "enc_sync";
    const char *Parameters::SYNC_RATIO = "enc_ratio";

    // Initialize runtime configuration
    uint16_t RuntimeConfig::ppr = Limits::DEFAULT_PPR;
    uint16_t RuntimeConfig::max_rpm = Limits::DEFAULT_RPM;
    uint8_t RuntimeConfig::filter_level = Limits::DEFAULT_FILTER;
    bool RuntimeConfig::invert_direction = false;
    bool RuntimeConfig::enable_sync = false;
    uint16_t RuntimeConfig::sync_ratio = Limits::DEFAULT_SYNC_RATIO;

    bool Interface::updateConfig(const char *param, uint32_t value)
    {
        SerialDebug.print(F("Updating encoder config: "));
        SerialDebug.print(param);
        SerialDebug.print(F(" = "));
        SerialDebug.println(value);

        if (strcmp(param, Parameters::PPR) == 0)
        {
            if (validatePPR(value))
            {
                RuntimeConfig::ppr = value;
                current_config.ppr = value;
                return true;
            }
        }
        else if (strcmp(param, Parameters::MAX_RPM) == 0)
        {
            if (validateRPM(value))
            {
                RuntimeConfig::max_rpm = value;
                current_config.max_rpm = value;
                return true;
            }
        }
        else if (strcmp(param, Parameters::FILTER_LEVEL) == 0)
        {
            if (validateFilter(value))
            {
                RuntimeConfig::filter_level = value;
                current_config.filter_level = value;
                return true;
            }
        }
        else if (strcmp(param, Parameters::INVERT_DIR) == 0)
        {
            RuntimeConfig::invert_direction = value != 0;
            current_config.invert_direction = value != 0;
            return true;
        }
        else if (strcmp(param, Parameters::ENABLE_SYNC) == 0)
        {
            RuntimeConfig::enable_sync = value != 0;
            current_config.enable_sync = value != 0;
            return true;
        }
        else if (strcmp(param, Parameters::SYNC_RATIO) == 0)
        {
            if (validateSyncRatio(value))
            {
                RuntimeConfig::sync_ratio = value;
                current_config.sync_ratio = value;
                return true;
            }
        }

        SerialDebug.println(F("Invalid encoder parameter or value"));
        return false;
    }

    uint32_t Interface::readConfig(const char *param)
    {
        if (strcmp(param, Parameters::PPR) == 0)
        {
            return RuntimeConfig::ppr;
        }
        else if (strcmp(param, Parameters::MAX_RPM) == 0)
        {
            return RuntimeConfig::max_rpm;
        }
        else if (strcmp(param, Parameters::FILTER_LEVEL) == 0)
        {
            return RuntimeConfig::filter_level;
        }
        else if (strcmp(param, Parameters::INVERT_DIR) == 0)
        {
            return RuntimeConfig::invert_direction ? 1 : 0;
        }
        else if (strcmp(param, Parameters::ENABLE_SYNC) == 0)
        {
            return RuntimeConfig::enable_sync ? 1 : 0;
        }
        else if (strcmp(param, Parameters::SYNC_RATIO) == 0)
        {
            return RuntimeConfig::sync_ratio;
        }
        return 0;
    }

    bool Interface::validatePPR(uint16_t value)
    {
        return (value >= Limits::MIN_PPR && value <= Limits::MAX_PPR);
    }

    bool Interface::validateRPM(uint16_t value)
    {
        return (value >= Limits::MIN_RPM && value <= Limits::MAX_RPM);
    }

    bool Interface::validateFilter(uint8_t value)
    {
        return (value >= Limits::MIN_FILTER && value <= Limits::MAX_FILTER);
    }

    bool Interface::validateSyncRatio(uint16_t value)
    {
        return (value >= Limits::MIN_SYNC_RATIO && value <= Limits::MAX_SYNC_RATIO);
    }

    void Interface::resetToDefaults()
    {
        RuntimeConfig::ppr = Limits::DEFAULT_PPR;
        RuntimeConfig::max_rpm = Limits::DEFAULT_RPM;
        RuntimeConfig::filter_level = Limits::DEFAULT_FILTER;
        RuntimeConfig::invert_direction = false;
        RuntimeConfig::enable_sync = false;
        RuntimeConfig::sync_ratio = Limits::DEFAULT_SYNC_RATIO;

        current_config.ppr = Limits::DEFAULT_PPR;
        current_config.max_rpm = Limits::DEFAULT_RPM;
        current_config.filter_level = Limits::DEFAULT_FILTER;
        current_config.invert_direction = false;
        current_config.enable_sync = false;
        current_config.sync_ratio = Limits::DEFAULT_SYNC_RATIO;

        SerialDebug.println(F("Encoder configuration reset to defaults"));
    }

    bool Interface::saveConfig()
    {
        // Placeholder for future flash implementation
        SerialDebug.println(F("Encoder save config - Not implemented"));
        return true;
    }

    bool Interface::loadConfig()
    {
        resetToDefaults();
        SerialDebug.println(F("Loaded default encoder configuration"));
        return true;
    }

    uint32_t Interface::calculateChecksum(const SaveableConfig &config)
    {
        // Simple checksum calculation
        uint32_t sum = 0;
        const uint8_t *data = (const uint8_t *)&config;
        for (size_t i = 0; i < sizeof(SaveableConfig) - sizeof(uint32_t); i++)
        {
            sum += data[i];
        }
        return sum;
    }
}