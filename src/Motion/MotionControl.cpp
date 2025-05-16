#include "Motion/MotionControl.h"
#include "Config/serial_debug.h"   // For SerialDebug if any error prints are kept
#include <STM32Step.h>             // For STM32Step::TimerControl, STM32Step::Stepper
#include "Hardware/EncoderTimer.h" // For EncoderTimer
#include "Motion/SyncTimer.h"      // For SyncTimer

/**
 * @brief Default constructor for MotionControl.
 * Initializes members to default values. Assumes pins will be configured if this constructor is used.
 */
MotionControl::MotionControl() : _stepper(nullptr),
                                 _currentMode(Mode::IDLE),
                                 _running(false),
                                 _error(false),
                                 _errorMsg(nullptr)
{
    // Default constructor: Pins might need to be set via a dedicated method if this is used,
    // or this constructor implies a system where pins are globally defined.
    // For now, _pins remains uninitialized; prefer the constructor with MotionPins.
}

/**
 * @brief Constructor for MotionControl with specified stepper motor pins.
 * @param pins A const reference to a MotionPins struct defining step, direction, and enable pins.
 */
MotionControl::MotionControl(const MotionPins &pins) : _pins(pins),
                                                       _stepper(nullptr),
                                                       _currentMode(Mode::IDLE),
                                                       _running(false),
                                                       _error(false),
                                                       _errorMsg(nullptr)
{
}

/**
 * @brief Destructor for MotionControl.
 * Calls end() to ensure all components are properly stopped and de-initialized.
 */
MotionControl::~MotionControl()
{
    end();
}

/**
 * @brief Initializes all core components required for motion control.
 * This includes initializing the STM32Step::TimerControl (for stepper ISR),
 * the EncoderTimer (_encoder), allocating and initializing the STM32Step::Stepper (_stepper),
 * and initializing the SyncTimer (_syncTimer) with its dependencies.
 * @return True if all initializations are successful, false if any component fails to initialize.
 * @note This method is critical and must be called before any motion operations.
 * The order of initialization (TimerControl for TIM1, then EncoderTimer for TIM2) is important.
 */
bool MotionControl::begin()
{
    // Initialize STM32Step TimerControl (TIM1) for Stepper ISR.
    // This is a static init and should be called once.
    STM32Step::TimerControl::init();
    // Note: We discovered that EncoderTimer (TIM2) should ideally be initialized *before*
    // STM32Step::TimerControl (TIM1) if there are HAL interactions.
    // However, MotionControl owns _encoder, and _encoder.begin() is called next.
    // The global EncoderTimer instance in main.cpp handles the pre-initialization of TIM2.

    // Initialize encoder (_encoder is a member object)
    if (!_encoder.begin())
    {
        handleError("Encoder initialization failed in MotionControl");
        return false;
    }

    // Allocate and initialize stepper motor object
    _stepper = new STM32Step::Stepper(_pins.step_pin, _pins.dir_pin, _pins.enable_pin);
    if (!_stepper)
    {
        handleError("Stepper motor allocation failed in MotionControl");
        return false;
    }
    // Note: Stepper constructor calls its initPins(). Enable/disable state is managed by MotionControl.

    // Initialize sync timer (_syncTimer is a member object)
    // Pass references to the already initialized _encoder and _stepper.
    if (!_syncTimer.begin(&_encoder, _stepper))
    {
        handleError("Sync timer initialization failed in MotionControl");
        return false;
    }

    _error = false; // Clear any error state if all initializations passed
    return true;
}

/**
 * @brief Stops any ongoing motion and de-initializes all components.
 * Disables the stepper, and ends EncoderTimer and SyncTimer operations.
 */
void MotionControl::end()
{
    stopMotion(); // Ensure motion is stopped first
    if (_stepper)
    {
        _stepper->disable(); // Disable the motor driver
        delete _stepper;     // Free the stepper object
        _stepper = nullptr;
    }
    _encoder.end();   // De-initialize encoder timer
    _syncTimer.end(); // De-initialize sync timer
    _running = false;
    _error = false;
}

/**
 * @brief Sets the operational mode of the MotionControl system.
 * If the new mode is different from the current mode, it stops any ongoing motion,
 * updates the internal mode, and then configures components for the new mode.
 * @param mode The desired Mode (IDLE, THREADING, TURNING, FEEDING).
 */
void MotionControl::setMode(Mode mode)
{
    if (mode == _currentMode && _running) // No change if mode is same and already running in it
    {
        return;
    }

    stopMotion(); // Stop current motion before changing mode
    _currentMode = mode;
    configureForMode(mode); // Apply mode-specific configurations
}

/**
 * @brief Sets the main configuration parameters for motion control.
 * Updates the internal _config member and calls updateSyncParameters to apply
 * relevant settings to the SyncTimer and Stepper.
 * @param config A const reference to the Config struct containing new parameters.
 */
void MotionControl::setConfig(const Config &config)
{
    _config = config;
    updateSyncParameters(); // Apply changes to dependent components
}

/**
 * @brief Starts synchronized motion.
 * This function configures the SyncTimer with parameters from the current _config,
 * resets the encoder, enables the stepper motor driver, and enables the SyncTimer ISR.
 * Sets the system state to running.
 */
