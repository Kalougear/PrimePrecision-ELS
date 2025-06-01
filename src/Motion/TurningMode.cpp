#include "Motion/TurningMode.h"
#include "Config/SystemConfig.h" // Already included, good.
#include "Config/serial_debug.h"
#include <cmath> // For fabsf if needed, though abs on int should be fine for steps.

TurningMode::TurningMode()
    : _motionControl(nullptr),
      _positioning(nullptr),
      // _feedRate(FeedRate::F0_10), // Removed, _feedRateManager handles this
      _mode(Mode::MANUAL),
      _z_axis_zero_offset_steps(0),     // Initialize zero offset
      _feedDirectionTowardsChuck(true), // Default to Towards Chuck
      _ui_autoStopEnabled(false),       // Initialize auto-stop UI state
      _ui_targetStopAbsoluteSteps(0),   // Initialize auto-stop UI target
      _ui_targetStopIsSet(false),       // Initialize auto-stop UI target set state
      _running(false),
      _error(false),
      _errorMsg(nullptr),
      _autoStopCompletionPendingHmiSignal(false) // Initialize new flag
{
    _positions.start_position = 0.0f;
    _positions.end_position = 0.0f;
    _positions.valid = false;
}

TurningMode::~TurningMode()
{
    end();
}

bool TurningMode::begin(MotionControl *motion_control)
{
    if (!motion_control)
    {
        handleError("Invalid motion control reference");
        return false;
    }

    _motionControl = motion_control;
    _positioning = new Positioning();

    if (!_positioning)
    {
        handleError("Failed to allocate positioning system");
        return false;
    }

    // Configure motion controller for turning mode
    _motionControl->setMode(MotionControl::Mode::TURNING);

    // Initialize FeedRateManager's unit based on system default
    // Use els_default_feed_rate_unit_is_metric as it's specific to feed rates.
    // If SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric is true, it means default is Metric.
    // FeedRateManager::setMetric(true) means use Metric.
    _feedRateManager.setMetric(SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric);
    SerialDebug.print("TurningMode::begin - Initializing _feedRateManager metric: ");
    SerialDebug.println(SystemConfig::RuntimeConfig::System::els_default_feed_rate_unit_is_metric ? "METRIC" : "IMPERIAL");

    // Configure feed rate (this will now use the correctly set unit)
    configureFeedRate();

    resetAutoStopRuntimeSettings(); // Initialize auto-stop state

    return true;
}

void TurningMode::end()
{
    stop();

    if (_positioning)
    {
        delete _positioning;
        _positioning = nullptr;
    }

    _motionControl = nullptr;
}

// --- Mode Activation/Deactivation ---
void TurningMode::activate()
{
    SerialDebug.println("TurningMode::activate() called.");
    if (_motionControl)
    {
        _motionControl->setMode(MotionControl::Mode::TURNING);
        configureFeedRate(); // Ensure MotionControl gets the latest feed rate config for turning
        // Attempt to start motion. MotionControl::startMotion() has guards to prevent issues.
        _motionControl->startMotion();
        SerialDebug.println("TurningMode: Activated. MotionControl mode set to TURNING, configured, and startMotion() called.");
    }
    else
    {
        SerialDebug.println("TurningMode::activate() - Error: _motionControl is null.");
    }
}

void TurningMode::deactivate()
{
    SerialDebug.println("TurningMode::deactivate() called.");
    if (_motionControl)
    {
        _motionControl->stopMotion();
        _motionControl->setMode(MotionControl::Mode::IDLE); // Set to IDLE when leaving turning mode
        SerialDebug.println("TurningMode: Deactivated. MotionControl stopped and mode set to IDLE.");
    }
    else
    {
        SerialDebug.println("TurningMode::deactivate() - Error: _motionControl is null.");
    }
}

