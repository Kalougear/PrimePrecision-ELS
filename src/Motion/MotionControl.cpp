#include "Motion/MotionControl.h"
#include "Config/serial_debug.h"   // For SerialDebug if any error prints are kept
#include "Config/SystemConfig.h"   // For SystemConfig::RuntimeConfig
#include <STM32Step.h>             // For STM32Step::TimerControl, STM32Step::Stepper
#include "Hardware/EncoderTimer.h" // For EncoderTimer
#include "Motion/SyncTimer.h"      // For SyncTimer
#include <cmath>                   // For roundf, fabsf

/**
 * @brief Default constructor for MotionControl.
 * Initializes members to default values. Assumes pins will be configured if this constructor is used.
 */
MotionControl::MotionControl() : _stepper(nullptr),
                                 _currentMode(Mode::IDLE),
                                 _running(false),
                                 _jogActive(false), // Initialize _jogActive
                                 _error(false),
                                 _errorMsg(nullptr),
                                 _currentFeedDirection(FeedDirection::UNKNOWN),
                                 _targetStopFeatureEnabledForMotion(false),
                                 _absoluteTargetStopStepsForMotion(0),
                                 _targetStopReached(false)
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
                                                       _jogActive(false), // Initialize _jogActive
                                                       _error(false),
                                                       _errorMsg(nullptr),
                                                       _currentFeedDirection(FeedDirection::UNKNOWN),
                                                       _targetStopFeatureEnabledForMotion(false),
                                                       _absoluteTargetStopStepsForMotion(0),
                                                       _targetStopReached(false)
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
    SerialDebug.println("MC::begin() - SUCCESSFUL.");
    return true;
}

/**
 * @brief Stops any ongoing motion and de-initializes all components.
 * Disables the stepper, and ends EncoderTimer and SyncTimer operations.
 */
void MotionControl::end()
{
    stopMotion(); // Ensure motion is stopped first
    if (_jogActive && _stepper)
    { // If a jog was active, ensure it's properly stopped
        _stepper->stop();
        _jogActive = false;
    }
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
    if (_jogActive && _stepper)
    { // If a jog is active, stop it before changing mode
        endContinuousJog();
    }
    if (mode == _currentMode && _running) // No change if mode is same and already running in it
    {
        return;
    }

    stopMotion(); // Stop current ELS motion before changing mode
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
    SerialDebug.println("MC::setConfig called.");
    bool was_running = _running;

    if (was_running)
    {
        SerialDebug.println("MC::setConfig - ELS was running, stopping motion before reconfig.");
        stopMotion(); // Stop all ELS motion (SyncTimer, Stepper/TimerControl)
    }

    _config = config;
    updateSyncParameters(); // This will call the robust SyncTimer::setConfig

    if (was_running)
    {
        SerialDebug.println("MC::setConfig - ELS was running, restarting motion with new config.");
        startMotion(); // Restart ELS motion with new config
    }
}

/**
 * @brief Starts synchronized motion.
 * This function configures the SyncTimer with parameters from the current _config,
 * resets the encoder, enables the stepper motor driver, and enables the SyncTimer ISR.
 * Sets the system state to running.
 */