void MotionControl::startMotion()
{
    if (_running || _error) // Do not start if already running or in an error state
    {
        return;
    }

    if (!_stepper || !_encoder.isValid() || !_syncTimer.isInitialized())
    {
        handleError("Cannot start motion: Components not initialized.");
        return;
    }

    // Configure sync timer with current motion parameters
    SyncTimer::SyncConfig syncConfig;
    syncConfig.thread_pitch = _config.thread_pitch;
    syncConfig.leadscrew_pitch = _config.leadscrew_pitch;
    syncConfig.update_freq = _config.sync_frequency;
    syncConfig.reverse_direction = _config.reverse_direction;
    _syncTimer.setConfig(syncConfig);

    // Prepare and start components
    _encoder.reset();        // Reset encoder count to start fresh
    _stepper->enable();      // Enable the stepper motor driver
    _syncTimer.enable(true); // Enable the SyncTimer ISR processing

    _running = true;
    _error = false; // Clear any previous error on successful start
}

/**
 * @brief Stops any ongoing synchronized motion.
 * Disables the SyncTimer ISR and stops the stepper motor.
 * Sets the system state to not running.
 */
void MotionControl::stopMotion()
{
    if (!_running) // Only act if currently running
    {
        return;
    }

    _syncTimer.enable(false); // Disable SyncTimer ISR
    if (_stepper)
    {
        _stepper->stop(); // Stop the stepper motor (pauses STM32Step::TimerControl)
    }
    _running = false;
}

/**
 * @brief Initiates an emergency stop.
 * Disables the SyncTimer ISR, commands an emergency stop to the stepper (which also
 * disables the driver), sets an error state, and logs the error.
 */
void MotionControl::emergencyStop()
{
    _syncTimer.enable(false); // Disable SyncTimer immediately
    if (_stepper)
    {
        _stepper->emergencyStop(); // Commands stepper to stop and disable driver
    }
    _running = false;
    handleError("Emergency stop triggered");
}

/**
 * @brief Retrieves the current status of the motion control system.
 * @return A MotionControl::Status struct populated with encoder position, stepper position,
 * spindle RPM, and error information.
 */
MotionControl::Status MotionControl::getStatus() const
{
    Status status;
    // Use const_cast if EncoderTimer methods like getCount/getRPM are not const,
    // but they should be if they only read status.
    // Assuming EncoderTimer::getPosition() is const and provides all needed data.
    EncoderTimer::Position encPos = _encoder.getPosition();
    status.encoder_position = encPos.count;
    status.spindle_rpm = encPos.rpm;

    status.stepper_position = _stepper ? _stepper->getCurrentPosition() : 0;
    status.error = _error;
    status.error_message = _errorMsg;
    return status;
}

/**
 * @brief Configures the stepper motor's operation mode based on the MotionControl mode.
 * Also calls updateSyncParameters to ensure sync logic matches.
 * @param mode The MotionControl::Mode to configure for.
 */
void MotionControl::configureForMode(Mode mode)
{
    if (!_stepper)
        return;

    switch (mode)
    {
    case Mode::THREADING:
        _stepper->setOperationMode(STM32Step::OperationMode::THREADING);
        break;
    case Mode::TURNING:
    case Mode::FEEDING: // FEEDING currently uses TURNING behavior
        _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
        break;
    case Mode::IDLE:
    default:
        _stepper->setOperationMode(STM32Step::OperationMode::IDLE);
        break;
    }
    updateSyncParameters(); // Re-apply sync parameters relevant to the new mode or config
}

/**
 * @brief Internal error handling function.
 * Sets the error state, stores the error message, and stops motion.
 * @param msg A C-string describing the error.
 */
void MotionControl::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
    // SerialDebug.print("MotionControl ERROR: "); SerialDebug.println(msg); // Optional immediate print
    stopMotion(); // Stop all motion on error
}

/**
 * @brief Updates parameters of the Stepper and SyncTimer based on the current _config.
 * This is called when _config changes or when mode changes might imply different sync needs.
 */
void MotionControl::updateSyncParameters()
{
    if (!_stepper) // Ensure stepper exists
        return;

    // Update stepper's understanding of microsteps (from _config)
    _stepper->setMicrosteps(_config.microsteps);

    // If motion is active, re-configure SyncTimer with current _config parameters.
    // If not running, SyncTimer will pick up new config when startMotion is called.
    if (_running)
    {
        SyncTimer::SyncConfig syncConfig;
        syncConfig.thread_pitch = _config.thread_pitch;
        syncConfig.leadscrew_pitch = _config.leadscrew_pitch;
        syncConfig.update_freq = _config.sync_frequency;
        syncConfig.reverse_direction = _config.reverse_direction;
        _syncTimer.setConfig(syncConfig);
    }
}

/**
 * @brief Main update loop for MotionControl.
 * With SyncTimer logic moved to its ISR, this function is currently not performing
 * active periodic tasks. It's retained for potential future use, such as managing
 * state transitions or periodic checks not suitable for an ISR.
 */
void MotionControl::update()
{
    // SyncTimer logic (processSyncRequest) has been moved into SyncTimer::handleInterrupt() (ISR).
    // Therefore, MotionControl::update() no longer needs to call it.
    // This method can be used for other non-ISR-critical periodic tasks related to MotionControl state
    // if they arise. For now, it's effectively a no-op if _running and no _error.
    if (_running && !_error)
    {
        // Placeholder for any future non-ISR update logic
    }
}
