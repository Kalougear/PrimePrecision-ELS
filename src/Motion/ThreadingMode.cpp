#include "Motion/ThreadingMode.h"
#include "Config/SystemConfig.h"
#include "Config/serial_debug.h"
#include "UI/HmiHandlers/ThreadingPageHandler.h" // For getSelectedPitchData
#include <math.h>                                // For fabs

ThreadingMode::ThreadingMode()
    : _motionControl(nullptr),
      _positioning(nullptr),
      m_feedDirectionIsTowardsChuck(true), // Default to Towards Chuck / RH
      _running(false),
      _error(false),
      _errorMsg(""),
      _z_axis_zero_offset_steps(0), // Initialize Z-axis zero offset
      _ui_autoStopEnabled(false),
      _ui_targetStopAbsoluteSteps(0),
      _ui_targetStopIsSet(false),
      _autoStopCompletionPendingHmiSignal(false)
{
    // Initialize thread data to defaults
    _threadData.pitch = 1.0f; // Default to 1mm pitch
    _threadData.starts = 1;
    _threadData.units = Units::METRIC;
    _threadData.type = ThreadType::STANDARD;
    _threadData.valid = true;

    // Initialize positions to defaults
    _positions.start_position = 0.0f;
    _positions.end_position = 0.0f; // Or some safe default
    _positions.valid = false;       // Positions not set by default
}

ThreadingMode::~ThreadingMode()
{
    end();
}

bool ThreadingMode::begin(MotionControl *motion_control)
{
    if (!motion_control)
    {
        handleError("MotionControl reference is null in ThreadingMode::begin");
        return false;
    }
    _motionControl = motion_control;

    // Initialize Positioning helper if needed, or assume it's managed elsewhere
    // For now, let's assume _motionControl provides position info or _positioning is not used yet.
    // _positioning = new Positioning(_motionControl);
    // if (!_positioning) {
    //     handleError("Failed to initialize Positioning in ThreadingMode");
    //     return false;
    // }

    // Load initial pitch from HMI selection (if already available)
    // This ensures that if the page handler has already set a default, we pick it up.
    updatePitchFromHmiSelection();

    SerialDebug.println("ThreadingMode initialized.");
    return true;
}

void ThreadingMode::end()
{
    if (_running)
    {
        stop();
    }
    // if (_positioning) {
    //     delete _positioning;
    //     _positioning = nullptr;
    // }
    _motionControl = nullptr; // Clear reference, don't delete if owned elsewhere
    SerialDebug.println("ThreadingMode ended.");
}

void ThreadingMode::setThreadData(const ThreadData &thread_data)
{
    _threadData = thread_data;
    _threadData.valid = true; // Assume data being set is intended to be valid

    SerialDebug.print("ThreadingMode: Thread data set - Pitch: ");
    SerialDebug.print(_threadData.pitch);
    SerialDebug.print(_threadData.units == Units::METRIC ? " mm, " : " TPI, ");
    SerialDebug.print("Starts: ");
    SerialDebug.println(_threadData.starts);

    // If running, reconfigure motion control with new thread data
    if (_running)
    {
        configureThreading();
    }
}

void ThreadingMode::setPositions(const Position &positions)
{
    _positions = positions;
    // Add validation for positions if necessary
    _positions.valid = true;
    SerialDebug.println("ThreadingMode: Positions set.");
}

void ThreadingMode::enableMultiStart(bool enable)
{
    // This function might be deprecated if starts are part of ThreadData
    // For now, let's assume it modifies _threadData.starts or is handled by HMI directly
    if (enable && _threadData.starts == 1)
    {
        _threadData.starts = 2; // Example: default to 2 starts if enabling
    }
    else if (!enable)
    {
        _threadData.starts = 1;
    }
    SerialDebug.print("ThreadingMode: Multi-start set to ");
    SerialDebug.println(_threadData.starts);
}

void ThreadingMode::start()
{
    if (!_motionControl)
    {
        handleError("MotionControl not initialized in ThreadingMode");
        return;
    }
    if (!_motionControl->isMotorEnabled()) // Check if motor is enabled
    {
        handleError("Motor is not enabled. Cannot start ThreadingMode.");
        return;
    }
    if (!_threadData.valid)
    {
        handleError("Thread data not valid for starting");
        return;
    }
    // if (!_positions.valid && _motionControl->isTargetStopFeatureEnabled()) { // Assuming target stop needs valid positions
    //     handleError("Position data not valid for starting with auto-stop");
    //     return;
    // }

    SerialDebug.println("ThreadingMode: Starting...");
    SerialDebug.print("ThreadingMode::start() - About to configure with Pitch: ");
    SerialDebug.print(_threadData.pitch);
    SerialDebug.print(_threadData.units == Units::METRIC ? " mm" : " TPI");
    SerialDebug.print(", Starts: ");
    SerialDebug.println(_threadData.starts);

    configureThreading();
    _motionControl->setMode(MotionControl::Mode::THREADING); // Ensure correct ELS mode
    _motionControl->startMotion();                           // Or a specific start for threading if different
    _running = true;
    _error = false;
}

