#include "Motion/ThreadingMode.h"
#include "Config/SystemConfig.h"
#include "Config/serial_debug.h"

ThreadingMode::ThreadingMode()
    : _motionControl(nullptr),
      _positioning(nullptr),
      _running(false),
      _error(false),
      _errorMsg(nullptr)
{
    // Initialize thread data
    _threadData.pitch = 1.0f;
    _threadData.starts = 1;
    _threadData.units = Units::METRIC;
    _threadData.type = ThreadType::STANDARD;
    _threadData.valid = false;
    
    // Initialize position data
    _positions.start_position = 0.0f;
    _positions.end_position = 0.0f;
    _positions.valid = false;
}

ThreadingMode::~ThreadingMode()
{
    end();
}

bool ThreadingMode::begin(MotionControl* motion_control)
{
    if (!motion_control) {
        handleError("Invalid motion control reference");
        return false;
    }
    
    _motionControl = motion_control;
    _positioning = new Positioning();
    
    if (!_positioning) {
        handleError("Failed to allocate positioning system");
        return false;
    }
    
    // Configure motion controller for threading mode
    _motionControl->setMode(MotionControl::Mode::THREADING);
    
    // Configure threading parameters
    configureThreading();
    
    return true;
}

void ThreadingMode::end()
{
    stop();
    
    if (_positioning) {
        delete _positioning;
        _positioning = nullptr;
    }
    
    _motionControl = nullptr;
}

void ThreadingMode::setThreadData(const ThreadData& thread_data)
{
    _threadData = thread_data;
    if (_motionControl) {
        configureThreading();
    }
}

void ThreadingMode::setPositions(const Position& positions)
{
    _positions = positions;
}

void ThreadingMode::enableMultiStart(bool enable)
{
    if (enable) {
        // Default to 2 starts if currently at 1
        if (_threadData.starts <= 1) {
            _threadData.starts = 2;
        }
    } else {
        _threadData.starts = 1;
    }
    
    if (_motionControl) {
        configureThreading();
    }
}

void ThreadingMode::start()
{
    if (_running || _error || !_motionControl) {
        return;
    }
    
    // Validate thread data
    if (!_threadData.valid) {
        handleError("Invalid thread data");
        return;
    }
    
    // Validate positions for auto stop
    if (_positions.valid) {
        _positioning->reset();
        _positioning->setEndPosition(_positions.end_position);
    }
    
    // Start motion
    _motionControl->startMotion();
    _running = true;
}

void ThreadingMode::stop()
{
    if (!_running) {
        return;
    }
    
    if (_motionControl) {
        _motionControl->stopMotion();
    }
    
    _running = false;
}

void ThreadingMode::update()
{
    if (!_running || !_motionControl) {
        return;
    }
    
    // Update motion controller
    _motionControl->update();
    
    // Check if we've reached the end position (if valid)
    if (_positions.valid && _positioning) {
        float currentPos = getCurrentPosition();
        if (_positioning->hasReachedEndPosition(currentPos)) {
            SerialDebug.println("Thread complete, stopping");
            stop();
        }
    }
}

float ThreadingMode::getCurrentPosition() const
{
    if (!_motionControl) {
        return 0.0f;
    }
    
    MotionControl::Status status = _motionControl->getStatus();
    // Convert stepper position to mm based on leadscrew pitch and microsteps
    float leadscrewPitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch;
    float stepsPerRev = SystemConfig::Limits::Stepper::STEPS_PER_REV * 
                        SystemConfig::RuntimeConfig::Stepper::microsteps;
    
    return (status.stepper_position / stepsPerRev) * leadscrewPitch;
}

float ThreadingMode::getEffectivePitch() const
{
    // For multi-start threads, effective pitch = pitch * starts
    float effectivePitch = _threadData.pitch;
    
    // For imperial threads, convert TPI to mm pitch
    if (_threadData.units == Units::IMPERIAL) {
        effectivePitch = convertToMetricPitch(effectivePitch);
    }
    
    // Adjust for multi-start threads
    if (_threadData.starts > 1) {
        effectivePitch *= _threadData.starts;
    }
    
    return effectivePitch;
}

void ThreadingMode::configureThreading()
{
    if (!_motionControl) {
        return;
    }
    
    // Calculate effective thread pitch
    float effectivePitch = getEffectivePitch();
    
    // Configure motion control
    MotionControl::Config config;
    config.thread_pitch = effectivePitch;
    config.leadscrew_pitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch;
    config.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    config.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    config.reverse_direction = SystemConfig::RuntimeConfig::Stepper::invert_direction;
    config.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;
    
    _motionControl->setConfig(config);
    
    SerialDebug.print("Threading mode configured: Pitch=");
    SerialDebug.print(effectivePitch);
    SerialDebug.print("mm, Starts=");
    SerialDebug.println(_threadData.starts);
}

float ThreadingMode::convertToMetricPitch(float imperial_tpi) const
{
    // Convert TPI (threads per inch) to mm pitch
    // Pitch in mm = 25.4 / TPI
    if (imperial_tpi <= 0) return 1.0f; // Default to 1mm if invalid
    return 25.4f / imperial_tpi;
}

void ThreadingMode::handleError(const char* msg)
{
    _error = true;
    _errorMsg = msg;
    stop();
    
    SerialDebug.print("ThreadingMode error: ");
    SerialDebug.println(msg);
}