// New methods to interact with FeedRateManager
void TurningMode::selectNextFeedRate()
{
    _feedRateManager.handlePrevNextValue(2); // 2 for next
    if (_motionControl)
    {
        configureFeedRate();
    }
}

void TurningMode::selectPreviousFeedRate()
{
    _feedRateManager.handlePrevNextValue(1); // 1 for previous
    if (_motionControl)
    {
        configureFeedRate();
    }
}

void TurningMode::setFeedRateMetric(bool isMetric)
{
    _feedRateManager.setMetric(isMetric);
    if (_motionControl)
    {
        configureFeedRate();
    }
}

bool TurningMode::getFeedRateIsMetric() const
{
    return _feedRateManager.getIsMetric();
}

FeedRateManager &TurningMode::getFeedRateManager()
{
    return _feedRateManager;
}

// void TurningMode::setFeedRate(FeedRate rate) // Removed
// {
//     _feedRate = rate; // Removed
//     if (_motionControl)
//     {
//         configureFeedRate();
//     }
// }

void TurningMode::setMode(Mode mode)
{
    _mode = mode;
}

void TurningMode::setPositions(const Position &positions)
{
    _positions = positions;
}

void TurningMode::start()
{
    if (_running || _error || !_motionControl)
    {
        return;
    }

    // In semi-auto mode, validate positions
    if (_mode == Mode::SEMI_AUTO && !_positions.valid)
    {
        handleError("Invalid position data for semi-auto mode");
        return;
    }

    // Reset position tracking if in semi-auto mode
    if (_mode == Mode::SEMI_AUTO && _positioning)
    {
        _positioning->reset();
        _positioning->setEndPosition(_positions.end_position);
    }

    // If auto-stop is enabled and a target is set, ensure MotionControl is armed
    if (_ui_autoStopEnabled && _ui_targetStopIsSet && _motionControl)
    {
        SerialDebug.print("TurningMode::start - AutoStop: Enabled=");
        SerialDebug.print(_ui_autoStopEnabled);
        SerialDebug.print(", TargetSet=");
        SerialDebug.print(_ui_targetStopIsSet);
        SerialDebug.print(", TargetSteps=");
        SerialDebug.println(_ui_targetStopAbsoluteSteps);
        SerialDebug.println("TurningMode::start - Arming auto-stop in MotionControl.");
        _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
    }
    else if (_motionControl)
    {
        SerialDebug.println("TurningMode::start - AutoStop: Not enabled or no target set. Clearing MC target.");
        // Ensure MC auto-stop is not armed if UI doesn't want it for this run
        _motionControl->clearAbsoluteTargetStop();
    }

    // Start motion
    _motionControl->startMotion();
    _running = true;
    SerialDebug.print("TurningMode::start() - END. _running is now: ");
    SerialDebug.println(_running);
}

void TurningMode::stop()
{
    SerialDebug.print("TurningMode::stop() called. Current _running state: ");
    SerialDebug.println(_running);
    if (!_running)
    {
        return;
    }

    if (_motionControl)
    {
        _motionControl->stopMotion();
    }

    _running = false;
}

