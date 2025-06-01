#include "Config/SystemConfig.h"
#include "eeprom.h"         // For EEPROM functions
#include <cstring>          // For memcpy
#include <HardwareSerial.h> // For HardwareSerial type

extern HardwareSerial SerialDebug; // Ensure SerialDebug is declared

// Define the virtual addresses for EEPROM storage
// NB_OF_VAR must be 29 in eeprom.h
// These are just example addresses; ensure they are unique and not 0xFFFF.
// Order them logically for easier management.
// Each float or uint32_t will take two uint16_t virtual addresses.

// Virtual addresses for EEPROM
// Total 29 words (uint16_t)
// Encoder: 4 words
#define VIRT_ADDR_ENCODER_PPR 0x0001
#define VIRT_ADDR_ENCODER_MAX_RPM 0x0002
#define VIRT_ADDR_ENCODER_FILTER 0x0003
#define VIRT_ADDR_ENCODER_INV_DIR 0x0004
// Stepper: 6 words
#define VIRT_ADDR_STEPPER_MICROSTEPS_L 0x0005 // Low word for uint32_t
#define VIRT_ADDR_STEPPER_MICROSTEPS_H 0x0006 // High word for uint32_t
#define VIRT_ADDR_STEPPER_MAX_SPEED_L 0x0007
#define VIRT_ADDR_STEPPER_MAX_SPEED_H 0x0008
#define VIRT_ADDR_STEPPER_INV_DIR 0x0009
#define VIRT_ADDR_STEPPER_INV_ENABLE 0x000A
// Motion: 5 words
#define VIRT_ADDR_MOTION_THREAD_PITCH_L 0x000B // Low word for float
#define VIRT_ADDR_MOTION_THREAD_PITCH_H 0x000C // High word for float
#define VIRT_ADDR_MOTION_SYNC_FREQ_L 0x000D
#define VIRT_ADDR_MOTION_SYNC_FREQ_H 0x000E
#define VIRT_ADDR_MOTION_SYNC_ENABLED 0x000F
// Z_Axis: 13 words
#define VIRT_ADDR_Z_AXIS_INV_DIR 0x0010
#define VIRT_ADDR_Z_AXIS_MOTOR_TEETH 0x0011
#define VIRT_ADDR_Z_AXIS_LEAD_SCREW_TEETH 0x0012
#define VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_L 0x0013
#define VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_H 0x0014
#define VIRT_ADDR_Z_AXIS_DRIVER_PULSES_L 0x0015
#define VIRT_ADDR_Z_AXIS_DRIVER_PULSES_H 0x0016
#define VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_L 0x0017
#define VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_H 0x0018
#define VIRT_ADDR_Z_AXIS_ACCELERATION_L 0x0019
#define VIRT_ADDR_Z_AXIS_ACCELERATION_H 0x001A
#define VIRT_ADDR_Z_AXIS_BACKLASH_L 0x001B
#define VIRT_ADDR_Z_AXIS_BACKLASH_H 0x001C
// System: 1 word
#define VIRT_ADDR_SYSTEM_UNITS_METRIC 0x001D // Corresponds to RuntimeConfig::System::measurement_unit_is_metric

// NEW Virtual Addresses for Setup Tab parameters
#define VIRT_ADDR_SYSTEM_ELS_FEED_UNIT_METRIC 0x001E // Corresponds to RuntimeConfig::System::els_default_feed_rate_unit_is_metric (bool)
#define VIRT_ADDR_SPINDLE_CHUCK_TEETH 0x001F         // Corresponds to RuntimeConfig::Spindle::chuck_pulley_teeth (uint16_t)
#define VIRT_ADDR_SPINDLE_ENCODER_TEETH 0x0020       // Corresponds to RuntimeConfig::Spindle::encoder_pulley_teeth (uint16_t)
#define VIRT_ADDR_Z_AXIS_LS_STD_METRIC 0x0021        // Corresponds to RuntimeConfig::Z_Axis::leadscrew_standard_is_metric (bool)
#define VIRT_ADDR_Z_AXIS_ENABLE_POL_HIGH 0x0022      // Corresponds to RuntimeConfig::Z_Axis::enable_polarity_active_high (bool)
#define VIRT_ADDR_JOG_SYSTEM_ENABLED 0x0023          // Corresponds to RuntimeConfig::System::jog_system_enabled (bool)
#define VIRT_ADDR_MAX_JOG_SPEED_L 0x0024             // Corresponds to RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min (float L)
#define VIRT_ADDR_MAX_JOG_SPEED_H 0x0025             // Corresponds to RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min (float H)
#define VIRT_ADDR_DEFAULT_JOG_SPEED_INDEX 0x0026     // Corresponds to RuntimeConfig::System::default_jog_speed_index (uint8_t)
                                                     // Last address (35 + 2 = 37th)

