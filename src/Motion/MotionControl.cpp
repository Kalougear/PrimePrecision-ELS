#include "Motion/MotionControl.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"
#include <STM32Step.h>
#include "Hardware/EncoderTimer.h"
#include "Motion/SyncTimer.h"
#include <cmath>

MotionControl::MotionControl() : _stepper(nullptr),
                                 _currentMode(Mode::IDLE),
                                 _running(false),
                                 _jogActive(false),
                                 _error(false),
                                 _errorMsg(nullptr),
                                 _currentFeedDirection(FeedDirection::UNKNOWN),
                                 _targetStopFeatureEnabledForMotion(false),
                                 _absoluteTargetStopStepsForMotion(0),
                                 _targetStopReached(false)
{
}

MotionControl::MotionControl(const MotionPins &pins) : _pins(pins),
                                                       _stepper(nullptr),
                                                       _currentMode(Mode::IDLE),
                                                       _running(false),
                                                       _jogActive(false),
                                                       _error(false),
                                                       _errorMsg(nullptr),
                                                       _currentFeedDirection(FeedDirection::UNKNOWN),
                                                       _targetStopFeatureEnabledForMotion(false),
                                                       _absoluteTargetStopStepsForMotion(0),
                                                       _targetStopReached(false)
{
}

MotionControl::~MotionControl()
{
    end();
}

bool MotionControl::begin(EncoderTimer *encoder)
{
    _encoder = encoder;

    STM32Step::TimerControl::init();

    if (!_encoder || !_encoder->isValid())
    {
        handleError("Encoder provided to MotionControl is not valid.");
        return false;
    }

    _stepper = new STM32Step::Stepper(_pins.step_pin, _pins.dir_pin, _pins.enable_pin);
    if (!_stepper)
    {
        handleError("Stepper motor allocation failed in MotionControl");
        return false;
    }

    if (!_syncTimer.begin(_encoder, _stepper))
    {
        handleError("Sync timer initialization failed in MotionControl");
        return false;
    }

    _error = false;
    return true;
}

void MotionControl::end()
{
    stopMotion();
    if (_jogActive && _stepper)
    {
        _stepper->stop();
        _jogActive = false;
    }
    if (_stepper)
    {
        _stepper->disable();
        delete _stepper;
        _stepper = nullptr;
    }
    _syncTimer.end();
    _running = false;
    _error = false;
}

void MotionControl::setMode(Mode mode)
{
    if (_jogActive && _stepper)
    {
        endContinuousJog();
    }
    if (mode == _currentMode && _running)
    {
        return;
    }

    stopMotion();
    _currentMode = mode;
    configureForMode(mode);
}

void MotionControl::setConfig(const Config &config)
{
    bool was_running = _running;
    if (was_running)
    {
        stopMotion();
    }

    _config = config;
    updateSyncParameters();

    if (was_running)
    {
        startMotion();
    }
}

void MotionControl::startMotion()
{
    if (_jogActive && _stepper)
    {
        endContinuousJog();
    }
    if (_running || _error)
    {
        return;
    }

    if (!_stepper || !_encoder->isValid() || !_syncTimer.isInitialized())
    {
        handleError("Cannot start motion: Components not initialized.");
        return;
    }

    calculateAndSetSyncTimerConfig();

    _encoder->reset();
    _stepper->enable();
    _syncTimer.enable(true);

    bool steps_will_increase = (_config.thread_pitch >= 0.0f) ^ _config.reverse_direction;
    if (steps_will_increase)
    {
        _currentFeedDirection = FeedDirection::AWAY_FROM_CHUCK;
    }
    else
    {
        _currentFeedDirection = FeedDirection::TOWARDS_CHUCK;
    }

    _running = true;
    _error = false;
}

void MotionControl::stopMotion()
{
    if (!_running)
    {
        return;
    }

    _syncTimer.enable(false);
    if (_stepper)
    {
        _stepper->stop();
    }
    _running = false;
}

void MotionControl::emergencyStop()
{
    if (_jogActive && _stepper)
    {
        _stepper->emergencyStop();
        _jogActive = false;
    }
    _syncTimer.enable(false);
    if (_stepper)
    {
        _stepper->emergencyStop();
    }
    _running = false;
    handleError("Emergency stop triggered");
}

MotionControl::Status MotionControl::getStatus() const
{
    Status status;
    EncoderTimer::Position encPos = _encoder->getPosition();
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
    status.spindle_rpm = static_cast<int16_t>(roundf(actualSpindleRpm));

    status.stepper_position = _stepper ? _stepper->getCurrentPosition() : 0;
    status.error = _error;
    status.error_message = _errorMsg;
    return status;
}

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
    if (!_jogActive)
    {
        updateSyncParameters();
    }
}

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

void MotionControl::updateSyncParameters()
{
    if (!_stepper)
        return;

    _stepper->setMicrosteps(_config.microsteps);
    calculateAndSetSyncTimerConfig();
}