void MotionControl::startMotion()
{
    SerialDebug.println("MC::startMotion() - CALLED.");
    if (_jogActive && _stepper)
    { // If a jog is active, stop it before starting ELS motion
        endContinuousJog();
    }
    if (_running || _error) // Do not start if already running or in an error state
    {
        SerialDebug.print("MC::startMotion - Guarded: _running=");
        SerialDebug.print(_running);
        SerialDebug.print(", _error=");
        SerialDebug.println(_error);
        return;
    }
    SerialDebug.print("MC::startMotion - Before component check. _stepper: ");
    SerialDebug.print(_stepper ? "VALID" : "NULL");
    SerialDebug.print(", _encoder.isValid(): ");
    SerialDebug.print(_encoder.isValid() ? "VALID" : "INVALID");
    SerialDebug.print(", _syncTimer.isInitialized(): ");
    SerialDebug.println(_syncTimer.isInitialized() ? "VALID" : "INVALID");

    // Ensure stepper's pulse generation is stopped before reconfiguring.
    // stopMotion() should have already called _stepper->stop(), but this is an extra measure for robustness.
    if (_stepper)
    {
        _stepper->stop();
    }

    if (!_stepper || !_encoder.isValid() || !_syncTimer.isInitialized())
    {
        SerialDebug.println("MC::startMotion - Error: Components not initialized.");
        handleError("Cannot start motion: Components not initialized.");
        return;
    }

    // Configure sync timer with current motion parameters using the new helper function
    calculateAndSetSyncTimerConfig();

    // Prepare and start components
    _encoder.reset();        // Reset encoder count to start fresh
    _stepper->enable();      // Enable the stepper motor driver
    _syncTimer.enable(true); // Enable the SyncTimer ISR processing

    // HAL_Delay(1); // Removed this delay as it didn't solve the issue and adds overhead.

    // Determine and set _currentFeedDirection based on effective motion
    // This is crucial for the auto-stop logic in update()
    // This uses the MotionControl's _config which has been set by TurningMode
    // and is used by calculateAndSetSyncTimerConfig().
    bool steps_will_increase = (_config.thread_pitch >= 0.0f) ^ _config.reverse_direction;
    // XOR logic:
    // thread_pitch   | reverse_dir | result (steps_increase)
    // POS (true)     | false       | true  (e.g. pitch 0.1, not reversed -> steps increase)
    // POS (true)     | true        | false (e.g. pitch 0.1, reversed -> steps decrease)
    // NEG (false)    | false       | false (e.g. pitch -0.1, not reversed -> steps decrease)
    // NEG (false)    | true        | true  (e.g. pitch -0.1, reversed -> steps increase)

    if (steps_will_increase)
    {
        _currentFeedDirection = FeedDirection::AWAY_FROM_CHUCK; // Assuming AWAY_FROM_CHUCK means positive step increment
    }
    else
    {
        _currentFeedDirection = FeedDirection::TOWARDS_CHUCK; // Assuming TOWARDS_CHUCK means negative step increment
    }
    SerialDebug.print("MC::startMotion - Effective _currentFeedDirection set to: ");
    SerialDebug.println((_currentFeedDirection == FeedDirection::AWAY_FROM_CHUCK) ? "AWAY_FROM_CHUCK (steps increasing)" : "TOWARDS_CHUCK (steps decreasing)");

    SerialDebug.println("MC::startMotion() - Components enabled, _running will be set to true.");
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
    if (!_running) // Only act if ELS currently running
    {
        // If only a jog was active, endContinuousJog should have been called.
        // This ensures _running is only for ELS.
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
    if (_jogActive && _stepper)
    {
        _stepper->emergencyStop(); // Stop jog first
        _jogActive = false;
    }
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
    EncoderTimer::Position encPos = _encoder.getPosition();
    status.encoder_position = encPos.count;

    float encoderRpm = static_cast<float>(encPos.rpm);
    float chuckPulleyTeeth = static_cast<float>(SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth);
    float encoderPulleyTeeth = static_cast<float>(SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth);
    float actualSpindleRpm = 0.0f;

    if (chuckPulleyTeeth > 0.00001f)
    {
        actualSpindleRpm = encoderRpm * (encoderPulleyTeeth / chuckPulleyTeeth);
    }
    else
    {
        actualSpindleRpm = encoderRpm;
    }
    status.spindle_rpm = static_cast<int16_t>(roundf(actualSpindleRpm)); // Changed to int16_t

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
    case Mode::FEEDING:
        _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
        break;
    case Mode::IDLE:
    default:
        _stepper->setOperationMode(STM32Step::OperationMode::IDLE);
        break;
    }
    // Only update sync parameters if not in jog mode, as jog has its own speed control
    if (!_jogActive)
    {
        updateSyncParameters();
    }
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
    if (_jogActive && _stepper)
    {
        _stepper->stop();
        _jogActive = false;
    }
    stopMotion();
}

/**
 * @brief Updates parameters of the Stepper and SyncTimer based on the current _config.
 * This is called when _config changes or when mode changes might imply different sync needs.
 */
void MotionControl::updateSyncParameters()
{
    if (!_stepper)
        return;

    _stepper->setMicrosteps(_config.microsteps);
    calculateAndSetSyncTimerConfig();
}

/**
 * @brief Main update loop for MotionControl.
 */