void ThreadingMode::stop()
{
    if (!_motionControl)
    {
        handleError("MotionControl not initialized in ThreadingMode");
        return;
    }
    SerialDebug.println("ThreadingMode: Stopping...");
    _motionControl->stopMotion(); // Or a specific stop for threading
    _running = false;
}

void ThreadingMode::update()
{
    if (!_running || !_motionControl)
    {
        return;
    }
    // Threading mode might have specific update logic, e.g., checking for end of thread pass
    // For now, main ELS sync is handled by MotionControl's ISR via SyncTimer

    // Check for auto-stop completion
    checkAndHandleAutoStopCompletion();
}

void ThreadingMode::setZeroPosition()
{
    if (_motionControl)
    {
        _z_axis_zero_offset_steps = _motionControl->getCurrentPositionSteps();
        SerialDebug.print("ThreadingMode: Z-axis zero offset set to ");
        SerialDebug.println(_z_axis_zero_offset_steps);
    }
    else
    {
        SerialDebug.println("ThreadingMode::setZeroPosition: MotionControl is null.");
    }
}

void ThreadingMode::resetZAxisZeroOffset()
{
    _z_axis_zero_offset_steps = 0;
    SerialDebug.println("ThreadingMode: Z-axis zero offset reset.");
}

float ThreadingMode::getCurrentPosition() const
{
    if (!_motionControl)
    {
        return 0.0f;
    }

    // Get current position in steps from MotionControl and apply offset
    int32_t current_steps = _motionControl->getCurrentPositionSteps() - _z_axis_zero_offset_steps;

    // Convert steps to mm or inches based on system configuration
    float leadscrew_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool is_metric_leadscrew = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;

    float leadscrew_pitch_mm;
    if (is_metric_leadscrew)
    {
        leadscrew_pitch_mm = leadscrew_pitch_val;
    }
    else // Imperial TPI
    {
        if (leadscrew_pitch_val == 0)
            return 0.0f;                                  // Avoid division by zero
        leadscrew_pitch_mm = 25.4f / leadscrew_pitch_val; // Convert TPI to mm pitch
    }

    float motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    float leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    // driver_pulses_per_rev from SystemConfig already accounts for microsteps for the Z-axis driver
    float pulses_per_rev = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);

    if (leadscrew_pulley_teeth == 0 || pulses_per_rev == 0)
        return 0.0f; // Avoid division by zero

    float travel_per_pulse = (leadscrew_pitch_mm * motor_pulley_teeth) / (leadscrew_pulley_teeth * pulses_per_rev);
    float current_position_mm = static_cast<float>(current_steps) * travel_per_pulse;

    // Convert to inches if system is set to imperial display
    if (::SystemConfig::RuntimeConfig::System::measurement_unit_is_metric == false)
    {
        return current_position_mm / 25.4f;
    }

    return current_position_mm;
}

float ThreadingMode::getEffectivePitch() const
{
    if (!_threadData.valid)
        return 0.0f;

    float basePitchMm;
    if (_threadData.units == Units::METRIC)
    {
        basePitchMm = _threadData.pitch;
    }
    else // IMPERIAL (TPI)
    {
        if (_threadData.pitch == 0)
            return 0.0f; // Avoid division by zero
        basePitchMm = 25.4f / _threadData.pitch;
    }
    return basePitchMm * _threadData.starts;
}

void ThreadingMode::updatePitchFromHmiSelection()
{
    const ThreadTable::ThreadData &hmiSelectedPitch = ThreadingPageHandler::getSelectedPitchData();

    ThreadData newInternalData;
    newInternalData.pitch = hmiSelectedPitch.pitch;
    newInternalData.units = hmiSelectedPitch.metric ? Units::METRIC : Units::IMPERIAL;

    // Preserve existing starts, type, and valid status unless HMI also controls them
    newInternalData.starts = _threadData.starts;
    newInternalData.type = _threadData.type; // Or determine from hmiSelectedPitch.name if it's "standard"
    newInternalData.valid = true;            // Assume valid if selected from HMI

    setThreadData(newInternalData); // This will also print debug info

    SerialDebug.print("ThreadingMode: Updated pitch from HMI. New effective pitch (mm): ");
    SerialDebug.println(getEffectivePitch());

    // If currently running, reconfigure motion control immediately
    if (_running && _motionControl)
    {
        // When pitch changes while running, we need to re-apply config which includes direction
        configureThreading(); // This will set direction in MotionControl
        // No, configureThreading sets the pitch. Direction should be set before startMotion.
        // Let's ensure start() handles setting the direction.
    }
}