void TurningMode::update()
{
    // Use MotionControl's ELS active state for the primary guard.
    // TurningMode::_running should ideally mirror this, but let's rely on MC's state directly here.
    if (!_motionControl || !_motionControl->isElsActive())
    {
        // If MC isn't running ELS, but TurningMode thinks it is, correct.
        if (_motionControl && _running && !_motionControl->isElsActive())
        {
            _running = false;
        }
        return;
    }
    // If we reach here, _motionControl is valid and its ELS is active.
    // Ensure TurningMode::_running reflects this if it was somehow false.
    if (!_running)
    {
        _running = true;
    }

    // Update motion controller
    _motionControl->update();

    // Log current position vs target if auto-stop is active
    // Condition now also implicitly relies on _motionControl->isElsActive() via the guard above.
    // if (_ui_autoStopEnabled && _ui_targetStopIsSet) // _running and _motionControl are confirmed by guard
    // {
    //     // This logging can be very verbose, enable only if needed for specific debugging
    //     // int32_t currentSteps = _motionControl->getCurrentPositionSteps();
    //     // float currentZPos = getCurrentPosition(); // Get Z position in current units (mm or in)
    //     // char zPosStr[20];
    //     // dtostrf(currentZPos, 7, 3, zPosStr); // Format float to string (width 7, 3 decimal places)

    //     // SerialDebug.print("TurningMode::update - AutoStop Active. ZPos: ");
    //     // SerialDebug.print(zPosStr);
    //     // SerialDebug.print(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? "mm" : "in");
    //     // SerialDebug.print(", CurrentSteps: ");
    //     // SerialDebug.print(currentSteps);
    //     // SerialDebug.print(", TargetSteps: ");
    //     // SerialDebug.println(_ui_targetStopAbsoluteSteps);
    // }

    // In semi-auto mode, check if we've reached the end position
    if (_mode == Mode::SEMI_AUTO && _positioning)
    {
        float currentPos = getCurrentPosition();
        if (_positioning->hasReachedEndPosition(currentPos))
        {
            // SerialDebug.println("Reached end position, stopping"); // Optional log
            stop();
        }
    }

    // Check for auto-stop completion
    if (checkAndHandleAutoStopCompletion())
    {
        // SerialDebug.println("TurningMode::update - Auto-stop completed. HMI flash needed."); // Optional log
        // The target is cleared within checkAndHandleAutoStopCompletion if MC confirms stop.
        // UI display should be updated by TurningPageHandler to "---" or similar.
    }
}

float TurningMode::getCurrentPosition() const
{
    if (!_motionControl)
    {
        return 0.0f;
    }

    MotionControl::Status status = _motionControl->getStatus();

    // Retrieve parameters for calculation
    float actualLeadscrewPitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch; // mm or inches per leadscrew revolution
    uint16_t motorPulleyTeeth = SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth;
    uint16_t leadscrewPulleyTeeth = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth; // Corrected variable name

    uint32_t motorNativeStepsPerRev = SystemConfig::Limits::Stepper::STEPS_PER_REV; // e.g., 200
    uint32_t microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;

    // Calculate gear ratio between motor and leadscrew
    float gearRatio = 1.0f;       // Default to 1:1 if pulley teeth are not set or invalid
    if (leadscrewPulleyTeeth > 0) // Avoid division by zero
    {
        gearRatio = static_cast<float>(motorPulleyTeeth) / static_cast<float>(leadscrewPulleyTeeth);
    }

    // Calculate effective distance moved by the Z-axis per one full revolution of the MOTOR
    float effectiveDistancePerMotorRev = actualLeadscrewPitch * gearRatio;

    // Calculate total effective steps for one full revolution of the MOTOR
    float totalEffectiveStepsPerMotorRev = static_cast<float>(motorNativeStepsPerRev * microsteps);

    if (totalEffectiveStepsPerMotorRev == 0)
        return 0.0f; // Avoid division by zero

    // Calculate final position
    // status.stepper_position is the raw step count from the motor
    int32_t raw_stepper_pos = status.stepper_position;
    int32_t compensated_stepper_pos = raw_stepper_pos - _z_axis_zero_offset_steps;

    float finalPosition = (static_cast<float>(compensated_stepper_pos) / totalEffectiveStepsPerMotorRev) * effectiveDistancePerMotorRev;

    return finalPosition;
}

float TurningMode::getFeedRateValue() const
{
    return _feedRateManager.getCurrentValue();
}

bool TurningMode::getCurrentFeedRateWarning() const
{
    // FeedRateManager is now the source of truth.
    return _feedRateManager.getCurrentWarning();
}

const char *TurningMode::getFeedRateCategory() const
{
    return _feedRateManager.getCurrentCategory();
}