void MotionControl::update()
{
    if (_running && !_error && !_jogActive) // ELS mode is running
    {
        // ELS logic is ISR driven by SyncTimer
    }
    else if (_jogActive && !_error) // Jog mode is running
    {
        // Continuous jog motion is now handled by STM32Step::Stepper::ISR when _isContinuousMode is true.
        // No explicit periodic calls needed from MotionControl::update() for step generation.
        // MotionControl::beginContinuousJog sets it up, MotionControl::endContinuousJog stops it.
    }

    // Check for auto-stop condition if ELS is running and feature is enabled
    if (_running && _targetStopFeatureEnabledForMotion && !_targetStopReached)
    {
        if (!_stepper)
            return;
        int32_t currentAbsoluteSteps = _stepper->getCurrentPosition(); // Assuming this gives absolute steps

        // Determine effective direction for comparison based on _currentFeedDirection set in startMotion()
        bool movingTowardsIncreasingSteps = (_currentFeedDirection == FeedDirection::AWAY_FROM_CHUCK);

        if (movingTowardsIncreasingSteps) // Moving towards increasing step counts
        {
            if (currentAbsoluteSteps >= _absoluteTargetStopStepsForMotion)
            {
                requestImmediateStop(StopType::CONTROLLED_DECELERATION);
                _targetStopReached = true;
                _targetStopFeatureEnabledForMotion = false; // Disarm after triggering
                SerialDebug.println("MC::update - Auto-stop triggered (positive direction).");
            }
        }
        else // Moving towards decreasing step counts (i.e., _currentFeedDirection == FeedDirection::TOWARDS_CHUCK)
        {
            if (currentAbsoluteSteps <= _absoluteTargetStopStepsForMotion)
            {
                requestImmediateStop(StopType::CONTROLLED_DECELERATION);
                _targetStopReached = true;
                _targetStopFeatureEnabledForMotion = false; // Disarm after triggering
                SerialDebug.println("MC::update - Auto-stop triggered (negative direction).");
            }
        }
    }
}