void MotionControl::update()
{
    // Debug: Uncomment to monitor sync stats
    // _syncTimer.printDebugInfo();

    // Auto-stop logic needs rework due to stepper position not being live-updated by ISR.
    // This will be addressed in a separate task.
    /*
    if (_running && _targetStopFeatureEnabledForMotion && !_targetStopReached)
    {
        if (!_stepper)
            return;
        int32_t currentAbsoluteSteps = _stepper->getCurrentPosition();

        bool movingTowardsIncreasingSteps = (_currentFeedDirection == FeedDirection::AWAY_FROM_CHUCK);

        if (movingTowardsIncreasingSteps)
        {
            if (currentAbsoluteSteps >= _absoluteTargetStopStepsForMotion)
            {
                requestImmediateStop(StopType::CONTROLLED_DECELERATION);
                _targetStopReached = true;
                _targetStopFeatureEnabledForMotion = false;
            }
        }
        else
        {
            if (currentAbsoluteSteps <= _absoluteTargetStopStepsForMotion)
            {
                requestImmediateStop(StopType::CONTROLLED_DECELERATION);
                _targetStopReached = true;
                _targetStopFeatureEnabledForMotion = false;
            }
        }
    }
    */
}

void MotionControl::calculateAndSetSyncTimerConfig()
{
    if (!_stepper || !_syncTimer.isInitialized())
    {
        return;
    }
    double target_feed_mm_spindle_rev = std::abs(static_cast<double>(_config.thread_pitch));
    double enc_ppr = static_cast<double>(SystemConfig::RuntimeConfig::Encoder::ppr);
    double quad_mult = static_cast<double>(SystemConfig::Limits::Encoder::QUADRATURE_MULT);
    double spindle_pulley_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Spindle::chuck_pulley_teeth);
    double encoder_pulley_teeth = static_cast<double>(SystemConfig::RuntimeConfig::Spindle::encoder_pulley_teeth);

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
    double ls_pitch_mm_per_ls_rev = ls_is_metric ? ls_pitch_val : ls_pitch_val * 25.4;
    if (std::abs(ls_pitch_mm_per_ls_rev) < 0.000001)
        ls_pitch_mm_per_ls_rev = 1.0;
    double mm_travel_per_motor_rev = G_ls_rev_per_motor_rev * ls_pitch_mm_per_ls_rev;
    if (std::abs(mm_travel_per_motor_rev) < 0.000001)
        mm_travel_per_motor_rev = 1.0;
    double usteps_per_mm_travel = s_motor_total_usteps / mm_travel_per_motor_rev;
    double calculated_steps_per_encoder_tick = (target_feed_mm_spindle_rev / enc_counts_per_spindle_rev) * usteps_per_mm_travel;

    SyncTimer::SyncConfig newSyncTimerConfig;
    newSyncTimerConfig.scaling_factor = 1000000;
    newSyncTimerConfig.steps_per_encoder_tick_scaled = static_cast<uint32_t>(calculated_steps_per_encoder_tick * newSyncTimerConfig.scaling_factor);
    newSyncTimerConfig.update_freq = _config.sync_frequency;
    // Combine pitch sign and config reversal to determine final direction
    // pitch < 0 means "towards chuck" (reverse), unless reversed by config.
    newSyncTimerConfig.reverse_direction = (_config.thread_pitch < 0.0f) ^ _config.reverse_direction;
    _syncTimer.setConfig(newSyncTimerConfig);
}

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

    if (!_running && (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING))
    {
        startMotion();
    }
}

void MotionControl::disableMotor()
{
    if (_jogActive && _stepper)
    {
        endContinuousJog();
    }
    stopMotion();
    if (_stepper)
    {
        _stepper->disable();
    }
}

void MotionControl::beginContinuousJog(JogDirection direction, float speed_mm_per_min)
{
    if (_error || !_stepper)
        return;
    if (!SystemConfig::RuntimeConfig::System::jog_system_enabled)
        return;
    if (_running)
        stopMotion();
    _syncTimer.enable(false);

    if (direction == JogDirection::JOG_NONE)
    {
        endContinuousJog();
        return;
    }

    _stepper->enable();

    float target_speed_mm_per_min = speed_mm_per_min;
    if (target_speed_mm_per_min < 0.0f)
        target_speed_mm_per_min = 0.0f;
    if (target_speed_mm_per_min > SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min)
    {
        target_speed_mm_per_min = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
    }

    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f;
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f;

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : z_ls_pitch_val * 25.4f;
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        z_ls_pitch_mm_per_ls_rev = 1.0f;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        z_mm_travel_per_motor_rev = 1.0f;

    float z_usteps_per_mm_travel = z_motor_total_usteps / z_mm_travel_per_motor_rev;

    float speed_mm_per_sec = target_speed_mm_per_min / 60.0f;
    float target_freq_hz = speed_mm_per_sec * z_usteps_per_mm_travel;

    float accel_mm_per_s2 = SystemConfig::RuntimeConfig::Z_Axis::acceleration;
    float accel_steps_per_s2 = accel_mm_per_s2 * z_usteps_per_mm_travel;

    bool stepper_lib_direction = (direction == JogDirection::JOG_AWAY_FROM_CHUCK);

    _stepper->setAcceleration(accel_steps_per_s2);
    _stepper->setSpeedHz(target_freq_hz);
    _stepper->runContinuous(stepper_lib_direction);

    _jogActive = true;
}