// This array must be defined and accessible by eeprom.c (it's extern there)
// It lists all unique virtual addresses used. The order here doesn't strictly
// matter for the EEPROM lib, but it's good practice to list them.
// The size of this array MUST match NB_OF_VAR in eeprom.h (which will be 37).
uint16_t VirtAddVarTab[NB_OF_VAR] = {
    VIRT_ADDR_ENCODER_PPR, VIRT_ADDR_ENCODER_MAX_RPM, VIRT_ADDR_ENCODER_FILTER, VIRT_ADDR_ENCODER_INV_DIR,
    VIRT_ADDR_STEPPER_MICROSTEPS_L, VIRT_ADDR_STEPPER_MICROSTEPS_H,
    VIRT_ADDR_STEPPER_MAX_SPEED_L, VIRT_ADDR_STEPPER_MAX_SPEED_H,
    VIRT_ADDR_STEPPER_INV_DIR, VIRT_ADDR_STEPPER_INV_ENABLE,
    VIRT_ADDR_MOTION_THREAD_PITCH_L, VIRT_ADDR_MOTION_THREAD_PITCH_H,
    VIRT_ADDR_MOTION_SYNC_FREQ_L, VIRT_ADDR_MOTION_SYNC_FREQ_H,
    VIRT_ADDR_MOTION_SYNC_ENABLED,
    VIRT_ADDR_Z_AXIS_INV_DIR, VIRT_ADDR_Z_AXIS_MOTOR_TEETH, VIRT_ADDR_Z_AXIS_LEAD_SCREW_TEETH,
    VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_L, VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_H,
    VIRT_ADDR_Z_AXIS_DRIVER_PULSES_L, VIRT_ADDR_Z_AXIS_DRIVER_PULSES_H,
    VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_L, VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_H,
    VIRT_ADDR_Z_AXIS_ACCELERATION_L, VIRT_ADDR_Z_AXIS_ACCELERATION_H,
    VIRT_ADDR_Z_AXIS_BACKLASH_L, VIRT_ADDR_Z_AXIS_BACKLASH_H,
    VIRT_ADDR_SYSTEM_UNITS_METRIC,
    // New addresses added to the table
    VIRT_ADDR_SYSTEM_ELS_FEED_UNIT_METRIC,
    VIRT_ADDR_SPINDLE_CHUCK_TEETH,
    VIRT_ADDR_SPINDLE_ENCODER_TEETH,
    VIRT_ADDR_Z_AXIS_LS_STD_METRIC,
    VIRT_ADDR_Z_AXIS_ENABLE_POL_HIGH,
    VIRT_ADDR_JOG_SYSTEM_ENABLED,
    VIRT_ADDR_MAX_JOG_SPEED_L, VIRT_ADDR_MAX_JOG_SPEED_H,
    VIRT_ADDR_DEFAULT_JOG_SPEED_INDEX};

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
    // RuntimeConfig::Motion::leadscrew_pitch is removed, now part of Z_Axis
    uint32_t RuntimeConfig::Motion::sync_frequency = Limits::Motion::DEFAULT_SYNC_FREQ;
    bool RuntimeConfig::Motion::sync_enabled = false;

    // Initialize Z_Axis Configuration
    volatile bool RuntimeConfig::Z_Axis::invert_direction = Limits::Z_Axis::DEFAULT_INVERT_DIRECTION; // Added volatile
    uint16_t RuntimeConfig::Z_Axis::motor_pulley_teeth = Limits::Z_Axis::DEFAULT_MOTOR_PULLEY_TEETH;
    uint16_t RuntimeConfig::Z_Axis::lead_screw_pulley_teeth = Limits::Z_Axis::DEFAULT_LEAD_SCREW_PULLEY_TEETH;
    float RuntimeConfig::Z_Axis::lead_screw_pitch = Limits::Z_Axis::DEFAULT_LEAD_SCREW_PITCH;
    uint32_t RuntimeConfig::Z_Axis::driver_pulses_per_rev = Limits::Z_Axis::DEFAULT_DRIVER_PULSES_PER_REV;
    float RuntimeConfig::Z_Axis::max_feed_rate = Limits::Z_Axis::DEFAULT_MAX_FEED_RATE;
    float RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min = Limits::Z_Axis::DEFAULT_MAX_JOG_SPEED_MM_PER_MIN;
    float RuntimeConfig::Z_Axis::acceleration = Limits::Z_Axis::DEFAULT_ACCELERATION;
    float RuntimeConfig::Z_Axis::backlash_compensation = Limits::Z_Axis::DEFAULT_BACKLASH_COMPENSATION;
    bool RuntimeConfig::Z_Axis::leadscrew_standard_is_metric = Limits::Z_Axis::DEFAULT_LEADSCREW_STANDARD_IS_METRIC;
    bool RuntimeConfig::Z_Axis::enable_polarity_active_high = Limits::Z_Axis::DEFAULT_ENABLE_POLARITY_ACTIVE_HIGH;
    // RuntimeConfig::Z_Axis::min_step_pulse_us and dir_setup_time_us are not typically runtime changeable from HMI in this manner,
    // they are more like fixed hardware timing characteristics set at compile time or deeper config.
    // If they need to be EEPROM stored and HMI changeable, they'll need full handling. For now, assuming fixed.

    // Initialize System Configuration
    bool RuntimeConfig::System::measurement_unit_is_metric = Limits::General::DEFAULT_MEASUREMENT_UNIT_IS_METRIC;
    bool RuntimeConfig::System::els_default_feed_rate_unit_is_metric = Limits::General::DEFAULT_ELS_FEED_RATE_UNIT_IS_METRIC;
    bool RuntimeConfig::System::jog_system_enabled = Limits::General::DEFAULT_JOG_SYSTEM_ENABLED;
    uint8_t RuntimeConfig::System::default_jog_speed_index = Limits::General::DEFAULT_JOG_SPEED_INDEX;

    // Initialize Spindle Configuration
    uint16_t RuntimeConfig::Spindle::chuck_pulley_teeth = Limits::Spindle::DEFAULT_CHUCK_PULLEY_TEETH;
    uint16_t RuntimeConfig::Spindle::encoder_pulley_teeth = Limits::Spindle::DEFAULT_ENCODER_PULLEY_TEETH;

    // Initialize Dirty Flags (all to false initially)
    bool RuntimeConfigDirtyFlags::Encoder::ppr = false;
    bool RuntimeConfigDirtyFlags::Encoder::max_rpm = false;
    bool RuntimeConfigDirtyFlags::Encoder::filter_level = false;
    bool RuntimeConfigDirtyFlags::Encoder::invert_direction = false;

    bool RuntimeConfigDirtyFlags::Stepper::microsteps = false;
    bool RuntimeConfigDirtyFlags::Stepper::max_speed = false;
    bool RuntimeConfigDirtyFlags::Stepper::invert_direction = false;
    bool RuntimeConfigDirtyFlags::Stepper::invert_enable = false;

    bool RuntimeConfigDirtyFlags::Motion::thread_pitch = false;
    bool RuntimeConfigDirtyFlags::Motion::sync_frequency = false;
    bool RuntimeConfigDirtyFlags::Motion::sync_enabled = false;

    bool RuntimeConfigDirtyFlags::Z_Axis::invert_direction = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::motor_pulley_teeth = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pulley_teeth = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::driver_pulses_per_rev = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::max_feed_rate = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::max_jog_speed_mm_per_min = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::acceleration = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::backlash_compensation = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::leadscrew_standard_is_metric = false;
    bool RuntimeConfigDirtyFlags::Z_Axis::enable_polarity_active_high = false;

    bool RuntimeConfigDirtyFlags::System::measurement_unit_is_metric = false;
    bool RuntimeConfigDirtyFlags::System::els_default_feed_rate_unit_is_metric = false;
    bool RuntimeConfigDirtyFlags::System::jog_system_enabled = false;
    bool RuntimeConfigDirtyFlags::System::default_jog_speed_index = false;

    bool RuntimeConfigDirtyFlags::Spindle::chuck_pulley_teeth = false;
    bool RuntimeConfigDirtyFlags::Spindle::encoder_pulley_teeth = false;

    // Initialize HMI Parameter Strings
    const char *HmiParameters::PPR = "ppr";
    const char *HmiParameters::MAX_RPM = "maxRpm";
    const char *HmiParameters::FILTER_LEVEL = "filter";
    const char *HmiParameters::ENCODER_DIR = "encDir";
    const char *HmiParameters::MICROSTEPS = "microsteps";
    const char *HmiParameters::MAX_SPEED = "maxSpeed";
    const char *HmiParameters::STEPPER_DIR = "stepDir";
    const char *HmiParameters::INVERT_ENABLE = "invEnable";
    const char *HmiParameters::THREAD_PITCH = "threadPitch";
    const char *HmiParameters::SYNC_FREQ = "syncFreq";
    const char *HmiParameters::SYNC_ENABLE = "syncEn";
    // NEW HMI Parameters from SystemConfig.h that were missing definitions here
    const char *HmiParameters::ELS_FEED_UNIT = "elsFeedUnit";  // Corresponds to RuntimeConfig::System::els_default_feed_rate_unit_is_metric
    const char *HmiParameters::CHUCK_TEETH = "chuckTeeth";     // Corresponds to RuntimeConfig::Spindle::chuck_pulley_teeth
    const char *HmiParameters::ENCODER_TEETH = "encoderTeeth"; // Corresponds to RuntimeConfig::Spindle::encoder_pulley_teeth
    const char *HmiParameters::LS_STD_METRIC = "lsStdMetric";  // Corresponds to RuntimeConfig::Z_Axis::leadscrew_standard_is_metric
    const char *HmiParameters::Z_ENABLE_POL = "zEnPol";        // Corresponds to RuntimeConfig::Z_Axis::enable_polarity_active_high
    // Z_MIN_STEP_US and Z_DIR_SETUP_US are not added as HMI params for now, assuming fixed.
    // Add a general system measurement unit HMI param if needed for direct HMI updateConfig
    const char *HMI_MEASUREMENT_UNIT = "measureUnit"; // Corresponds to RuntimeConfig::System::measurement_unit_is_metric
    const char *HMI_JOG_SYSTEM_ENABLED = "jogSysEn";  // Corresponds to RuntimeConfig::System::jog_system_enabled
    const char *HMI_MAX_JOG_SPEED = "maxJogSpeed";    // Corresponds to RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min
    const char *HMI_DEF_JOG_SPEED_IDX = "defJogIdx";  // Corresponds to RuntimeConfig::System::default_jog_speed_index

    bool ConfigManager::updateConfig(const char *param, uint32_t value)
    {
        if (!validateParameter(param, value))
            return false;

        // Encoder Parameters
        if (strcmp(param, HmiParameters::PPR) == 0)
            RuntimeConfig::Encoder::ppr = value;
        else if (strcmp(param, HmiParameters::MAX_RPM) == 0)
            RuntimeConfig::Encoder::max_rpm = value;
        else if (strcmp(param, HmiParameters::FILTER_LEVEL) == 0)
            RuntimeConfig::Encoder::filter_level = value;
        else if (strcmp(param, HmiParameters::ENCODER_DIR) == 0)
            RuntimeConfig::Encoder::invert_direction = value;

        // Stepper Parameters
        else if (strcmp(param, HmiParameters::MICROSTEPS) == 0)
            RuntimeConfig::Stepper::microsteps = value;
        else if (strcmp(param, HmiParameters::MAX_SPEED) == 0)
            RuntimeConfig::Stepper::max_speed = value;
        else if (strcmp(param, HmiParameters::STEPPER_DIR) == 0)
            RuntimeConfig::Stepper::invert_direction = value;
        else if (strcmp(param, HmiParameters::INVERT_ENABLE) == 0)
            RuntimeConfig::Stepper::invert_enable = value;

        // Motion Parameters
        else if (strcmp(param, HmiParameters::SYNC_FREQ) == 0)
            RuntimeConfig::Motion::sync_frequency = value;
        else if (strcmp(param, HmiParameters::SYNC_ENABLE) == 0)
            RuntimeConfig::Motion::sync_enabled = value;
        else
            return false;

        return true;
    }

    uint32_t ConfigManager::readConfig(const char *param)
    {
        // Encoder Parameters
        if (strcmp(param, HmiParameters::PPR) == 0)
            return RuntimeConfig::Encoder::ppr;
        else if (strcmp(param, HmiParameters::MAX_RPM) == 0)
            return RuntimeConfig::Encoder::max_rpm;
        else if (strcmp(param, HmiParameters::FILTER_LEVEL) == 0)
            return RuntimeConfig::Encoder::filter_level;
        else if (strcmp(param, HmiParameters::ENCODER_DIR) == 0)
            return RuntimeConfig::Encoder::invert_direction;

        // Stepper Parameters
        else if (strcmp(param, HmiParameters::MICROSTEPS) == 0)
            return RuntimeConfig::Stepper::microsteps;
        else if (strcmp(param, HmiParameters::MAX_SPEED) == 0)
            return RuntimeConfig::Stepper::max_speed;
        else if (strcmp(param, HmiParameters::STEPPER_DIR) == 0)
            return RuntimeConfig::Stepper::invert_direction;
        else if (strcmp(param, HmiParameters::INVERT_ENABLE) == 0)
            return RuntimeConfig::Stepper::invert_enable;

        // Motion Parameters
        else if (strcmp(param, HmiParameters::SYNC_FREQ) == 0)
            return RuntimeConfig::Motion::sync_frequency;
        else if (strcmp(param, HmiParameters::SYNC_ENABLE) == 0)
            return RuntimeConfig::Motion::sync_enabled;

        return 0;
    }

    bool ConfigManager::validateParameter(const char *param, uint32_t value)
    {
        // Encoder parameter validation
        if (strcmp(param, HmiParameters::PPR) == 0)
            return value >= Limits::Encoder::MIN_PPR && value <= Limits::Encoder::MAX_PPR;
        else if (strcmp(param, HmiParameters::MAX_RPM) == 0)
            return value >= Limits::Encoder::MIN_RPM && value <= Limits::Encoder::MAX_RPM;
        else if (strcmp(param, HmiParameters::FILTER_LEVEL) == 0)
            return value >= Limits::Encoder::MIN_FILTER && value <= Limits::Encoder::MAX_FILTER;

        // Stepper parameter validation
        else if (strcmp(param, HmiParameters::MAX_SPEED) == 0)
            return value >= Limits::Stepper::MIN_SPEED && value <= Limits::Stepper::MAX_SPEED;

        // Motion parameter validation
        else if (strcmp(param, HmiParameters::SYNC_FREQ) == 0)
            return value >= Limits::Motion::MIN_SYNC_FREQ && value <= Limits::Motion::MAX_SYNC_FREQ;

        // Boolean parameters (no validation needed)
        else if (strcmp(param, HmiParameters::ENCODER_DIR) == 0 ||
                 strcmp(param, HmiParameters::STEPPER_DIR) == 0 ||
                 strcmp(param, HmiParameters::INVERT_ENABLE) == 0 ||
                 strcmp(param, HmiParameters::SYNC_ENABLE) == 0)
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
        // RuntimeConfig::Motion::leadscrew_pitch removed
        RuntimeConfig::Motion::sync_frequency = Limits::Motion::DEFAULT_SYNC_FREQ;
        RuntimeConfig::Motion::sync_enabled = false;

        // Reset Z_Axis configuration
        RuntimeConfig::Z_Axis::invert_direction = Limits::Z_Axis::DEFAULT_INVERT_DIRECTION;
        RuntimeConfig::Z_Axis::motor_pulley_teeth = Limits::Z_Axis::DEFAULT_MOTOR_PULLEY_TEETH;
        RuntimeConfig::Z_Axis::lead_screw_pulley_teeth = Limits::Z_Axis::DEFAULT_LEAD_SCREW_PULLEY_TEETH;
        RuntimeConfig::Z_Axis::lead_screw_pitch = Limits::Z_Axis::DEFAULT_LEAD_SCREW_PITCH;
        RuntimeConfig::Z_Axis::driver_pulses_per_rev = Limits::Z_Axis::DEFAULT_DRIVER_PULSES_PER_REV;
        RuntimeConfig::Z_Axis::max_feed_rate = Limits::Z_Axis::DEFAULT_MAX_FEED_RATE;
        RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min = Limits::Z_Axis::DEFAULT_MAX_JOG_SPEED_MM_PER_MIN;
        RuntimeConfig::Z_Axis::acceleration = Limits::Z_Axis::DEFAULT_ACCELERATION;
        RuntimeConfig::Z_Axis::backlash_compensation = Limits::Z_Axis::DEFAULT_BACKLASH_COMPENSATION;
        RuntimeConfig::Z_Axis::leadscrew_standard_is_metric = Limits::Z_Axis::DEFAULT_LEADSCREW_STANDARD_IS_METRIC;
        RuntimeConfig::Z_Axis::enable_polarity_active_high = Limits::Z_Axis::DEFAULT_ENABLE_POLARITY_ACTIVE_HIGH;

        // Reset System configuration
        RuntimeConfig::System::measurement_unit_is_metric = Limits::General::DEFAULT_MEASUREMENT_UNIT_IS_METRIC;
        RuntimeConfig::System::els_default_feed_rate_unit_is_metric = Limits::General::DEFAULT_ELS_FEED_RATE_UNIT_IS_METRIC;
        RuntimeConfig::System::jog_system_enabled = Limits::General::DEFAULT_JOG_SYSTEM_ENABLED;
        RuntimeConfig::System::default_jog_speed_index = Limits::General::DEFAULT_JOG_SPEED_INDEX;

        // Reset Spindle configuration
        RuntimeConfig::Spindle::chuck_pulley_teeth = Limits::Spindle::DEFAULT_CHUCK_PULLEY_TEETH;
        RuntimeConfig::Spindle::encoder_pulley_teeth = Limits::Spindle::DEFAULT_ENCODER_PULLEY_TEETH;

        // Set all dirty flags to true as these are new values to be saved
        RuntimeConfigDirtyFlags::Encoder::ppr = true;
        RuntimeConfigDirtyFlags::Encoder::max_rpm = true;
        RuntimeConfigDirtyFlags::Encoder::filter_level = true;
        RuntimeConfigDirtyFlags::Encoder::invert_direction = true;
        RuntimeConfigDirtyFlags::Stepper::microsteps = true;
        RuntimeConfigDirtyFlags::Stepper::max_speed = true;
        RuntimeConfigDirtyFlags::Stepper::invert_direction = true;
        RuntimeConfigDirtyFlags::Stepper::invert_enable = true;
        RuntimeConfigDirtyFlags::Motion::thread_pitch = true;
        RuntimeConfigDirtyFlags::Motion::sync_frequency = true;
        RuntimeConfigDirtyFlags::Motion::sync_enabled = true;
        RuntimeConfigDirtyFlags::Z_Axis::invert_direction = true;
        RuntimeConfigDirtyFlags::Z_Axis::motor_pulley_teeth = true;
        RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pulley_teeth = true;
        RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = true;
        RuntimeConfigDirtyFlags::Z_Axis::driver_pulses_per_rev = true;
        RuntimeConfigDirtyFlags::Z_Axis::max_feed_rate = true;
        RuntimeConfigDirtyFlags::Z_Axis::max_jog_speed_mm_per_min = true;
        RuntimeConfigDirtyFlags::Z_Axis::acceleration = true;
        RuntimeConfigDirtyFlags::Z_Axis::backlash_compensation = true;
        RuntimeConfigDirtyFlags::Z_Axis::leadscrew_standard_is_metric = true;
        RuntimeConfigDirtyFlags::Z_Axis::enable_polarity_active_high = true;
        RuntimeConfigDirtyFlags::System::measurement_unit_is_metric = true;
        RuntimeConfigDirtyFlags::System::els_default_feed_rate_unit_is_metric = true;
        RuntimeConfigDirtyFlags::System::jog_system_enabled = true;
        RuntimeConfigDirtyFlags::System::default_jog_speed_index = true;
        RuntimeConfigDirtyFlags::Spindle::chuck_pulley_teeth = true;
        RuntimeConfigDirtyFlags::Spindle::encoder_pulley_teeth = true;
    }

    // --- EEPROM Load/Save Implementation ---

    // Helper union for type punning float <-> uint16_t[2] and uint32_t <-> uint16_t[2]
    union FloatConverter
    {
        float f;
        uint16_t u16[2];
        uint32_t u32;
    };

    bool ConfigManager::initialize()
    {
        // Unlock Flash for EEPROM operations
        if (HAL_FLASH_Unlock() != HAL_OK)
        {
            // Handle Flash unlock error (e.g., log, halt)
            return false;
        }

        uint16_t eeprom_status = EE_Init();
        if (eeprom_status == EE_OK)
        {
            if (!loadAllSettings())
            {
                // Failed to load settings (e.g. variables not found, could be first boot or corruption)
                // So, reset to defaults and save them.
                resetToDefaults();
                if (!saveAllSettings())
                {
                    // Handle error saving defaults
                    HAL_FLASH_Lock(); // Lock flash even on error
                    return false;
                }
            }
        }
        else
        {
            // EE_Init failed. This is serious. Could try to format and re-init, or halt.
            // For now, just indicate failure.
            HAL_FLASH_Lock(); // Lock flash even on error
            return false;
        }
        HAL_FLASH_Lock(); // Lock flash after successful init/load
        return true;
    }

    bool ConfigManager::loadAllSettings()
    {
        // SerialDebug.println("--- Loading All Settings from EEPROM ---");
        FloatConverter converter;
        uint16_t val_l, val_h; // For 32-bit types
        uint16_t temp_filter;
        uint16_t temp_bool;

        // Encoder (Only PPR is directly on Setup Page HMI)
        if (EE_ReadVariable(VIRT_ADDR_ENCODER_PPR, &RuntimeConfig::Encoder::ppr) != EE_OK)
        {
            // SerialDebug.println("Error loading Encoder PPR");
            return false;
        }
        // SerialDebug.print("Loaded Encoder PPR: ");
        // SerialDebug.println(RuntimeConfig::Encoder::ppr);
        // Parameters not on Setup Page HMI - no logging for them during load
        if (EE_ReadVariable(VIRT_ADDR_ENCODER_MAX_RPM, &RuntimeConfig::Encoder::max_rpm) != EE_OK)
            return false;
        if (EE_ReadVariable(VIRT_ADDR_ENCODER_FILTER, &temp_filter) != EE_OK)
            return false;
        RuntimeConfig::Encoder::filter_level = static_cast<uint8_t>(temp_filter);
        if (EE_ReadVariable(VIRT_ADDR_ENCODER_INV_DIR, &temp_bool) != EE_OK)
            return false;
        RuntimeConfig::Encoder::invert_direction = (temp_bool != 0);

        // Stepper (These general stepper params are not on Setup Page HMI)
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_MICROSTEPS_L, &val_l) != EE_OK)
            return false;
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_MICROSTEPS_H, &val_h) != EE_OK)
            return false;
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Stepper::microsteps = converter.u32;
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_MAX_SPEED_L, &val_l) != EE_OK)
            return false;
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_MAX_SPEED_H, &val_h) != EE_OK)
            return false;
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Stepper::max_speed = converter.u32;
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_INV_DIR, &temp_bool) != EE_OK)
            return false;
        RuntimeConfig::Stepper::invert_direction = (temp_bool != 0);
        if (EE_ReadVariable(VIRT_ADDR_STEPPER_INV_ENABLE, &temp_bool) != EE_OK)
            return false;
        RuntimeConfig::Stepper::invert_enable = (temp_bool != 0);

        // Motion (These general motion params are not on Setup Page HMI)
        if (EE_ReadVariable(VIRT_ADDR_MOTION_THREAD_PITCH_L, &val_l) != EE_OK)
            return false;
        if (EE_ReadVariable(VIRT_ADDR_MOTION_THREAD_PITCH_H, &val_h) != EE_OK)
            return false;
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Motion::thread_pitch = converter.f;
        if (EE_ReadVariable(VIRT_ADDR_MOTION_SYNC_FREQ_L, &val_l) != EE_OK)
            return false;
        if (EE_ReadVariable(VIRT_ADDR_MOTION_SYNC_FREQ_H, &val_h) != EE_OK)
            return false;
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Motion::sync_frequency = converter.u32;
        if (EE_ReadVariable(VIRT_ADDR_MOTION_SYNC_ENABLED, &temp_bool) != EE_OK)
            return false;
        RuntimeConfig::Motion::sync_enabled = (temp_bool != 0);

        // Z_Axis (All these are on Setup Page HMI)
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_INV_DIR, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Inv Dir");
            return false;
        }
        RuntimeConfig::Z_Axis::invert_direction = (temp_bool != 0);
        // SerialDebug.print("Loaded Z-Axis Invert Direction: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::invert_direction);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_MOTOR_TEETH, &RuntimeConfig::Z_Axis::motor_pulley_teeth) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Motor Teeth");
            return false;
        }
        // SerialDebug.print("Loaded Z-Axis Motor Pulley Teeth: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::motor_pulley_teeth);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_LEAD_SCREW_TEETH, &RuntimeConfig::Z_Axis::lead_screw_pulley_teeth) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Leadscrew Teeth");
            return false;
        }
        // SerialDebug.print("Loaded Z-Axis Leadscrew Pulley Teeth: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Leadscrew Pitch (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Leadscrew Pitch (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::lead_screw_pitch = converter.f;
        // SerialDebug.print("Loaded Z-Axis Leadscrew Pitch: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::lead_screw_pitch);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_DRIVER_PULSES_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Driver Pulses (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_DRIVER_PULSES_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Driver Pulses (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::driver_pulses_per_rev = converter.u32;
        // SerialDebug.print("Loaded Z-Axis Driver Pulses/Rev: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::driver_pulses_per_rev);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Max Feed Rate (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Max Feed Rate (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::max_feed_rate = converter.f;
        // SerialDebug.print("Loaded Z-Axis Max Feed Rate: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::max_feed_rate);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_ACCELERATION_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Acceleration (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_ACCELERATION_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Acceleration (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::acceleration = converter.f;
        // SerialDebug.print("Loaded Z-Axis Acceleration: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::acceleration);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_BACKLASH_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Backlash (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_BACKLASH_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Backlash (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::backlash_compensation = converter.f;
        // SerialDebug.print("Loaded Z-Axis Backlash Compensation: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::backlash_compensation);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_LS_STD_METRIC, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Leadscrew Standard Metric");
            return false;
        }
        RuntimeConfig::Z_Axis::leadscrew_standard_is_metric = (temp_bool == 0); // HMI 0=Metric, stored 0=Metric
        // SerialDebug.print("Loaded Z-Axis Leadscrew Standard Metric (0=Metric): ");
        // SerialDebug.println(temp_bool);
        if (EE_ReadVariable(VIRT_ADDR_Z_AXIS_ENABLE_POL_HIGH, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading Z-Axis Enable Polarity High");
            return false;
        }
        RuntimeConfig::Z_Axis::enable_polarity_active_high = (temp_bool != 0);
        // SerialDebug.print("Loaded Z-Axis Enable Polarity Active High: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::enable_polarity_active_high);

        // System
        if (EE_ReadVariable(VIRT_ADDR_SYSTEM_UNITS_METRIC, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading System Measurement Unit Metric");
            return false;
        }
        RuntimeConfig::System::measurement_unit_is_metric = (temp_bool == 0); // HMI 0=Metric, stored 0=Metric
        // SerialDebug.print("Loaded System Measurement Unit Metric (0=Metric): ");
        // SerialDebug.println(temp_bool);
        if (EE_ReadVariable(VIRT_ADDR_SYSTEM_ELS_FEED_UNIT_METRIC, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading System ELS Feed Unit Metric");
            return false;
        }
        RuntimeConfig::System::els_default_feed_rate_unit_is_metric = (temp_bool == 0); // HMI 0=Metric, stored 0=Metric
        // SerialDebug.print("Loaded System ELS Default Feed Unit Metric (0=Metric): ");
        // SerialDebug.println(temp_bool);
        if (EE_ReadVariable(VIRT_ADDR_JOG_SYSTEM_ENABLED, &temp_bool) != EE_OK)
        {
            // SerialDebug.println("Error loading Jog System Enabled");
            return false;
        }
        RuntimeConfig::System::jog_system_enabled = (temp_bool != 0); // Stored as 1 for true, 0 for false
        // SerialDebug.print("Loaded Jog System Enabled: ");
        // SerialDebug.println(RuntimeConfig::System::jog_system_enabled);

        if (EE_ReadVariable(VIRT_ADDR_MAX_JOG_SPEED_L, &val_l) != EE_OK)
        {
            // SerialDebug.println("Error loading Max Jog Speed (L)");
            return false;
        }
        if (EE_ReadVariable(VIRT_ADDR_MAX_JOG_SPEED_H, &val_h) != EE_OK)
        {
            // SerialDebug.println("Error loading Max Jog Speed (H)");
            return false;
        }
        converter.u16[0] = val_l;
        converter.u16[1] = val_h;
        RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min = converter.f;
        // SerialDebug.print("Loaded Max Jog Speed: ");
        // SerialDebug.println(RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min);

        uint16_t temp_idx;
        if (EE_ReadVariable(VIRT_ADDR_DEFAULT_JOG_SPEED_INDEX, &temp_idx) != EE_OK)
        {
            // SerialDebug.println("Error loading Default Jog Speed Index");
            return false;
        }
        RuntimeConfig::System::default_jog_speed_index = static_cast<uint8_t>(temp_idx);
        // SerialDebug.print("Loaded Default Jog Speed Index: ");
        // SerialDebug.println(RuntimeConfig::System::default_jog_speed_index);

        // Spindle
        if (EE_ReadVariable(VIRT_ADDR_SPINDLE_CHUCK_TEETH, &RuntimeConfig::Spindle::chuck_pulley_teeth) != EE_OK)
        {
            // SerialDebug.println("Error loading Spindle Chuck Teeth");
            return false;
        }
        // SerialDebug.print("Loaded Spindle Chuck Pulley Teeth: ");
        // SerialDebug.println(RuntimeConfig::Spindle::chuck_pulley_teeth);
        if (EE_ReadVariable(VIRT_ADDR_SPINDLE_ENCODER_TEETH, &RuntimeConfig::Spindle::encoder_pulley_teeth) != EE_OK)
        {
            // SerialDebug.println("Error loading Spindle Encoder Teeth");
            return false;
        }
        // SerialDebug.print("Loaded Spindle Encoder Pulley Teeth: ");
        // SerialDebug.println(RuntimeConfig::Spindle::encoder_pulley_teeth);

        // SerialDebug.println("--- Finished Loading All Settings ---");

        // Clear all dirty flags after successful load
        RuntimeConfigDirtyFlags::Encoder::ppr = false;
        RuntimeConfigDirtyFlags::Encoder::max_rpm = false;
        RuntimeConfigDirtyFlags::Encoder::filter_level = false;
        RuntimeConfigDirtyFlags::Encoder::invert_direction = false;
        RuntimeConfigDirtyFlags::Stepper::microsteps = false;
        RuntimeConfigDirtyFlags::Stepper::max_speed = false;
        RuntimeConfigDirtyFlags::Stepper::invert_direction = false;
        RuntimeConfigDirtyFlags::Stepper::invert_enable = false;
        RuntimeConfigDirtyFlags::Motion::thread_pitch = false;
        RuntimeConfigDirtyFlags::Motion::sync_frequency = false;
        RuntimeConfigDirtyFlags::Motion::sync_enabled = false;
        RuntimeConfigDirtyFlags::Z_Axis::invert_direction = false;
        RuntimeConfigDirtyFlags::Z_Axis::motor_pulley_teeth = false;
        RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pulley_teeth = false;
        RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch = false;
        RuntimeConfigDirtyFlags::Z_Axis::driver_pulses_per_rev = false;
        RuntimeConfigDirtyFlags::Z_Axis::max_feed_rate = false;
        RuntimeConfigDirtyFlags::Z_Axis::max_jog_speed_mm_per_min = false;
        RuntimeConfigDirtyFlags::Z_Axis::acceleration = false;
        RuntimeConfigDirtyFlags::Z_Axis::backlash_compensation = false;
        RuntimeConfigDirtyFlags::Z_Axis::leadscrew_standard_is_metric = false;
        RuntimeConfigDirtyFlags::Z_Axis::enable_polarity_active_high = false;
        RuntimeConfigDirtyFlags::System::measurement_unit_is_metric = false;
        RuntimeConfigDirtyFlags::System::els_default_feed_rate_unit_is_metric = false;
        RuntimeConfigDirtyFlags::System::jog_system_enabled = false;
        RuntimeConfigDirtyFlags::System::default_jog_speed_index = false;
        RuntimeConfigDirtyFlags::Spindle::chuck_pulley_teeth = false;
        RuntimeConfigDirtyFlags::Spindle::encoder_pulley_teeth = false;

        return true; // All variables read successfully
    }

    bool ConfigManager::saveAllSettings()
    {
        // SerialDebug.println("--- Saving All Settings to EEPROM (Dirty Only) ---");

        if (HAL_FLASH_Unlock() != HAL_OK)
        {
            // SerialDebug.println("CRITICAL ERROR: HAL_FLASH_Unlock() failed in saveAllSettings. Settings NOT saved.");
            return false;
        }

        // __disable_irq(); // Disable Interrupts - Removed to prevent freezing

        FloatConverter converter;
        uint16_t temp_bool;
        bool overall_success = true;
        bool any_var_saved = false;

// Modified SAVE_VAR macros to include dirty flag check and clearing
#define SAVE_VAR_IF_DIRTY(addr, val, dirty_flag, type_suffix, description)   \
    if (dirty_flag && overall_success)                                       \
    {                                                                        \
        any_var_saved = true;                                                \
        __disable_irq(); /* Disable IRQ before EEPROM write */               \
        if (EE_WriteVariable(addr, val) != EE_OK)                            \
        {                                                                    \
            __enable_irq(); /* Re-enable IRQ on error */                     \
            /* SerialDebug.print("ERROR saving "); */                        \
            /* SerialDebug.println(description); */                          \
            overall_success = false;                                         \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            __enable_irq(); /* Re-enable IRQ after successful write */       \
            /* SerialDebug.print("Saving "); */                              \
            /* SerialDebug.print(description); */                            \
            /* SerialDebug.print(": "); */                                   \
            /* SerialDebug.println(val); */                                  \
            dirty_flag = false; /* Clear dirty flag after successful save */ \
        }                                                                    \
    }

#define SAVE_VAR_FLOAT_IF_DIRTY(addr_l, addr_h, float_val, dirty_flag, description)                                     \
    if (dirty_flag && overall_success)                                                                                  \
    {                                                                                                                   \
        any_var_saved = true;                                                                                           \
        converter.f = float_val;                                                                                        \
        __disable_irq(); /* Disable IRQ before EEPROM write */                                                          \
        if (EE_WriteVariable(addr_l, converter.u16[0]) != EE_OK || EE_WriteVariable(addr_h, converter.u16[1]) != EE_OK) \
        {                                                                                                               \
            __enable_irq(); /* Re-enable IRQ on error */                                                                \
            /* SerialDebug.print("ERROR saving "); */                                                                   \
            /* SerialDebug.println(description); */                                                                     \
            overall_success = false;                                                                                    \
        }                                                                                                               \
        else                                                                                                            \
        {                                                                                                               \
            __enable_irq(); /* Re-enable IRQ after successful write */                                                  \
            /* SerialDebug.print("Saving "); */                                                                         \
            /* SerialDebug.print(description); */                                                                       \
            /* SerialDebug.print(": "); */                                                                              \
            /* SerialDebug.println(float_val); */                                                                       \
            dirty_flag = false; /* Clear dirty flag */                                                                  \
        }                                                                                                               \
    }

#define SAVE_VAR_U32_IF_DIRTY(addr_l, addr_h, u32_val, dirty_flag, description)                                         \
    if (dirty_flag && overall_success)                                                                                  \
    {                                                                                                                   \
        any_var_saved = true;                                                                                           \
        converter.u32 = u32_val;                                                                                        \
        __disable_irq(); /* Disable IRQ before EEPROM write */                                                          \
        if (EE_WriteVariable(addr_l, converter.u16[0]) != EE_OK || EE_WriteVariable(addr_h, converter.u16[1]) != EE_OK) \
        {                                                                                                               \
            __enable_irq(); /* Re-enable IRQ on error */                                                                \
            /* SerialDebug.print("ERROR saving "); */                                                                   \
            /* SerialDebug.println(description); */                                                                     \
            overall_success = false;                                                                                    \
        }                                                                                                               \
        else                                                                                                            \
        {                                                                                                               \
            __enable_irq(); /* Re-enable IRQ after successful write */                                                  \
            /* SerialDebug.print("Saving "); */                                                                         \
            /* SerialDebug.print(description); */                                                                       \
            /* SerialDebug.print(": "); */                                                                              \
            /* SerialDebug.println(u32_val); */                                                                         \
            dirty_flag = false; /* Clear dirty flag */                                                                  \
        }                                                                                                               \
    }

        // Encoder
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_ENCODER_PPR, RuntimeConfig::Encoder::ppr, RuntimeConfigDirtyFlags::Encoder::ppr, u16, "Encoder PPR");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_ENCODER_MAX_RPM, RuntimeConfig::Encoder::max_rpm, RuntimeConfigDirtyFlags::Encoder::max_rpm, u16, "Encoder Max RPM");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_ENCODER_FILTER, static_cast<uint16_t>(RuntimeConfig::Encoder::filter_level), RuntimeConfigDirtyFlags::Encoder::filter_level, u16, "Encoder Filter");
        temp_bool = RuntimeConfig::Encoder::invert_direction ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_ENCODER_INV_DIR, temp_bool, RuntimeConfigDirtyFlags::Encoder::invert_direction, u16, "Encoder Inv Dir");

        // Stepper
        SAVE_VAR_U32_IF_DIRTY(VIRT_ADDR_STEPPER_MICROSTEPS_L, VIRT_ADDR_STEPPER_MICROSTEPS_H, RuntimeConfig::Stepper::microsteps, RuntimeConfigDirtyFlags::Stepper::microsteps, "Stepper Microsteps");
        SAVE_VAR_U32_IF_DIRTY(VIRT_ADDR_STEPPER_MAX_SPEED_L, VIRT_ADDR_STEPPER_MAX_SPEED_H, RuntimeConfig::Stepper::max_speed, RuntimeConfigDirtyFlags::Stepper::max_speed, "Stepper Max Speed");
        temp_bool = RuntimeConfig::Stepper::invert_direction ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_STEPPER_INV_DIR, temp_bool, RuntimeConfigDirtyFlags::Stepper::invert_direction, u16, "Stepper Inv Dir");
        temp_bool = RuntimeConfig::Stepper::invert_enable ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_STEPPER_INV_ENABLE, temp_bool, RuntimeConfigDirtyFlags::Stepper::invert_enable, u16, "Stepper Inv Enable");

        // Motion
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_MOTION_THREAD_PITCH_L, VIRT_ADDR_MOTION_THREAD_PITCH_H, RuntimeConfig::Motion::thread_pitch, RuntimeConfigDirtyFlags::Motion::thread_pitch, "Motion Thread Pitch");
        SAVE_VAR_U32_IF_DIRTY(VIRT_ADDR_MOTION_SYNC_FREQ_L, VIRT_ADDR_MOTION_SYNC_FREQ_H, RuntimeConfig::Motion::sync_frequency, RuntimeConfigDirtyFlags::Motion::sync_frequency, "Motion Sync Freq");
        temp_bool = RuntimeConfig::Motion::sync_enabled ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_MOTION_SYNC_ENABLED, temp_bool, RuntimeConfigDirtyFlags::Motion::sync_enabled, u16, "Motion Sync Enabled");

        // Z_Axis
        temp_bool = RuntimeConfig::Z_Axis::invert_direction ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_Z_AXIS_INV_DIR, temp_bool, RuntimeConfigDirtyFlags::Z_Axis::invert_direction, u16, "Z-Axis Inv Dir");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_Z_AXIS_MOTOR_TEETH, RuntimeConfig::Z_Axis::motor_pulley_teeth, RuntimeConfigDirtyFlags::Z_Axis::motor_pulley_teeth, u16, "Z-Axis Motor Teeth");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_Z_AXIS_LEAD_SCREW_TEETH, RuntimeConfig::Z_Axis::lead_screw_pulley_teeth, RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pulley_teeth, u16, "Z-Axis Leadscrew Teeth");
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_L, VIRT_ADDR_Z_AXIS_LEAD_SCREW_PITCH_H, RuntimeConfig::Z_Axis::lead_screw_pitch, RuntimeConfigDirtyFlags::Z_Axis::lead_screw_pitch, "Z-Axis Leadscrew Pitch");
        SAVE_VAR_U32_IF_DIRTY(VIRT_ADDR_Z_AXIS_DRIVER_PULSES_L, VIRT_ADDR_Z_AXIS_DRIVER_PULSES_H, RuntimeConfig::Z_Axis::driver_pulses_per_rev, RuntimeConfigDirtyFlags::Z_Axis::driver_pulses_per_rev, "Z-Axis Driver Pulses");
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_L, VIRT_ADDR_Z_AXIS_MAX_FEED_RATE_H, RuntimeConfig::Z_Axis::max_feed_rate, RuntimeConfigDirtyFlags::Z_Axis::max_feed_rate, "Z-Axis Max Feed Rate");
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_Z_AXIS_ACCELERATION_L, VIRT_ADDR_Z_AXIS_ACCELERATION_H, RuntimeConfig::Z_Axis::acceleration, RuntimeConfigDirtyFlags::Z_Axis::acceleration, "Z-Axis Acceleration");
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_Z_AXIS_BACKLASH_L, VIRT_ADDR_Z_AXIS_BACKLASH_H, RuntimeConfig::Z_Axis::backlash_compensation, RuntimeConfigDirtyFlags::Z_Axis::backlash_compensation, "Z-Axis Backlash Comp");
        temp_bool = RuntimeConfig::Z_Axis::leadscrew_standard_is_metric ? 0 : 1;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_Z_AXIS_LS_STD_METRIC, temp_bool, RuntimeConfigDirtyFlags::Z_Axis::leadscrew_standard_is_metric, u16, "Z-Axis LS Std Metric");
        temp_bool = RuntimeConfig::Z_Axis::enable_polarity_active_high ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_Z_AXIS_ENABLE_POL_HIGH, temp_bool, RuntimeConfigDirtyFlags::Z_Axis::enable_polarity_active_high, u16, "Z-Axis Enable Pol High");

        // System
        temp_bool = RuntimeConfig::System::measurement_unit_is_metric ? 0 : 1;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_SYSTEM_UNITS_METRIC, temp_bool, RuntimeConfigDirtyFlags::System::measurement_unit_is_metric, u16, "System Units Metric");
        temp_bool = RuntimeConfig::System::els_default_feed_rate_unit_is_metric ? 0 : 1;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_SYSTEM_ELS_FEED_UNIT_METRIC, temp_bool, RuntimeConfigDirtyFlags::System::els_default_feed_rate_unit_is_metric, u16, "System ELS Feed Unit Metric");
        temp_bool = RuntimeConfig::System::jog_system_enabled ? 1 : 0;
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_JOG_SYSTEM_ENABLED, temp_bool, RuntimeConfigDirtyFlags::System::jog_system_enabled, u16, "Jog System Enabled");
        SAVE_VAR_FLOAT_IF_DIRTY(VIRT_ADDR_MAX_JOG_SPEED_L, VIRT_ADDR_MAX_JOG_SPEED_H, RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min, RuntimeConfigDirtyFlags::Z_Axis::max_jog_speed_mm_per_min, "Max Jog Speed");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_DEFAULT_JOG_SPEED_INDEX, static_cast<uint16_t>(RuntimeConfig::System::default_jog_speed_index), RuntimeConfigDirtyFlags::System::default_jog_speed_index, u16, "Default Jog Speed Index");

        // Spindle
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_SPINDLE_CHUCK_TEETH, RuntimeConfig::Spindle::chuck_pulley_teeth, RuntimeConfigDirtyFlags::Spindle::chuck_pulley_teeth, u16, "Spindle Chuck Teeth");
        SAVE_VAR_IF_DIRTY(VIRT_ADDR_SPINDLE_ENCODER_TEETH, RuntimeConfig::Spindle::encoder_pulley_teeth, RuntimeConfigDirtyFlags::Spindle::encoder_pulley_teeth, u16, "Spindle Encoder Teeth");

// Undefine macros after use to keep them local to this function
#undef SAVE_VAR_IF_DIRTY
#undef SAVE_VAR_FLOAT_IF_DIRTY
#undef SAVE_VAR_U32_IF_DIRTY

        // __enable_irq();   // Re-enable Interrupts - Removed
        HAL_FLASH_Lock(); // Lock Flash

        if (overall_success)
        {
            // SerialDebug.println("--- Finished Saving All Settings ---");
        }
        else
        {
            // SerialDebug.println("--- ERROR During Saving All Settings to EEPROM ---");
        }
        return overall_success;
    }
} // namespace SystemConfig
