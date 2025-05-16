#pragma once

#include <Arduino.h>               // For standard types like uint32_t, bool
#include "Hardware/EncoderTimer.h" // Dependency
#include "Motion/SyncTimer.h"      // Dependency
#include <STM32Step.h>             // Dependency (STM32Step::Stepper)

/**
 * @class MotionControl
 * @brief Central class for managing and coordinating motion based on encoder input and desired operation modes.
 *
 * This class integrates the EncoderTimer, SyncTimer, and STM32Step::Stepper components
 * to provide synchronized motion for operations like threading, turning, and feeding.
 * It handles configuration, mode changes, starting/stopping motion, and status reporting.
 */
class MotionControl
{
public:
    /**
     * @struct MotionPins
     * @brief Defines the GPIO pins used for the stepper motor.
     * These pins are passed to the STM32Step::Stepper instance.
     */
    struct MotionPins
    {
        uint32_t step_pin;   ///< GPIO pin for STEP signal.
        uint32_t dir_pin;    ///< GPIO pin for DIRECTION signal.
        uint32_t enable_pin; ///< GPIO pin for ENABLE signal.
    };

    /**
     * @struct Config
     * @brief Configuration parameters for motion control.
     * Defines pitches, stepper properties, and synchronization frequency.
     */
    struct Config
    {
        float thread_pitch;      ///< Desired effective thread pitch or feed rate (e.g., mm/rev or inch/rev).
        float leadscrew_pitch;   ///< Actual pitch of the machine's leadscrew (e.g., mm or inches).
                                 ///< The ratio thread_pitch/leadscrew_pitch determines electronic gearing.
        uint32_t steps_per_rev;  ///< Full steps per revolution of the stepper motor (e.g., 200).
        uint32_t microsteps;     ///< Microstepping setting for the stepper driver (e.g., 8, 16).
        bool reverse_direction;  ///< If true, reverses the calculated direction of stepper motion.
        uint32_t sync_frequency; ///< Frequency (Hz) at which SyncTimer ISR runs to update stepper commands.
    };

    /**
     * @struct Status
     * @brief Holds current status information for the motion system.
     */
    struct Status
    {
        int32_t encoder_position;  ///< Current raw encoder position (quadrature counts).
        int32_t stepper_position;  ///< Current software-tracked stepper position (microsteps).
        int16_t spindle_rpm;       ///< Current spindle RPM calculated by EncoderTimer.
        bool error;                ///< True if an error state is active.
        const char *error_message; ///< Pointer to a string describing the current error, if any.
    };

    /**
     * @enum Mode
     * @brief Defines the operational modes for motion control.
     */
    enum class Mode
    {
        IDLE,      ///< System is idle, no motion.
        THREADING, ///< Thread cutting mode.
        TURNING,   ///< General turning/feeding mode.
        FEEDING    ///< Potentially a distinct feeding mode (currently maps to TURNING behavior).
    };

    // --- Constructors and Destructor ---
    /**
     * @brief Default constructor.
     * Initializes members to default states. Pins must be set via other means if this is used.
     */
    MotionControl();

    /**
     * @brief Constructor with pin configuration.
     * @param pins A MotionPins struct defining the stepper motor pins.
     */
    explicit MotionControl(const MotionPins &pins);

    /**
     * @brief Destructor. Ensures motion is stopped and resources are released.
     */
    ~MotionControl();

    // --- Basic Operations ---
    /**
     * @brief Initializes all underlying components (EncoderTimer, SyncTimer, Stepper, STM32Step::TimerControl).
     * Must be called before any other operations.
     * @return True if all initializations are successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Stops motion and de-initializes components.
     */
    void end();

    /**
     * @brief Main update function, typically called in the main application loop.
     * Note: With SyncTimer logic moved to its ISR, this function is currently empty
     * but is kept for potential future use (e.g., state management, error checking).
     */
    void update();

    // --- Configuration ---
    /**
     * @brief Sets the motion control configuration.
     * @param config A const reference to the Config struct with new parameters.
     * This updates internal settings and reconfigures the SyncTimer.
     */
    void setConfig(const Config &config);

    /**
     * @brief Gets the current motion control configuration.
     * @return A const reference to the internal Config struct.
     */
    const Config &getConfig() const { return _config; }

    /**
     * @brief Sets the operational mode (e.g., IDLE, THREADING, TURNING).
     * Stops current motion before changing mode and reconfigures components for the new mode.
     * @param mode The desired Mode.
     */
    void setMode(Mode mode);

    // --- Motion Control ---
    /**
     * @brief Starts synchronized motion based on the current mode and configuration.
     * Enables the stepper, encoder, and SyncTimer.
     */
    void startMotion();

    /**
     * @brief Stops any ongoing synchronized motion.
     * Disables SyncTimer and stops the stepper.
     */
    void stopMotion();

    /**
     * @brief Initiates an emergency stop.
     * Disables SyncTimer, commands an emergency stop to the stepper (which disables driver),
     * and sets an error state.
     */
    void emergencyStop();

    // --- Status ---
    /**
     * @brief Gets the current status of the motion control system.
     * @return A Status struct populated with current positions, RPM, and error state.
     */
    Status getStatus() const;

    /**
     * @brief Provides access to the internal Stepper instance.
     * Useful for direct stepper interaction if necessary (e.g., for manual jogging not managed by SyncTimer).
     * @return Pointer to the STM32Step::Stepper instance.
     */
    STM32Step::Stepper *getStepperInstance() { return _stepper; }

private:
    // Components
    EncoderTimer _encoder;        ///< Instance of EncoderTimer for spindle tracking.
    SyncTimer _syncTimer;         ///< Instance of SyncTimer for synchronization logic.
    STM32Step::Stepper *_stepper; ///< Pointer to the Stepper motor object.

    // Configuration
    MotionPins _pins;  ///< Stepper motor pin configuration.
    Config _config;    ///< Current motion parameters.
    Mode _currentMode; ///< Current operational mode.

    // State
    volatile bool _running; ///< True if synchronized motion is active.
    volatile bool _error;   ///< True if an error has occurred.
    const char *_errorMsg;  ///< Descriptive error message.

    // Private methods
    /**
     * @brief Configures stepper and sync parameters based on the selected operational mode.
     * @param mode The Mode to configure for.
     */
    void configureForMode(Mode mode);

    /**
     * @brief Handles error conditions by setting error state and stopping motion.
     * @param msg A descriptive error message string.
     */
    void handleError(const char *msg);

    /**
     * @brief Updates stepper and SyncTimer parameters based on the current _config.
     * Called by setConfig and potentially by setMode.
     */
    void updateSyncParameters();
};