void TurningMode::configureFeedRate()
{
    if (!_motionControl)
    {
        return;
    }

    // Configure motion control with feed rate as thread pitch
    MotionControl::Config config;

    float baseFeedValue = _feedRateManager.getCurrentValue(); // This is always positive
    SerialDebug.print("TurningMode::configureFeedRate - baseFeedValue from FeedRateManager: ");
    SerialDebug.println(baseFeedValue, 4);
    if (!_feedRateManager.getIsMetric())
    {
        baseFeedValue *= 25.4f; // Convert inches/rev to mm/rev
        SerialDebug.print("TurningMode::configureFeedRate - baseFeedValue (converted to mm if applicable): ");
        SerialDebug.println(baseFeedValue, 4);
    }

    // Apply the feed direction to the thread_pitch
    config.thread_pitch = _feedDirectionTowardsChuck ? -baseFeedValue : baseFeedValue;
    SerialDebug.print("TurningMode::configureFeedRate - _feedDirectionTowardsChuck: ");
    SerialDebug.println(_feedDirectionTowardsChuck);
    SerialDebug.print("TurningMode::configureFeedRate - final config.thread_pitch: ");
    SerialDebug.println(config.thread_pitch, 4);

    config.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    config.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    config.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    config.reverse_direction = SystemConfig::RuntimeConfig::Z_Axis::invert_direction; // Corrected to use Z_Axis config
    config.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;

    _motionControl->setConfig(config);
    // _running state is now managed by requestMotorEnable/Disable and start/stop,
    // not directly by configureFeedRate's effect on MotionControl's ELS state.
}

void TurningMode::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
    stop();

    SerialDebug.print("TurningMode error: ");
    SerialDebug.println(msg);
}

void TurningMode::setZeroPosition()
{
    if (!_motionControl)
    {
        SerialDebug.println("TurningMode::setZeroPosition - Error: MotionControl not available.");
        return;
    }
    MotionControl::Status status = _motionControl->getStatus();
    _z_axis_zero_offset_steps = status.stepper_position;
    SerialDebug.print("TurningMode::setZeroPosition - New Z offset: ");
    SerialDebug.println(_z_axis_zero_offset_steps);
}

// --- Motor Enable/Disable Methods ---
bool TurningMode::isMotorEnabled() const
{
    if (_motionControl)
    {
        return _motionControl->isMotorEnabled(); // Assumes MotionControl::isMotorEnabled() exists
    }
    SerialDebug.println("TurningMode::isMotorEnabled - Warning: _motionControl is null.");
    return false; // Default to false if no motion control
}

void TurningMode::requestMotorEnable()
{
    if (_motionControl)
    {
        _motionControl->enableMotor(); // Assumes MotionControl::enableMotor() exists
        _running = true;               // Reflect that TurningMode considers itself running
        SerialDebug.println("TurningMode: Motor enable requested. _running set to true.");
    }
    else
    {
        SerialDebug.println("TurningMode::requestMotorEnable - Warning: _motionControl is null.");
    }
}

void TurningMode::requestMotorDisable()
{
    if (_motionControl)
    {
        _motionControl->disableMotor(); // Assumes MotionControl::disableMotor() exists
        _running = false;               // Reflect that TurningMode considers itself stopped
        SerialDebug.println("TurningMode: Motor disable requested. _running set to false.");
    }
    else
    {
        SerialDebug.println("TurningMode::requestMotorDisable - Warning: _motionControl is null.");
    }
}

// --- Feed Direction Methods ---
void TurningMode::setFeedDirection(bool towardsChuck)
{
    if (_feedDirectionTowardsChuck != towardsChuck)
    {
        _feedDirectionTowardsChuck = towardsChuck;
        SerialDebug.print("TurningMode: Feed direction set to: ");
        SerialDebug.println(_feedDirectionTowardsChuck ? "TOWARDS CHUCK" : "AWAY FROM CHUCK");
        if (_motionControl)
        {
            // Re-configure motion control to apply the new direction immediately if needed
            // This is important if a feed is active or about to be configured.
            configureFeedRate();
            // If motion is running, we might need to stop and restart or update MotionControl more dynamically.
            // For now, configureFeedRate will set the new pitch for the next startMotion or if setConfig is called by MC.
        }
    }
}

