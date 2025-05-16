#include "Motion/TurningMode.h"
#include "Config/SystemConfig.h"
#include "Config/serial_debug.h"

TurningMode::TurningMode()
    : _motionControl(nullptr),
      _positioning(nullptr),
      _feedRate(FeedRate::F0_10),
      _mode(Mode::MANUAL),
      _running(false),
      _error(false),
      _errorMsg(nullptr)
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

    // Configure feed rate
    configureFeedRate();

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

void TurningMode::setFeedRate(FeedRate rate)
{
    _feedRate = rate;
    if (_motionControl)
    {
        configureFeedRate();
    }
}

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

    // Start motion
    _motionControl->startMotion();
    _running = true;
}

void TurningMode::stop()
{
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
    if (!_running || !_motionControl)
    {
        return;
    }

    // Update motion controller
    _motionControl->update();

    // In semi-auto mode, check if we've reached the end position
    if (_mode == Mode::SEMI_AUTO && _positioning)
    {
        float currentPos = getCurrentPosition();
        if (_positioning->hasReachedEndPosition(currentPos))
        {
            SerialDebug.println("Reached end position, stopping");
            stop();
        }
    }
}

float TurningMode::getCurrentPosition() const
{
    if (!_motionControl)
    {
        return 0.0f;
    }

    MotionControl::Status status = _motionControl->getStatus();
    // Convert stepper position to mm based on leadscrew pitch and microsteps
    float leadscrewPitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch;
    float stepsPerRev = SystemConfig::Limits::Stepper::STEPS_PER_REV *
                        SystemConfig::RuntimeConfig::Stepper::microsteps;

    return (status.stepper_position / stepsPerRev) * leadscrewPitch;
}

float TurningMode::getFeedRateValue() const
{
    switch (_feedRate)
    {
    case FeedRate::F0_01:
        return 0.01f;
    case FeedRate::F0_02:
        return 0.02f;
    case FeedRate::F0_05:
        return 0.05f;
    case FeedRate::F0_10:
        return 0.10f;
    case FeedRate::F0_20:
        return 0.20f;
    case FeedRate::F0_50:
        return 0.50f;
    case FeedRate::F1_00:
        return 1.00f;
    default:
        return 0.10f;
    }
}

bool TurningMode::getCurrentFeedRateWarning() const
{
    // This assumes _feedRateManager is kept in sync with _feedRate
    // A future refactor should make FeedRateManager the source of truth.
    return _feedRateManager.getCurrentWarning();
}

void TurningMode::configureFeedRate()
{
    if (!_motionControl)
    {
        return;
    }

    // Configure motion control with feed rate as thread pitch
    MotionControl::Config config;
    config.thread_pitch = getFeedRateValue();
    config.leadscrew_pitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch;
    config.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    config.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    config.reverse_direction = SystemConfig::RuntimeConfig::Stepper::invert_direction;
    config.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;

    _motionControl->setConfig(config);
}

void TurningMode::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
    stop();

    SerialDebug.print("TurningMode error: ");
    SerialDebug.println(msg);
}