void MotionControl::calculateAndSetSyncTimerConfig()
{
    if (!_stepper || !_syncTimer.isInitialized())
    {
        return;
    }
    double target_feed_mm_spindle_rev = static_cast<double>(_config.thread_pitch);
    double enc_ppr = static_cast<double>(SystemConfig::RuntimeConfig::Encoder::ppr);
    double quad_mult = static_cast<double>(SystemConfig::Limits::Encoder::QUADRATURE_MULT);
    double spindle_pulley_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth);
    double encoder_pulley_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth);

    SerialDebug.println("---- MC::calculateAndSetSyncTimerConfig ----");
    SerialDebug.print("Input: target_feed_mm_spindle_rev (_config.thread_pitch): ");
    SerialDebug.println(target_feed_mm_spindle_rev, 6);

    SerialDebug.println("-- SystemConfig Parameters Read --");
    SerialDebug.print("Encoder::ppr (enc_ppr): ");
    SerialDebug.println(enc_ppr);
    SerialDebug.print("Encoder::QUADRATURE_MULT (quad_mult): ");
    SerialDebug.println(quad_mult);
    SerialDebug.print("Spindle::chuck_pulley_teeth (spindle_pulley_teeth): ");
    SerialDebug.println(spindle_pulley_teeth);
    SerialDebug.print("Spindle::encoder_pulley_teeth (encoder_pulley_teeth): ");
    SerialDebug.println(encoder_pulley_teeth);
    SerialDebug.print("Z_Axis::driver_pulses_per_rev: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    SerialDebug.print("Z_Axis::motor_pulley_teeth: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    SerialDebug.print("Z_Axis::lead_screw_pulley_teeth: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    SerialDebug.print("Z_Axis::lead_screw_pitch (ls_pitch_val): ");
    SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch);
    SerialDebug.print("Z_Axis::leadscrew_standard_is_metric: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric ? "METRIC" : "IMPERIAL (TPI)");

    SerialDebug.println("-- Initial Provided Debug Prints --");
    // Debug print for sync calculation inputs
    SerialDebug.print("MC::calcSync: target_feed=");
    SerialDebug.print(target_feed_mm_spindle_rev);
    SerialDebug.print(", encPPR=");
    SerialDebug.print(enc_ppr);
    SerialDebug.print(", encTeeth=");
    SerialDebug.print(encoder_pulley_teeth);
    SerialDebug.print(", chuckTeeth=");
    SerialDebug.println(spindle_pulley_teeth);
    // Add more prints for intermediate values if needed

    double enc_counts_per_spindle_rev = 1.0;
    if (spindle_pulley_teeth > 0.000001 && encoder_pulley_teeth > 0.000001 && enc_ppr > 0.000001)
    {
        enc_counts_per_spindle_rev = (enc_ppr * quad_mult) * (encoder_pulley_teeth / spindle_pulley_teeth);
    }
    else if (enc_ppr > 0.000001)
    {
        enc_counts_per_spindle_rev = enc_ppr * quad_mult;
    }
    if (std::abs(enc_counts_per_spindle_rev) < 0.000001)
        enc_counts_per_spindle_rev = 1.0;
    double s_motor_total_usteps = static_cast<double>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    if (std::abs(s_motor_total_usteps) < 0.000001)
        s_motor_total_usteps = 1.0;
    double motor_pulley_z_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    double leadscrew_pulley_z_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    double G_ls_rev_per_motor_rev = 1.0;
    if (leadscrew_pulley_z_teeth > 0.000001 && motor_pulley_z_teeth > 0.000001)
    {
        G_ls_rev_per_motor_rev = motor_pulley_z_teeth / leadscrew_pulley_z_teeth;
    }
    if (std::abs(G_ls_rev_per_motor_rev) < 0.000001 && (leadscrew_pulley_z_teeth > 0.000001 || motor_pulley_z_teeth > 0.000001))
    {
        G_ls_rev_per_motor_rev = 1.0;
    }
    double ls_pitch_val = static_cast<double>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch);
    bool ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    // Corrected TPI to mm/rev conversion: (1.0 / TPI_value) * 25.4
    double ls_pitch_mm_per_ls_rev = ls_is_metric ? ls_pitch_val : ((ls_pitch_val == 0) ? 1.0 : (25.4 / ls_pitch_val));
    if (std::abs(ls_pitch_mm_per_ls_rev) < 0.000001) // Avoid div by zero if ls_pitch_val was 0 and then corrected
        ls_pitch_mm_per_ls_rev = 1.0;

    SerialDebug.println("-- Intermediate Calculated Values --");
    SerialDebug.print("enc_counts_per_spindle_rev: ");
    SerialDebug.println(enc_counts_per_spindle_rev, 6);
    SerialDebug.print("s_motor_total_usteps: ");
    SerialDebug.println(s_motor_total_usteps);
    SerialDebug.print("G_ls_rev_per_motor_rev (Leadscrew rev per Z-motor rev): ");
    SerialDebug.println(G_ls_rev_per_motor_rev, 6);
    SerialDebug.print("ls_pitch_mm_per_ls_rev (Leadscrew pitch in mm/rev): ");
    SerialDebug.println(ls_pitch_mm_per_ls_rev, 6);

    double mm_travel_per_motor_rev = G_ls_rev_per_motor_rev * ls_pitch_mm_per_ls_rev;
    if (std::abs(mm_travel_per_motor_rev) < 0.000001) // Avoid div by zero
        mm_travel_per_motor_rev = 1.0;
    SerialDebug.print("mm_travel_per_motor_rev (Carriage travel in mm per Z-motor rev): ");
    SerialDebug.println(mm_travel_per_motor_rev, 6);

    double usteps_per_mm_travel = s_motor_total_usteps / mm_travel_per_motor_rev;
    SerialDebug.print("usteps_per_mm_travel (Stepper microsteps per mm of carriage travel): ");
    SerialDebug.println(usteps_per_mm_travel, 6);

    double mm_travel_per_enc_count = target_feed_mm_spindle_rev / enc_counts_per_spindle_rev;
    SerialDebug.print("mm_travel_per_enc_count (Carriage travel in mm per encoder quadrature tick): ");
    SerialDebug.println(mm_travel_per_enc_count, 9);

    double calculated_steps_per_encoder_tick = mm_travel_per_enc_count * usteps_per_mm_travel;
    SerialDebug.print("FINAL: calculated_steps_per_encoder_tick: ");
    SerialDebug.println(calculated_steps_per_encoder_tick, 9);
    SerialDebug.println("--------------------------------------");

    SyncTimer::SyncConfig newSyncTimerConfig;
    newSyncTimerConfig.steps_per_encoder_tick = calculated_steps_per_encoder_tick;

    // The line "SyncTimer::SyncConfig newSyncTimerConfig;" is already declared above this replaced block.
    // The line "newSyncTimerConfig.steps_per_encoder_tick = calculated_steps_per_encoder_tick;" is also already done.

    // For this test, let's force SyncTimer frequency to 50kHz for software bit-banging
    // User should ensure SystemConfig::Limits::Stepper::PULSE_WIDTH_US is appropriate (e.g., 5us)
    // MAX_STEP_FREQ_HZ is less critical for bit-banged ELS steps but set to 20kHz.
    // Forcing to 5kHz for this test to reduce step command rate.
    newSyncTimerConfig.update_freq = 5000; // Changed from 50000 to 5000
    SerialDebug.print("MC::calcAndSetSyncTimerCfg - DEBUG: Forcing SyncTimer update_freq for Bit-Bang Test to: ");
    SerialDebug.println(newSyncTimerConfig.update_freq);

    newSyncTimerConfig.reverse_direction = false;
    _syncTimer.setConfig(newSyncTimerConfig);
}

