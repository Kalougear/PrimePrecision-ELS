#include "Config/SystemConfig.h"

namespace SystemConfig
{
    // Initialize Encoder Configuration
    uint16_t RuntimeConfig::Encoder::ppr = Limits::Encoder::DEFAULT_PPR;
    uint16_t RuntimeConfig::Encoder::max_rpm = Limits::Encoder::DEFAULT_RPM;
    uint8_t RuntimeConfig::Encoder::filter_level = Limits::Encoder::DEFAULT_FILTER;
    bool RuntimeConfig::Encoder::invert_direction = false;

    // Initialize Stepper Configuration
    uint32_t RuntimeConfig::Stepper::microsteps = Limits::Stepper::DEFAULT_MICROSTEPS;
    uint32_t RuntimeConfig::Stepper::max_speed = Limits::Stepper::DEFAULT_SPEED;
    bool RuntimeConfig::Stepper::invert_direction = false;
    bool RuntimeConfig::Stepper::invert_enable = false;

    // Initialize Motion Configuration
    float RuntimeConfig::Motion::thread_pitch = Limits::Motion::DEFAULT_THREAD_PITCH;
    float RuntimeConfig::Motion::leadscrew_pitch = Limits::Motion::DEFAULT_LEADSCREW_PITCH;
    uint32_t RuntimeConfig::Motion::sync_frequency = Limits::Motion::DEFAULT_SYNC_FREQ;
    bool RuntimeConfig::Motion::sync_enabled = false;

    // Initialize Nextion Parameter Strings
    const char *NextionParameters::PPR = "ppr";
    const char *NextionParameters::MAX_RPM = "maxRpm";
    const char *NextionParameters::FILTER_LEVEL = "filter";
    const char *NextionParameters::ENCODER_DIR = "encDir";
    const char *NextionParameters::MICROSTEPS = "microsteps";
    const char *NextionParameters::MAX_SPEED = "maxSpeed";
    const char *NextionParameters::STEPPER_DIR = "stepDir";
    const char *NextionParameters::INVERT_ENABLE = "invEnable";
    const char *NextionParameters::THREAD_PITCH = "threadPitch";
    const char *NextionParameters::SYNC_FREQ = "syncFreq";
    const char *NextionParameters::SYNC_ENABLE = "syncEn";

    bool ConfigManager::updateConfig(const char *param, uint32_t value)
    {
        if (!validateParameter(param, value))
            return false;

        // Encoder Parameters
        if (strcmp(param, NextionParameters::PPR) == 0)
            RuntimeConfig::Encoder::ppr = value;
        else if (strcmp(param, NextionParameters::MAX_RPM) == 0)
            RuntimeConfig::Encoder::max_rpm = value;
        else if (strcmp(param, NextionParameters::FILTER_LEVEL) == 0)
            RuntimeConfig::Encoder::filter_level = value;
        else if (strcmp(param, NextionParameters::ENCODER_DIR) == 0)
            RuntimeConfig::Encoder::invert_direction = value;

        // Stepper Parameters
        else if (strcmp(param, NextionParameters::MICROSTEPS) == 0)
            RuntimeConfig::Stepper::microsteps = value;
        else if (strcmp(param, NextionParameters::MAX_SPEED) == 0)
            RuntimeConfig::Stepper::max_speed = value;
        else if (strcmp(param, NextionParameters::STEPPER_DIR) == 0)
            RuntimeConfig::Stepper::invert_direction = value;
        else if (strcmp(param, NextionParameters::INVERT_ENABLE) == 0)
            RuntimeConfig::Stepper::invert_enable = value;

        // Motion Parameters
        else if (strcmp(param, NextionParameters::SYNC_FREQ) == 0)
            RuntimeConfig::Motion::sync_frequency = value;
        else if (strcmp(param, NextionParameters::SYNC_ENABLE) == 0)
            RuntimeConfig::Motion::sync_enabled = value;
        else
            return false;

        return true;
    }

