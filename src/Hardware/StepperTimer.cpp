#include "Hardware/StepperTimer.h"
#include "Config/serial_debug.h"

StepperTimer::StepperTimer()
    : _stepper(nullptr),
      _currentMode(Mode::IDLE),
      _error(false),
      _errorMsg(nullptr)
{
}

StepperTimer::~StepperTimer()
{
    end();
}

bool StepperTimer::begin(const PinConfig& pins)
{
    // Create stepper instance
    _stepper = new STM32Step::Stepper(pins.step_pin, pins.dir_pin, pins.enable_pin);
    
    if (!_stepper) {
        handleError("Failed to allocate stepper driver");
        return false;
    }
    
    // Configure initial parameters
    _stepper->setMicrosteps(SystemConfig::RuntimeConfig::Stepper::microsteps);
    
    // Determine mode and configure accordingly
    configureForMode(_currentMode);
    
    SerialDebug.println("Stepper driver initialized");
    return true;
}

void StepperTimer::end()
{
    disable();
    
    if (_stepper) {
        delete _stepper;
        _stepper = nullptr;
    }
}

void StepperTimer::enable(bool enable)
{
    if (!_stepper) return;
    
    if (enable) {
        _stepper->enable();
    } else {
        _stepper->disable();
    }
}

void StepperTimer::disable()
{
    if (!_stepper) return;
    
    _stepper->disable();
}

void StepperTimer::setMode(Mode mode)
{
    if (mode == _currentMode) return;
    
    _currentMode = mode;
    configureForMode(mode);
}

void StepperTimer::setMicrosteps(uint16_t microsteps)
{
    if (!_stepper) return;
    
    _stepper->setMicrosteps(microsteps);
}

bool StepperTimer::setSpeed(uint32_t steps_per_second)
{
    if (!_stepper) return false;
    
    if (steps_per_second > SystemConfig::Limits::Stepper::MAX_SPEED) {
        steps_per_second = SystemConfig::Limits::Stepper::MAX_SPEED;
        SerialDebug.println("Warning: Speed limited to maximum");
    }
    
    // STM32Step library doesn't have setMaxSpeed method
    // Timer frequency adjustment would be needed
    
    return true;
}

void StepperTimer::setPosition(int32_t position)
{
    if (!_stepper) return;
    
    _stepper->setTargetPosition(position);
}

void StepperTimer::setRelativePosition(int32_t steps)
{
    if (!_stepper) return;
    
    _stepper->setRelativePosition(steps);
}

int32_t StepperTimer::getPosition() const
{
    if (!_stepper) return 0;
    
    return _stepper->getCurrentPosition();
}

void StepperTimer::stop()
{
    if (!_stepper) return;
    
    _stepper->stop();
}

void StepperTimer::emergencyStop()
{
    if (!_stepper) return;
    
    _stepper->emergencyStop();
}

void StepperTimer::resetPosition()
{
    if (!_stepper) return;
    
    // The STM32Step library doesn't have a resetPosition method
    // Set current position to 0 by calling incrementCurrentPosition
    int32_t currentPos = _stepper->getCurrentPosition();
    _stepper->incrementCurrentPosition(-currentPos);
}

StepperTimer::Status StepperTimer::getStatus() const
{
    Status status = {0};
    
    if (!_stepper) {
        status.error = true;
        status.error_message = "Stepper not initialized";
        return status;
    }
    
    STM32Step::StepperStatus stepper_status = _stepper->getStatus();
    
    status.position = stepper_status.currentPosition;
    status.target_position = stepper_status.targetPosition;
    status.speed = 0; // Speed isn't part of StepperStatus
    status.enabled = stepper_status.enabled;
    status.running = stepper_status.running;
    status.error = _error;
    status.error_message = _errorMsg;
    
    return status;
}

bool StepperTimer::isRunning() const
{
    if (!_stepper) return false;
    
    return _stepper->getStatus().running;
}

bool StepperTimer::hasError() const
{
    return _error;
}

const char* StepperTimer::getErrorMessage() const
{
    return _errorMsg;
}

void StepperTimer::configureForMode(Mode mode)
{
    if (!_stepper) return;
    
    switch (mode) {
        case Mode::IDLE:
            _stepper->setOperationMode(STM32Step::OperationMode::IDLE);
            break;
            
        case Mode::TURNING:
            _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
            break;
            
        case Mode::THREADING:
            _stepper->setOperationMode(STM32Step::OperationMode::THREADING);
            break;
            
        case Mode::MANUAL:
            _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
            break;
    }
}

void StepperTimer::handleError(const char* msg)
{
    _error = true;
    _errorMsg = msg;
    
    SerialDebug.print("StepperTimer error: ");
    SerialDebug.println(msg);
}