// Feed Direction Control
void ThreadingMode::setFeedDirection(bool isTowardsChuck)
{
    m_feedDirectionIsTowardsChuck = isTowardsChuck;
    SerialDebug.print("ThreadingMode: Feed direction set to: ");
    SerialDebug.println(m_feedDirectionIsTowardsChuck ? "Towards Chuck (RH)" : "Away from Chuck (LH)");

    // If ELS is active and this changes, we might need to update MotionControl immediately
    // or ensure it's picked up before next motion.
    // For now, assume it's picked up by start() or when configureThreading() is called.
    // If running, it might be complex to change direction mid-operation.
    // Typically, direction is set before starting.

    // Reconfigure MotionControl if it's initialized, to apply the new direction.
    // This mimics TurningMode's behavior where changing direction immediately reconfigures.
    if (_motionControl)
    {
        configureThreading();
        // If _running, MotionControl::setConfig (called by configureThreading)
        // should handle stopping and restarting motion with the new config.
    }
}

bool ThreadingMode::isFeedDirectionTowardsChuck() const
{
    return m_feedDirectionIsTowardsChuck;
}

void ThreadingMode::configureThreading()
{
    if (!_motionControl || !_threadData.valid)
    {
        handleError("Cannot configure threading: invalid state or data");
        return;
    }

    MotionControl::Config mcCfg;
    mcCfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    mcCfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV; // Base steps
    mcCfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    // Keep reverse_direction consistent with TurningMode's apparent default (likely false)
    // TurningMode achieves direction by negating pitch. We will do the same.
    mcCfg.reverse_direction = false;
    mcCfg.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;

    float effectivePitchValue = getEffectivePitch(); // This is always positive
    if (m_feedDirectionIsTowardsChuck)
    { // True for Towards Chuck (RH)
        // MotionControl interprets negative pitch as TOWARDS_CHUCK (based on TurningMode logs)
        mcCfg.thread_pitch = -effectivePitchValue;
    }
    else
    { // False for Away from Chuck (LH)
        // MotionControl interprets positive pitch as AWAY_FROM_CHUCK
        mcCfg.thread_pitch = effectivePitchValue;
    }

    _motionControl->setConfig(mcCfg); // This will apply the pitch and its sign

    SerialDebug.print("ThreadingMode: MotionControl configured with effective pitch (mm): ");
    SerialDebug.println(mcCfg.thread_pitch); // This will now show signed pitch
    SerialDebug.print("ThreadingMode: mcCfg.reverse_direction is: ");
    SerialDebug.println(mcCfg.reverse_direction);
    SerialDebug.print("ThreadingMode: m_feedDirectionIsTowardsChuck is: ");
    SerialDebug.println(m_feedDirectionIsTowardsChuck ? "true (TOWARDS)" : "false (AWAY)");
}

void ThreadingMode::activate()
{
    SerialDebug.println("ThreadingMode: Activating...");
    resetZAxisZeroOffset(); // Reset Z offset when activating mode
    // Ensure we have the latest pitch from HMI before starting
    updatePitchFromHmiSelection();
    // The start() method will set _running = true, configureThreading, and start MotionControl
    start();
}

void ThreadingMode::deactivate()
{
    SerialDebug.println("ThreadingMode: Deactivating...");
    // The stop() method will set _running = false and stop MotionControl
    stop();
}

float ThreadingMode::convertToMetricPitch(float imperial_tpi) const
{
    if (imperial_tpi == 0)
        return 0.0f;
    return 25.4f / imperial_tpi;
}

void ThreadingMode::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
    SerialDebug.print("ThreadingMode ERROR: ");
    SerialDebug.println(msg);
    // Potentially stop motion or enter a safe state
    if (_motionControl && _running)
    {
        _motionControl->stopMotion();
    }
    _running = false;
}

// --- Z-Axis Auto-Stop Feature Implementations ---

void ThreadingMode::resetAutoStopRuntimeSettings()
{
    _ui_autoStopEnabled = false;
    _ui_targetStopIsSet = false;
    _ui_targetStopAbsoluteSteps = 0;
    if (_motionControl)
    {
        _motionControl->clearAbsoluteTargetStop();
    }
    SerialDebug.println("ThreadingMode: Auto-stop runtime settings reset.");
}