    uint32_t ConfigManager::readConfig(const char *param)
    {
        // Encoder Parameters
        if (strcmp(param, NextionParameters::PPR) == 0)
            return RuntimeConfig::Encoder::ppr;
        else if (strcmp(param, NextionParameters::MAX_RPM) == 0)
            return RuntimeConfig::Encoder::max_rpm;
        else if (strcmp(param, NextionParameters::FILTER_LEVEL) == 0)
            return RuntimeConfig::Encoder::filter_level;
        else if (strcmp(param, NextionParameters::ENCODER_DIR) == 0)
            return RuntimeConfig::Encoder::invert_direction;

        // Stepper Parameters
        else if (strcmp(param, NextionParameters::MICROSTEPS) == 0)
            return RuntimeConfig::Stepper::microsteps;
        else if (strcmp(param, NextionParameters::MAX_SPEED) == 0)
            return RuntimeConfig::Stepper::max_speed;
        else if (strcmp(param, NextionParameters::STEPPER_DIR) == 0)
            return RuntimeConfig::Stepper::invert_direction;
        else if (strcmp(param, NextionParameters::INVERT_ENABLE) == 0)
            return RuntimeConfig::Stepper::invert_enable;

        // Motion Parameters
        else if (strcmp(param, NextionParameters::SYNC_FREQ) == 0)
            return RuntimeConfig::Motion::sync_frequency;
        else if (strcmp(param, NextionParameters::SYNC_ENABLE) == 0)
            return RuntimeConfig::Motion::sync_enabled;

        return 0;
    }

    bool ConfigManager::validateParameter(const char *param, uint32_t value)
    {
        // Encoder parameter validation
        if (strcmp(param, NextionParameters::PPR) == 0)
            return value >= Limits::Encoder::MIN_PPR && value <= Limits::Encoder::MAX_PPR;
        else if (strcmp(param, NextionParameters::MAX_RPM) == 0)
            return value >= Limits::Encoder::MIN_RPM && value <= Limits::Encoder::MAX_RPM;
        else if (strcmp(param, NextionParameters::FILTER_LEVEL) == 0)
            return value >= Limits::Encoder::MIN_FILTER && value <= Limits::Encoder::MAX_FILTER;

        // Stepper parameter validation
        else if (strcmp(param, NextionParameters::MAX_SPEED) == 0)
            return value >= Limits::Stepper::MIN_SPEED && value <= Limits::Stepper::MAX_SPEED;

        // Motion parameter validation
        else if (strcmp(param, NextionParameters::SYNC_FREQ) == 0)
            return value >= Limits::Motion::MIN_SYNC_FREQ && value <= Limits::Motion::MAX_SYNC_FREQ;

        // Boolean parameters (no validation needed)
        else if (strcmp(param, NextionParameters::ENCODER_DIR) == 0 ||
                 strcmp(param, NextionParameters::STEPPER_DIR) == 0 ||
                 strcmp(param, NextionParameters::INVERT_ENABLE) == 0 ||
                 strcmp(param, NextionParameters::SYNC_ENABLE) == 0)
            return true;

        return false;
    }

    void ConfigManager::resetToDefaults()
    {
        // Reset Encoder configuration
        RuntimeConfig::Encoder::ppr = Limits::Encoder::DEFAULT_PPR;
        RuntimeConfig::Encoder::max_rpm = Limits::Encoder::DEFAULT_RPM;
        RuntimeConfig::Encoder::filter_level = Limits::Encoder::DEFAULT_FILTER;
        RuntimeConfig::Encoder::invert_direction = false;

        // Reset Stepper configuration
        RuntimeConfig::Stepper::microsteps = Limits::Stepper::DEFAULT_MICROSTEPS;
        RuntimeConfig::Stepper::max_speed = Limits::Stepper::DEFAULT_SPEED;
        RuntimeConfig::Stepper::invert_direction = false;
        RuntimeConfig::Stepper::invert_enable = false;

        // Reset Motion configuration
        RuntimeConfig::Motion::thread_pitch = Limits::Motion::DEFAULT_THREAD_PITCH;
        RuntimeConfig::Motion::leadscrew_pitch = Limits::Motion::DEFAULT_LEADSCREW_PITCH;
        RuntimeConfig::Motion::sync_frequency = Limits::Motion::DEFAULT_SYNC_FREQ;
        RuntimeConfig::Motion::sync_enabled = false;
    }

} // namespace SystemConfig