void MotionControl::endContinuousJog()
{
    if (!_jogActive || !_stepper)
    {
        return;
    }

    _stepper->stop();
    _jogActive = false;

    if (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING)
    {
        startMotion();
    }
}

void MotionControl::configureAbsoluteTargetStop(int32_t absoluteSteps, bool enable)
{
    _absoluteTargetStopStepsForMotion = absoluteSteps;
    _targetStopFeatureEnabledForMotion = enable;
    _targetStopReached = false;

    if (enable)
    {
        if (isMotorEnabled() && !_running &&
            (_currentMode == Mode::TURNING || _currentMode == Mode::THREADING || _currentMode == Mode::FEEDING))
        {
            startMotion();
        }
    }
}

void MotionControl::clearAbsoluteTargetStop()
{
    _targetStopFeatureEnabledForMotion = false;
    _absoluteTargetStopStepsForMotion = 0;
    _targetStopReached = false;
}

bool MotionControl::wasTargetStopReachedAndMotionHalted()
{
    bool status = _targetStopReached;
    if (status)
    {
        _targetStopReached = false;
    }
    return status;
}

MotionControl::FeedDirection MotionControl::getCurrentFeedDirection() const
{
    return _currentFeedDirection;
}

int32_t MotionControl::getCurrentPositionSteps() const
{
    if (_stepper)
    {
        return _stepper->getCurrentPosition();
    }
    return 0;
}

void MotionControl::requestImmediateStop(StopType type)
{
    if (_jogActive && _stepper)
    {
        if (type == StopType::IMMEDIATE_HALT)
        {
            _stepper->emergencyStop();
        }
        else
        {
            _stepper->stop();
        }
        _jogActive = false;
    }

    if (_running)
    {
        _syncTimer.enable(false);
        if (_stepper)
        {
            if (type == StopType::IMMEDIATE_HALT)
            {
                _stepper->emergencyStop();
            }
            else
            {
                _stepper->stop();
            }
        }
        _running = false;
    }
    else if (_stepper && !_jogActive && !_running)
    {
        if (type == StopType::IMMEDIATE_HALT)
        {
            _stepper->emergencyStop();
        }
        else
        {
            _stepper->stop();
        }
    }
}

bool MotionControl::isJogActive() const
{
    return _jogActive;
}

bool MotionControl::isElsActive() const
{
    return _running && (_currentMode == Mode::THREADING || _currentMode == Mode::TURNING || _currentMode == Mode::FEEDING);
}

int32_t MotionControl::convertUnitsToSteps(float units) const
{
    float valueInMm = units;
    if (!SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        valueInMm = units * 25.4f;
    }

    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f;
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f;

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : (z_ls_pitch_val > 0 ? 25.4f / z_ls_pitch_val : 0.0f);
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        return 0;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        return 0;

    float z_usteps_per_mm_travel = z_motor_total_usteps / z_mm_travel_per_motor_rev;

    return static_cast<int32_t>(roundf(valueInMm * z_usteps_per_mm_travel));
}

float MotionControl::convertStepsToUnits(int32_t steps) const
{
    float z_motor_total_usteps = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);
    float z_motor_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::motor_pulley_teeth);
    if (z_motor_pulley_teeth < 1.0f)
        z_motor_pulley_teeth = 1.0f;
    float z_leadscrew_pulley_teeth = static_cast<float>(SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pulley_teeth);
    if (z_leadscrew_pulley_teeth < 1.0f)
        z_leadscrew_pulley_teeth = 1.0f;

    float z_ls_pitch_val = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    bool z_ls_is_metric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
    float z_ls_pitch_mm_per_ls_rev = z_ls_is_metric ? z_ls_pitch_val : (z_ls_pitch_val > 0 ? 25.4f / z_ls_pitch_val : 0.0f);
    if (fabsf(z_ls_pitch_mm_per_ls_rev) < 0.00001f)
        return 0.0f;

    float z_mm_travel_per_motor_rev = (z_motor_pulley_teeth / z_leadscrew_pulley_teeth) * z_ls_pitch_mm_per_ls_rev;
    if (fabsf(z_mm_travel_per_motor_rev) < 0.00001f)
        return 0.0f;

    float valueInMm = static_cast<float>(steps) * (z_mm_travel_per_motor_rev / z_motor_total_usteps);

    if (!SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
    {
        return valueInMm / 25.4f;
    }

    return valueInMm;
}