// --- Motor Enable/Disable Methods ---
bool MotionControl::isMotorEnabled() const
{
    if (_stepper)
    {
        return _stepper->isEnabled();
    }
    return false;
}

void MotionControl::enableMotor()
{
    if (!_stepper)
    {
        return;
    }
    _stepper->enable();
    SerialDebug.println("MotionControl: Motor enabled via enableMotor().");

    // If current mode is an ELS mode (and not IDLE), restart ELS motion.
    // Check _running == false to ensure we are not already trying to run ELS
    // (e.g. if enableMotor was called during an active ELS for some reason, though disableMotor should stop it)
    if (!_running && (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING))
    {
        SerialDebug.print("MotionControl: enableMotor - Re-engaging ELS mode: ");
        SerialDebug.println(static_cast<int>(_currentMode));
        startMotion(); // This will re-initialize sync and start ISRs
    }
    else if (_running)
    {
        SerialDebug.println("MotionControl: enableMotor - ELS was already running or attempting to run. No action taken to restart.");
    }
    else
    {
        SerialDebug.println("MotionControl: enableMotor - Current mode is IDLE or not an ELS mode. ELS not re-engaged.");
    }
}

void MotionControl::disableMotor()
{
    if (_jogActive && _stepper)
    {
        endContinuousJog(); // Stop jog if active
    }
    stopMotion(); // Stop ELS motion
    if (_stepper)
    {
        _stepper->disable();
        SerialDebug.println("MotionControl: Motor disabled via disableMotor().");
    }
}

// --- Jogging Control Implementations ---

/**
 * @brief Begins continuous jogging motion in the specified direction and speed.
 * Overrides any ELS-synchronized motion. Disables SyncTimer.
 * @param direction The JogDirection (TOWARDS_CHUCK or AWAY_FROM_CHUCK).
 * @param speed_mm_per_min The desired jog speed in mm/minute. Speed will be capped by system limits.
 */
