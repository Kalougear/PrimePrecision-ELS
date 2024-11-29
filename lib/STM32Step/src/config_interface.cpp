#include "config_interface.h"
#include "Config/serial_debug.h"

namespace STM32Step
{
    // Initialize static members
    const char *ConfigInterface::PARAM_PULSE_WIDTH = "pulse_width";
    const char *ConfigInterface::PARAM_MICROSTEPS = "microsteps";
    const char *ConfigInterface::PARAM_MAX_SPEED = "max_speed";
    const char *ConfigInterface::PARAM_INVERT_DIR = "invert_dir";
    const char *ConfigInterface::PARAM_INVERT_ENABLE = "invert_enable";

    SaveableConfig ConfigInterface::current_config;

    bool ConfigInterface::updateConfig(const char *param, uint32_t value)
    {
        SerialDebug.print("Updating config: ");
        SerialDebug.print(param);
        SerialDebug.print(" = ");
        SerialDebug.println(value);

        if (strcmp(param, PARAM_PULSE_WIDTH) == 0)
        {
            if (value >= 1 && value <= 20)
            { // Reasonable pulse width range
                RuntimeConfig::current_pulse_width = value;
                current_config.pulse_width = value;
                return true;
            }
        }
        else if (strcmp(param, PARAM_MICROSTEPS) == 0)
        {
            // Check if value is a valid microstep setting (usually powers of 2)
            if (value == 1 || value == 2 || value == 4 || value == 8 ||
                value == 16 || value == 32 || value == 64 || value == 128 ||
                value == 256 || value == 512 || value == 1024 || value == 1600)
            {
                RuntimeConfig::current_microsteps = value;
                current_config.microsteps = value;
                return true;
            }
        }
        else if (strcmp(param, PARAM_MAX_SPEED) == 0)
        {
            if (value >= MotorDefaults::MIN_FREQ && value <= MotorDefaults::MAX_FREQ)
            {
                RuntimeConfig::current_max_speed = value;
                current_config.max_speed = value;
                return true;
            }
        }
        else if (strcmp(param, PARAM_INVERT_DIR) == 0)
        {
            RuntimeConfig::invert_direction = value != 0;
            current_config.invert_direction = value != 0;
            return true;
        }
        else if (strcmp(param, PARAM_INVERT_ENABLE) == 0)
        {
            RuntimeConfig::invert_enable = value != 0;
            current_config.invert_enable = value != 0;
            return true;
        }

        SerialDebug.println("Invalid parameter or value");
        return false;
    }

    uint32_t ConfigInterface::readConfig(const char *param)
    {
        if (strcmp(param, PARAM_PULSE_WIDTH) == 0)
        {
            return RuntimeConfig::current_pulse_width;
        }
        else if (strcmp(param, PARAM_MICROSTEPS) == 0)
        {
            return RuntimeConfig::current_microsteps;
        }
        else if (strcmp(param, PARAM_MAX_SPEED) == 0)
        {
            return RuntimeConfig::current_max_speed;
        }
        else if (strcmp(param, PARAM_INVERT_DIR) == 0)
        {
            return RuntimeConfig::invert_direction ? 1 : 0;
        }
        else if (strcmp(param, PARAM_INVERT_ENABLE) == 0)
        {
            return RuntimeConfig::invert_enable ? 1 : 0;
        }
        return 0;
    }

    bool ConfigInterface::saveConfig()
    {
        // Placeholder for future flash implementation
        SerialDebug.println("Save config - Not implemented");
        return true;
    }

    bool ConfigInterface::loadConfig()
    {
        // Load defaults since we don't have storage yet
        resetToDefaults();
        SerialDebug.println("Loaded default configuration");
        return true;
    }

    void ConfigInterface::resetToDefaults()
    {
        RuntimeConfig::current_pulse_width = TimingConfig::PULSE_WIDTH;
        RuntimeConfig::current_microsteps = MotorDefaults::DEFAULT_MICROSTEPS;
        RuntimeConfig::current_max_speed = MotorDefaults::DEFAULT_FREQ;
        RuntimeConfig::invert_direction = false;
        RuntimeConfig::invert_enable = false;

        current_config.pulse_width = TimingConfig::PULSE_WIDTH;
        current_config.microsteps = MotorDefaults::DEFAULT_MICROSTEPS;
        current_config.max_speed = MotorDefaults::DEFAULT_FREQ;
        current_config.invert_direction = false;
        current_config.invert_enable = false;

        SerialDebug.println("Configuration reset to defaults");
    }
} // namespace STM32Step