bool TurningMode::getFeedDirectionTowardsChuck() const
{
    return _feedDirectionTowardsChuck;
}

// --- Z-Axis Auto-Stop Feature Implementations ---

void TurningMode::resetAutoStopRuntimeSettings()
{
    _ui_autoStopEnabled = false;
    _ui_targetStopIsSet = false;
    _ui_targetStopAbsoluteSteps = 0;
    if (_motionControl)
    {
        _motionControl->clearAbsoluteTargetStop();
    }
    SerialDebug.println("TurningMode: Auto-stop runtime settings reset.");
}

void TurningMode::setUiAutoStopEnabled(bool enabled)
{
    _ui_autoStopEnabled = enabled;
    if (!_ui_autoStopEnabled)
    {
        // If disabling, also clear any set target in MC and UI
        _ui_targetStopIsSet = false;
        _ui_targetStopAbsoluteSteps = 0;
        if (_motionControl)
        {
            _motionControl->clearAbsoluteTargetStop();
        }
        SerialDebug.println("TurningMode: UI Auto-stop disabled, target cleared.");
    }
    else
    {
        // If enabling and a target is already set in UI, re-arm it in MC
        if (_ui_targetStopIsSet && _motionControl)
        {
            _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
        }
        SerialDebug.println("TurningMode: UI Auto-stop enabled.");
    }
}

bool TurningMode::isUiAutoStopEnabled() const
{
    return _ui_autoStopEnabled;
}

