#include "Application/MotionControl.h"
#include "Config/serial_debug.h"
#include <STM32Step.h>
#include "Core/encoder_timer.h"

MotionControl::MotionControl() : _stepper(nullptr),
                                 _currentMode(Mode::IDLE),
                                 _running(false),
                                 _error(false),
                                 _errorMsg(nullptr)
{
}

MotionControl::MotionControl(const MotionPins &pins) : _pins(pins),
                                                       _stepper(nullptr),
                                                       _currentMode(Mode::IDLE),
                                                       _running(false),
                                                       _error(false),
                                                       _errorMsg(nullptr)
{
}

MotionControl::~MotionControl()
{
    end();
}

bool MotionControl::begin()
{
    // Initialize encoder
    if (!_encoder.begin())
    {
        handleError("Encoder initialization failed");
        return false;
    }

    // Initialize stepper
    _stepper = new STM32Step::Stepper(_pins.step_pin, _pins.dir_pin, _pins.enable_pin);
    if (!_stepper)
    {
        handleError("Stepper allocation failed");
        return false;
    }

    // Initialize sync timer
    if (!_syncTimer.begin())
    {
        handleError("Sync timer initialization failed");
        return false;
    }

    return true;
}

void MotionControl::end()
{
    stopMotion();
    if (_stepper)
    {
        _stepper->disable();
        delete _stepper;
        _stepper = nullptr;
    }
    _encoder.end();
    _syncTimer.end();
}

void MotionControl::setMode(Mode mode)
{
    if (mode == _currentMode)
    {
        return;
    }

    stopMotion();
    _currentMode = mode;
    configureForMode(mode);
}

void MotionControl::setConfig(const Config &config)
{
    _config = config;
    updateSyncParameters();
}

void MotionControl::startMotion()
{
    if (_running || _error)
    {
        return;
    }

    // Configure sync timer
    SyncTimer::SyncConfig syncConfig;
    syncConfig.thread_pitch = _config.thread_pitch;
    syncConfig.leadscrew_pitch = _config.leadscrew_pitch;
    syncConfig.update_freq = _config.sync_frequency;
    syncConfig.reverse_direction = _config.reverse_direction;
    _syncTimer.setConfig(syncConfig);

    // Start components
    _encoder.reset();
    _stepper->enable();
    _syncTimer.enable(true);

    _running = true;
}

void MotionControl::stopMotion()
{
    if (!_running)
    {
        return;
    }

    _syncTimer.enable(false);
    _stepper->stop();
    _running = false;
}

void MotionControl::emergencyStop()
{
    _syncTimer.enable(false);
    _stepper->emergencyStop();
    _running = false;
    handleError("Emergency stop triggered");
}

MotionControl::Status MotionControl::getStatus() const
{
    Status status;
    status.encoder_position = const_cast<EncoderTimer &>(_encoder).getCount();
    status.stepper_position = _stepper ? _stepper->getCurrentPosition() : 0;
    status.spindle_rpm = const_cast<EncoderTimer &>(_encoder).getRPM();
    status.error = _error;
    status.error_message = _errorMsg;
    return status;
}

void MotionControl::configureForMode(Mode mode)
{
    switch (mode)
    {
    case Mode::THREADING:
        _stepper->setOperationMode(STM32Step::OperationMode::THREADING);
        break;
    case Mode::TURNING:
        _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
        break;
    case Mode::FEEDING:
        _stepper->setOperationMode(STM32Step::OperationMode::TURNING);
        break;
    case Mode::IDLE:
        _stepper->setOperationMode(STM32Step::OperationMode::IDLE);
        break;
    }
    updateSyncParameters();
}

void MotionControl::handleError(const char *msg)
{
    _error = true;
    _errorMsg = msg;
    stopMotion();
}

void MotionControl::updateSyncParameters()
{
    if (!_stepper)
        return;

    _stepper->setMicrosteps(_config.microsteps);

    // Update sync timer if running
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

// Process sync requests outside interrupt context
void MotionControl::update()
{
    if (_running && !_error)
    {
        _syncTimer.processSyncRequest();
    }
}