void MotionControl::beginContinuousJog(JogDirection direction, float speed_mm_per_min)
{
    SerialDebug.print("MC::beginContinuousJog - Entry. Received Direction Enum: ");
    SerialDebug.print(static_cast<int>(direction)); // Log the enum value
    SerialDebug.print(" (1=TOWARDS_CHUCK, 2=AWAY_FROM_CHUCK). Current jog_system_enabled: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::System::jog_system_enabled ? "TRUE" : "FALSE");

    if (_error || !_stepper)
    {
        SerialDebug.println("MC::beginContinuousJog - Error or no stepper.");
        return;
    }

    if (!SystemConfig::RuntimeConfig::System::jog_system_enabled)
    {
        SerialDebug.println("MC::beginContinuousJog - Jog system is disabled.");
        if (_jogActive)
        {
            _stepper->stop();
            _jogActive = false;
        }
        return;
    }

    // Status currentStatus = getStatus();
    // if (currentStatus.spindle_rpm != 0)
    // {
    //     SerialDebug.print("MC::beginContinuousJog - Spindle RPM is ");
    //     SerialDebug.print(currentStatus.spindle_rpm);
    //     SerialDebug.println(". Jogging inhibited (RPM CHECK TEMPORARILY DISABLED FOR DEBUG).");
    //     if (_jogActive)
    //     {
    //         _stepper->stop();
    //         _jogActive = false;
    //     }
    //     return;
    // }
    SerialDebug.println("MC::beginContinuousJog - RPM check temporarily disabled for debugging.");

    if (_running)
    {
        stopMotion();
    }
    _syncTimer.enable(false);

    if (direction == JogDirection::JOG_NONE)
    {
        endContinuousJog();
        return;
    }

    // Calculate microstep multiplier from driver_pulses_per_rev and motor steps_per_rev
    uint32_t driver_ppr = SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev;
    uint16_t motor_spr = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    uint32_t microstep_multiplier = 1; // Default to 1 (full step) if calculation is problematic
    if (motor_spr > 0 && driver_ppr >= motor_spr)
    {
        microstep_multiplier = driver_ppr / motor_spr;
    }
    else
    {
        SerialDebug.println("MC::beginContinuousJog - WARNING: Invalid motor_spr or driver_ppr for microstep calculation. Defaulting to 1x.");
    }
    if (microstep_multiplier == 0)
        microstep_multiplier = 1; // Should not happen if driver_ppr >= motor_spr

    _stepper->setMicrosteps(microstep_multiplier);
    SerialDebug.print("MC::beginContinuousJog - Driver Pulses/Rev: ");
    SerialDebug.print(driver_ppr);
    SerialDebug.print(", Motor Steps/Rev: ");
    SerialDebug.print(motor_spr);
    SerialDebug.print(" -> Calculated & Set Microstep Multiplier to: ");
    SerialDebug.println(microstep_multiplier);

    _stepper->enable();
    SerialDebug.print("MC::beginContinuousJog - Called _stepper->enable(). Is enabled: ");
    SerialDebug.println(_stepper->isEnabled() ? "YES" : "NO");

    float target_speed_mm_per_min = speed_mm_per_min;
    if (target_speed_mm_per_min < 0.0f)
        target_speed_mm_per_min = 0.0f; // Speed must be positive
    if (target_speed_mm_per_min > SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min)
    {
        target_speed_mm_per_min = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
    }

    // Calculate steps per mm for Z-axis
    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f; // Avoid div by zero
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f; // Avoid div by zero

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : z_ls_pitch_val * 25.4f;
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        z_ls_pitch_mm_per_ls_rev = 1.0f;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        z_mm_travel_per_motor_rev = 1.0f;

    float z_usteps_per_mm_travel = z_motor_total_usteps / z_mm_travel_per_motor_rev;

    // Convert speed from mm/min to steps/sec (Hz)
    float speed_mm_per_sec = target_speed_mm_per_min / 60.0f;
    float target_freq_hz = speed_mm_per_sec * z_usteps_per_mm_travel;

    // Convert acceleration from mm/s^2 to steps/s^2
    float accel_mm_per_s2 = SystemConfig::RuntimeConfig::Z_Axis::acceleration;
    float accel_steps_per_s2 = accel_mm_per_s2 * z_usteps_per_mm_travel;

    // Determine stepper library direction (true for one way, false for other)
    // This needs to map to how STM32Step::runContinuous interprets its boolean direction
    // Let's assume: runContinuous(true) means "positive" direction according to its internal logic.
    // And JOG_AWAY_FROM_CHUCK is conceptually positive Z.
    SerialDebug.print("MC::beginContinuousJog - Comparing direction (");
    SerialDebug.print(static_cast<int>(direction));
    SerialDebug.print(") == JOG_AWAY_FROM_CHUCK (");
    SerialDebug.print(static_cast<int>(JogDirection::JOG_AWAY_FROM_CHUCK));
    SerialDebug.println(")");

    bool stepper_lib_direction = (direction == JogDirection::JOG_AWAY_FROM_CHUCK);
    SerialDebug.print("MC::beginContinuousJog - Calculated stepper_lib_direction (for Stepper::runContinuous): ");
    SerialDebug.println(stepper_lib_direction ? "true (AWAY)" : "false (TOWARDS)");
    // The Stepper::ISR will handle the actual Z_Axis::invert_direction logic.

    _stepper->setAcceleration(accel_steps_per_s2);
    _stepper->setSpeedHz(target_freq_hz);

    SerialDebug.print("MotionControl: Just before _stepper->runContinuous. Stepper enabled: ");
    SerialDebug.print(_stepper->isEnabled() ? "YES" : "NO");
    SerialDebug.print(". Target Freq: ");
    SerialDebug.print(target_freq_hz);
    SerialDebug.print(" Hz. Accel: ");
    SerialDebug.print(accel_steps_per_s2);
    SerialDebug.print(" steps/s^2. StepperLibDir: ");
    SerialDebug.println(stepper_lib_direction);

    _stepper->runContinuous(stepper_lib_direction);

    SerialDebug.print("MotionControl: beginContinuousJog - Dir: ");
    SerialDebug.print(static_cast<int>(direction));
    SerialDebug.print(" (StepperLibDir: ");
    SerialDebug.print(stepper_lib_direction);
    SerialDebug.print(")");
    SerialDebug.print(", ReqSpeed: ");
    SerialDebug.print(speed_mm_per_min);
    SerialDebug.print("mm/min, TargetFreq: ");
    SerialDebug.print(target_freq_hz);
    SerialDebug.print("Hz, Accel: ");
    SerialDebug.print(accel_steps_per_s2);
    SerialDebug.println("steps/s^2");

    _jogActive = true;
}

/**
 * @brief Stops any active continuous jogging motion.
 */
void MotionControl::endContinuousJog()
{
    if (!_jogActive || !_stepper)
    {
        return;
    }

    _stepper->stop();
    _jogActive = false;
    SerialDebug.println("MotionControl: endContinuousJog - Stepper stopped, _jogActive=false.");

    // Re-engage ELS mode if it was the active mode before jog
    if (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING)
    {
        SerialDebug.print("MotionControl: endContinuousJog - Re-engaging ELS mode: ");
        SerialDebug.println(static_cast<int>(_currentMode));
        // startMotion() re-calculates SyncTimer config, resets encoder, enables stepper, enables SyncTimer ISR
        startMotion();
    }
    else
    {
        SerialDebug.println("MotionControl: endContinuousJog - No ELS mode to re-engage or current mode is IDLE.");
        // If not returning to an ELS mode, ensure stepper is explicitly disabled if desired.
        // However, a global motor enable/disable button should ideally handle this.
        // For now, if it was IDLE, it remains IDLE and stepper is stopped.
        // If motor was enabled before jog, it remains enabled but stopped.
    }
}

// --- Auto-Stop Feature Control Implementations ---

/**
 * @brief Configures the absolute target step position for the auto-stop feature.
 * @param absoluteSteps The target position in absolute machine steps.
 * @param enable True to enable the auto-stop feature with this target, false to disable.
 */
void MotionControl::configureAbsoluteTargetStop(int32_t absoluteSteps, bool enable)
{
    _absoluteTargetStopStepsForMotion = absoluteSteps;
    _targetStopFeatureEnabledForMotion = enable;
    _targetStopReached = false; // Reset flag when new target is set or feature is toggled

    if (enable)
    {
        SerialDebug.print("MC::configureAbsoluteTargetStop - Enabled. Target: ");
        SerialDebug.println(_absoluteTargetStopStepsForMotion);

        // If auto-stop is being enabled, and the physical motor driver is enabled,
        // and ELS is not currently running, but we are in an ELS-compatible mode,
        // then attempt to start ELS motion.
        // isMotorEnabled() checks _stepper->isEnabled().
        if (isMotorEnabled() && !_running &&
            (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING))
        {
            SerialDebug.println("MC::configureAbsoluteTargetStop - Motor enabled but ELS not running. Attempting to start ELS motion.");
            startMotion(); // This will setup SyncTimer and prepare for encoder ticks
        }
    }
    else
    {
        SerialDebug.println("MC::configureAbsoluteTargetStop - Disabled.");
        // Optionally, if disabling auto-stop while motion is running, we might not need to do anything
        // else here, as the check in update() will simply no longer trigger a stop.
    }
}

/**
 * @brief Clears any configured absolute target stop and disables the feature.
 */
void MotionControl::clearAbsoluteTargetStop()
{
    _targetStopFeatureEnabledForMotion = false;
    _absoluteTargetStopStepsForMotion = 0; // Or some other indicator of "not set"
    _targetStopReached = false;
    SerialDebug.println("MC::clearAbsoluteTargetStop - Cleared and disabled.");
}

/**
 * @brief Checks if the auto-stop target was reached and motion was consequently halted.
 * This flag is reset after being read.
 * @return True if auto-stop triggered a halt since last check, false otherwise.
 */
bool MotionControl::wasTargetStopReachedAndMotionHalted()
{
    bool status = _targetStopReached;
    if (status)
    {
        _targetStopReached = false; // Reset flag after reading
    }
    return status;
}

/**
 * @brief Gets the current feed direction of the Z-axis.
 * @return The current FeedDirection.
 */
MotionControl::FeedDirection MotionControl::getCurrentFeedDirection() const
{
    // This needs to be accurately set when ELS motion starts,
    // based on the mode (Turning, Threading) and user inputs (e.g., feed direction toggle).
    // For now, returning the member variable.
    // TODO: Ensure _currentFeedDirection is correctly updated in startMotion() or setMode()
    // based on effective direction of movement.
    return _currentFeedDirection;
}

/**
 * @brief Gets the current absolute position of the Z-axis stepper in microsteps.
 * @return Absolute stepper position in microsteps.
 */
int32_t MotionControl::getCurrentPositionSteps() const
{
    if (_stepper)
    {
        return _stepper->getCurrentPosition();
    }
    return 0;
}

/**
 * @brief Requests an immediate stop of motion, with a specified stop type.
 * This can be used for auto-stop features or other scenarios requiring a halt.
 * @param type The type of stop to perform (e.g., controlled deceleration).
 */
void MotionControl::requestImmediateStop(StopType type)
{
    SerialDebug.print("MC::requestImmediateStop - Type: ");
    SerialDebug.println(type == StopType::CONTROLLED_DECELERATION ? "CONTROLLED_DECELERATION" : "IMMEDIATE_HALT");

    if (_jogActive && _stepper)
    {
        if (type == StopType::IMMEDIATE_HALT)
        {
            _stepper->emergencyStop(); // Or a more direct halt if available that doesn't disable driver
        }
        else
        {
            _stepper->stop(); // Uses configured deceleration
        }
        _jogActive = false;
    }

    if (_running) // If ELS was running
    {
        _syncTimer.enable(false); // Disable SyncTimer ISR
        if (_stepper)
        {
            if (type == StopType::IMMEDIATE_HALT)
            {
                // For STM32Step, emergencyStop() is the most immediate, but also disables driver.
                // If a halt without disable is needed, STM32Step might need enhancement.
                _stepper->emergencyStop();
            }
            else
            {
                _stepper->stop(); // Uses configured deceleration
            }
        }
        _running = false;
    }
    else if (_stepper && !_jogActive && !_running)
    {
        // If neither jog nor ELS was running, but a stop is requested (e.g. from an external event)
        // and stepper exists, ensure it's stopped.
        if (type == StopType::IMMEDIATE_HALT)
        {
            _stepper->emergencyStop();
        }
        else
        {
            _stepper->stop();
        }
    }
    // Note: This function stops motion. It does not disable the motor driver here,
    // unless IMMEDIATE_HALT uses emergencyStop which does.
    // disableMotor() is a separate explicit action.
}

/**
 * @brief Checks if a continuous jog operation is currently active.
 * @return True if jogging is active, false otherwise.
 */
bool MotionControl::isJogActive() const
{
    return _jogActive;
}

/**
 * @brief Checks if any ELS (Electronic Lead Screw) synchronized mode is active.
 * This includes THREADING, TURNING, or FEEDING modes.
 * @return True if an ELS mode is active (i.e., _running is true and not IDLE), false otherwise.
 */
bool MotionControl::isElsActive() const
{
    bool isActive = _running && (_currentMode == Mode::THREADING || _currentMode == Mode::TURNING || _currentMode == Mode::FEEDING);
    // SerialDebug.print("MC::isElsActive() check: MC._running=");
    // SerialDebug.print(_running);
    // SerialDebug.print(", MC._currentMode=");
    // SerialDebug.print(static_cast<int>(_currentMode)); // Cast enum to int for printing
    // SerialDebug.print(" -> isActive=");
    // SerialDebug.println(isActive);
    return isActive;
}