void TurningMode::setUiAutoStopTargetPositionFromString(const char *valueStr)
{
    if (!_motionControl)
    {
        SerialDebug.println("TurningMode::setUiAutoStopTargetPositionFromString - MC not available.");
        return;
    }

    float userValue = atof(valueStr);
    float valueInMm;

    // Convert userValue to mm based on current system units
    // SystemConfig::RuntimeConfig::System::measurement_unit_is_metric == false means Imperial
    if (!SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        valueInMm = userValue * 25.4f; // Convert inches to mm
    }
    else
    {
        valueInMm = userValue; // Already in mm
    }

    // Convert the mm value (which is relative to Z-zero) to absolute steps
    // First, get Z-zero offset in mm
    float zZeroOffsetMm = 0.0f;
    if (SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev > 0 && fabsf(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch) > 0.00001f)
    {
        float motorNativeStepsPerRev = static_cast<float>(SystemConfig::Limits::Stepper::STEPS_PER_REV);
        float microsteps = static_cast<float>(SystemConfig::RuntimeConfig::Stepper::microsteps);
        float totalEffectiveStepsPerMotorRev = motorNativeStepsPerRev * microsteps;

        float actualLeadscrewPitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
        // Corrected TPI to pitch conversion: pitch = 25.4 / TPI
        if (!SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric && actualLeadscrewPitch > 0)
        {
            actualLeadscrewPitch = 25.4f / actualLeadscrewPitch;
        }
        else if (!SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
        {
            actualLeadscrewPitch *= 25.4f; // If it was inches (not TPI)
        }

        float motorPulleyTeeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
        float leadscrewPulleyTeeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
        float gearRatio = (leadscrewPulleyTeeth > 0) ? (motorPulleyTeeth / leadscrewPulleyTeeth) : 1.0f;
        float effectiveDistancePerMotorRev = actualLeadscrewPitch * gearRatio;

        if (fabsf(totalEffectiveStepsPerMotorRev) > 0.00001f)
        {
            zZeroOffsetMm = (static_cast<float>(_z_axis_zero_offset_steps) / totalEffectiveStepsPerMotorRev) * effectiveDistancePerMotorRev;
        }
    }

    // Determine relative target in mm based on feed direction
    // valueInMm is the magnitude from HMI (always positive)
    float relativeTargetMmFromZero;
    if (_feedDirectionTowardsChuck)
    {                                          // If UI-selected direction is towards chuck (negative movement)
        relativeTargetMmFromZero = -valueInMm; // Target is, e.g., -1mm from Z-zero point
    }
    else
    {                                         // UI-selected direction is away from chuck (positive movement)
        relativeTargetMmFromZero = valueInMm; // Target is, e.g., +1mm from Z-zero point
    }
    float absoluteTargetMm = zZeroOffsetMm + relativeTargetMmFromZero;

    // Now convert absoluteTargetMm to absolute steps
    // This reuses logic similar to getCurrentPosition's inverse
    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f;
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f;

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : z_ls_pitch_val * 25.4f; // This should be direct mm pitch
    if (!z_ls_is_metric && z_ls_pitch_val > 0)
        z_ls_pitch_mm_per_ls_rev = 25.4f / z_ls_pitch_val; // If TPI, convert to mm pitch
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        z_ls_pitch_mm_per_ls_rev = 1.0f;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        z_mm_travel_per_motor_rev = 1.0f;

    float z_usteps_per_mm_travel = z_motor_total_usteps / z_mm_travel_per_motor_rev;

    _ui_targetStopAbsoluteSteps = static_cast<int32_t>(roundf(absoluteTargetMm * z_usteps_per_mm_travel));
    _ui_targetStopIsSet = true;
    // ADDED LOG
    SerialDebug.print("TM::setUiAutoStopTargetPositionFromString - AFTER setting _ui_targetStopIsSet: ");
    SerialDebug.println(_ui_targetStopIsSet);

    SerialDebug.print("TurningMode: UI Auto-stop target string '");
    SerialDebug.print(valueStr);
    SerialDebug.print("' parsed to user val: ");
    SerialDebug.print(userValue);
    SerialDebug.print(", valInMm: ");
    SerialDebug.print(valueInMm);
    SerialDebug.print(", absTargetMm: ");
    SerialDebug.print(absoluteTargetMm);
    SerialDebug.print(", absSteps: ");
    SerialDebug.println(_ui_targetStopAbsoluteSteps);

    if (_ui_autoStopEnabled)
    {
        _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
    }
    // ADDED LOG
    SerialDebug.print("TM::setUiAutoStopTargetPositionFromString - END OF FUNCTION. _ui_targetStopIsSet: ");
    SerialDebug.print(_ui_targetStopIsSet);
    SerialDebug.print(", _running: ");
    SerialDebug.println(_running);
}

void TurningMode::grabCurrentZAsUiAutoStopTarget()
{
    if (!_motionControl)
    {
        SerialDebug.println("TurningMode::grabCurrentZAsUiAutoStopTarget - MC not available.");
        return;
    }
    _ui_targetStopAbsoluteSteps = _motionControl->getCurrentPositionSteps();
    _ui_targetStopIsSet = true;

    SerialDebug.print("TurningMode: UI Auto-stop target grabbed as current Z (abs steps): ");
    SerialDebug.println(_ui_targetStopAbsoluteSteps);

    if (_ui_autoStopEnabled)
    {
        _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
    }
}

String TurningMode::getFormattedUiAutoStopTarget() const
{
    if (!_ui_targetStopIsSet || !_motionControl)
    {
        return String("--- ") + (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? "mm" : "in");
    }

    // Convert _ui_targetStopAbsoluteSteps back to a displayable value relative to Z-zero
    // Inverse of the logic in setUiAutoStopTargetPositionFromString
    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f;
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f;

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : z_ls_pitch_val * 25.4f; // Direct mm pitch
    if (!z_ls_is_metric && z_ls_pitch_val > 0)
        z_ls_pitch_mm_per_ls_rev = 25.4f / z_ls_pitch_val; // If TPI, convert to mm pitch
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        z_ls_pitch_mm_per_ls_rev = 1.0f;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        z_mm_travel_per_motor_rev = 1.0f;

    float absoluteTargetMm = static_cast<float>(_ui_targetStopAbsoluteSteps) / (z_motor_total_usteps / z_mm_travel_per_motor_rev);

    // Convert Z-zero offset from steps to mm
    float zZeroOffsetMm = 0.0f;
    // Re-define or ensure motorPulleyTeeth and leadscrewPulleyTeeth are in scope for this calculation block
    float motorPulleyTeeth_forOffset = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (motorPulleyTeeth_forOffset < 1.0f)
        motorPulleyTeeth_forOffset = 1.0f;
    float leadscrewPulleyTeeth_forOffset = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (leadscrewPulleyTeeth_forOffset < 1.0f)
        leadscrewPulleyTeeth_forOffset = 1.0f;

    if (SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev > 0 && fabsf(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch) > 0.00001f)
    {
        float motorNativeStepsPerRev = static_cast<float>(SystemConfig::Limits::Stepper::STEPS_PER_REV);
        float microsteps = static_cast<float>(SystemConfig::RuntimeConfig::Stepper::microsteps);
        float totalEffectiveStepsPerMotorRev = motorNativeStepsPerRev * microsteps;

        float actualLeadscrewPitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
        if (!SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric && actualLeadscrewPitch > 0)
        {
            actualLeadscrewPitch = 25.4f / actualLeadscrewPitch;
        }
        else if (!SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric)
        {
            actualLeadscrewPitch *= 25.4f; // if it was inches
        }

        float gearRatio = (leadscrewPulleyTeeth_forOffset > 0) ? (motorPulleyTeeth_forOffset / leadscrewPulleyTeeth_forOffset) : 1.0f;
        float effectiveDistancePerMotorRev = actualLeadscrewPitch * gearRatio;

        if (fabsf(totalEffectiveStepsPerMotorRev) > 0.00001f)
        {
            zZeroOffsetMm = (static_cast<float>(_z_axis_zero_offset_steps) / totalEffectiveStepsPerMotorRev) * effectiveDistancePerMotorRev;
        }
    }

    float displayValueMm = absoluteTargetMm - zZeroOffsetMm;
    float displayValueFinal;
    String unitStr;

    if (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        displayValueFinal = displayValueMm;
        unitStr = "mm";
    }
    else
    {
        displayValueFinal = displayValueMm / 25.4f; // Convert mm to inches
        unitStr = "in";
    }

    char buffer[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH];
    snprintf(buffer, sizeof(buffer), "%.2f %s", displayValueFinal, unitStr.c_str()); // Adjust precision as needed
    return String(buffer);
}

bool TurningMode::checkAndHandleAutoStopCompletion()
{
    if (_motionControl && _motionControl->wasTargetStopReachedAndMotionHalted())
    {
        SerialDebug.println("TurningMode: Auto-stop completion detected from MotionControl.");
        // The flag in MotionControl is reset by wasTargetStopReachedAndMotionHalted()
        // Clear UI target so user has to set a new one
        _ui_targetStopIsSet = false;
        // _ui_autoStopEnabled remains as is, user might want to set a new target with it still enabled.
        _autoStopCompletionPendingHmiSignal = true; // Signal HMI handler
        return true;                                // Indicates completion should be signaled
    }
    return false;
}

// --- HMI Signaling for Auto-Stop Completion ---
bool TurningMode::isAutoStopCompletionPendingHmiSignal() const
{
    return _autoStopCompletionPendingHmiSignal;
}

void TurningMode::clearAutoStopCompletionHmiSignal()
{
    _autoStopCompletionPendingHmiSignal = false;
}