void ThreadingMode::setUiAutoStopEnabled(bool enabled)
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
        SerialDebug.println("ThreadingMode: UI Auto-stop disabled, target cleared.");
    }
    else
    {
        // If enabling and a target is already set in UI, re-arm it in MC
        if (_ui_targetStopIsSet && _motionControl)
        {
            _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
        }
        SerialDebug.println("ThreadingMode: UI Auto-stop enabled.");
    }
}

bool ThreadingMode::isUiAutoStopEnabled() const
{
    return _ui_autoStopEnabled;
}

void ThreadingMode::setUiAutoStopTargetPositionFromString(const char *valueStr)
{
    if (!_motionControl)
    {
        SerialDebug.println("ThreadingMode::setUiAutoStopTargetPositionFromString - MC not available.");
        return;
    }

    float userValue = atof(valueStr);

    // In Threading mode, the target is always relative to the Z-zero point.
    // The value from the HMI is the desired travel distance.
    float target_travel_mm;

    // Convert userValue to mm based on current system units
    if (!SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        target_travel_mm = userValue * 25.4f; // Convert inches to mm
    }
    else
    {
        target_travel_mm = userValue; // Already in mm
    }

    // Determine the absolute target position in steps.
    // The target is the Z-zero position plus the desired travel distance, considering the feed direction.
    int32_t travel_steps = _motionControl->convertUnitsToSteps(target_travel_mm);

    if (m_feedDirectionIsTowardsChuck)
    { // Towards chuck is negative direction
        _ui_targetStopAbsoluteSteps = _z_axis_zero_offset_steps - travel_steps;
    }
    else
    { // Away from chuck is positive direction
        _ui_targetStopAbsoluteSteps = _z_axis_zero_offset_steps + travel_steps;
    }

    _ui_targetStopIsSet = true;

    SerialDebug.print("ThreadingMode: UI Auto-stop target string '");
    SerialDebug.print(valueStr);
    SerialDebug.print("' parsed to travel mm: ");
    SerialDebug.print(target_travel_mm);
    SerialDebug.print(", travel steps: ");
    SerialDebug.print(travel_steps);
    SerialDebug.print(", abs target steps: ");
    SerialDebug.println(_ui_targetStopAbsoluteSteps);

    if (_ui_autoStopEnabled)
    {
        _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
    }
}

void ThreadingMode::grabCurrentZAsUiAutoStopTarget()
{
    if (!_motionControl)
    {
        SerialDebug.println("ThreadingMode::grabCurrentZAsUiAutoStopTarget - MC not available.");
        return;
    }
    _ui_targetStopAbsoluteSteps = _motionControl->getCurrentPositionSteps();
    _ui_targetStopIsSet = true;

    SerialDebug.print("ThreadingMode: UI Auto-stop target grabbed as current Z (abs steps): ");
    SerialDebug.println(_ui_targetStopAbsoluteSteps);

    if (_ui_autoStopEnabled)
    {
        _motionControl->configureAbsoluteTargetStop(_ui_targetStopAbsoluteSteps, true);
    }
}

String ThreadingMode::getFormattedUiAutoStopTarget() const
{
    if (!_ui_targetStopIsSet || !_motionControl)
    {
        return String("--- ") + (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? "mm" : "in");
    }

    // Convert the absolute target steps back to a displayable travel distance from Z-zero
    int32_t travel_steps = _ui_targetStopAbsoluteSteps - _z_axis_zero_offset_steps;
    float travel_mm = _motionControl->convertStepsToUnits(abs(travel_steps));

    float displayValue;
    String unitStr;

    if (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        displayValue = travel_mm;
        unitStr = "mm";
    }
    else
    {
        displayValue = travel_mm / 25.4f; // Convert mm to inches
        unitStr = "in";
    }

    char buffer[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH];
    snprintf(buffer, sizeof(buffer), "%.2f %s", displayValue, unitStr.c_str());
    return String(buffer);
}

bool ThreadingMode::checkAndHandleAutoStopCompletion()
{
    if (_motionControl && _motionControl->wasTargetStopReachedAndMotionHalted())
    {
        SerialDebug.println("ThreadingMode: Auto-stop completion detected from MotionControl.");
        _ui_targetStopIsSet = false;
        _autoStopCompletionPendingHmiSignal = true;
        return true;
    }
    return false;
}

bool ThreadingMode::isAutoStopCompletionPendingHmiSignal() const
{
    return _autoStopCompletionPendingHmiSignal;
}

void ThreadingMode::clearAutoStopCompletionHmiSignal()
{
    _autoStopCompletionPendingHmiSignal = false;
}
