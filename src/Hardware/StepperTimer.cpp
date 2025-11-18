#include "Hardware/StepperTimer.h"

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

bool StepperTimer::begin(const PinConfig &pins)
{
    _stepper = new STM32Step::Stepper(pins.step_pin, pins.dir_pin, pins.enable_pin);
    if (!_stepper)
    {
        handleError("Stepper allocation failed");
        return false;
    }
    return true;
}

void StepperTimer::end()
{
    if (_stepper)
    {
        delete _stepper;
        _stepper = nullptr;
    }
}

void StepperTimer::enable(bool enable)
{
    if (_stepper)
    {
        if (enable)
            _stepper->enable();
        else
            _stepper->disable();
    }
}

void StepperTimer::disable()
{
    if (_stepper)
    {
        _stepper->disable();
    }
}

void StepperTimer::setMode(Mode mode)
{
    _currentMode = mode;
    configureForMode(mode);
}

void StepperTimer::setMicrosteps(uint16_t microsteps)
{
    if (_stepper)
    {
        _stepper->setMicrosteps(microsteps);
    }
}

bool StepperTimer::setSpeed(uint32_t steps_per_second)
{
    if (_stepper)
    {
        _stepper->setSpeed(steps_per_second);
        return true;
    }
    return false;
}

void StepperTimer::setPosition(int32_t position)
{
    if (_stepper)
    {
        _stepper->setPosition(position);
    }
}

void StepperTimer::setRelativePosition(int32_t steps)
{
    if (_stepper)
    {
        _stepper->setRelativePosition(steps);
    }
}

int32_t StepperTimer::getPosition() const
{
    if (_stepper)
    {
        return _stepper->getCurrentPosition();
    }
    return 0;
}

void StepperTimer::stop()
{
    if (_stepper)
    {
        _stepper->stop();
    }
}

void StepperTimer::emergencyStop()
{
    if (_stepper)
    {
        _stepper->emergencyStop();
    }
}

void StepperTimer::resetPosition()
{
    if (_stepper)
    {
        _stepper->resetPosition();
    }
}

StepperTimer::Status StepperTimer::getStatus() const
{
    Status status = {0};
    if (_stepper)
    {
        status.position = _stepper->getCurrentPosition();
        status.target_position = _stepper->getTargetPosition();
        status.speed = _stepper->getCurrentSpeed();
        status.enabled = _stepper->isEnabled();
        status.running = _stepper->isRunning();
    }
    status.error = _error;
    status.error_message = _errorMsg;
    return status;
}

bool StepperTimer::isRunning() const
{
    if (_stepper)
    {
        return _stepper->isRunning();
    }
    return false;
}

bool StepperTimer::hasError() const
{
    return _error;
}

const char *StepperTimer::getErrorMessage() const
{
    return _errorMsg;
}

void StepperTimer::configureForMode(Mode mode)
{
    if (!_stepper)
        return;

    switch (mode)
    {
    case Mode::THREADING:
        _stepper->setOperationMode(STM32Step::OperationMode::THREADING);
        break;
    case Mode::TURNING:
        _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
        break;
    case Mode::MANUAL:
    case Mode::IDLE:
    default:
        _stepper->setOperationMode(STM32Step::OperationMode::IDLE);
        break;
    }
}

void StepperTimer::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
}
