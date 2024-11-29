#include "stepper.h"
#include "Config/serial_debug.h"
#include <algorithm>

namespace STM32Step
{
    Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
        : _stepPin(stepPin),
          _dirPin(dirPin),
          _enablePin(enablePin),
          _currentPosition(0),
          _targetPosition(0),
          _currentSpeed(0),
          _enabled(false),
          _running(false),
          _currentDirection(false),
          _directionChanged(false),
          _mode(OperationMode::TURNING)
    {
        // Initialize configuration with runtime defaults
        _config.microsteps = RuntimeConfig::current_microsteps;
        _config.maxSpeed = RuntimeConfig::current_max_speed;
        _config.mode = OperationMode::TURNING;
        _stepsPerFullStep = _config.microsteps;

        initPins();
    }

    bool Stepper::moveSteps(int32_t steps, bool wait)
    {
        if (!_enabled)
        {
            SerialDebug.println("ERROR: Motor not enabled");
            return false;
        }

        if (steps == 0)
        {
            SerialDebug.println("Warning: Zero steps requested");
            return true;
        }

        // Stop any ongoing movement
        if (_running)
        {
            TimerControl::stop();
            _running = false;
        }

        // Set target position
        _targetPosition = _currentPosition + steps;

        // Set direction
        bool newDirection = steps >= 0;
        if (_currentDirection != newDirection)
        {
            _currentDirection = newDirection;
            setDirection(_currentDirection);
            delayMicroseconds(TimingConfig::DIR_SETUP);
        }

        // Start motion
        _running = true;
        TimerControl::start(this);

        if (wait)
        {
            const uint32_t MOVEMENT_TIMEOUT = 10000; // 10 seconds timeout
            const uint32_t STATUS_INTERVAL = 100;    // Status update every 100ms
            uint32_t lastPrint = 0;
            uint32_t startTime = millis();

            while (_running && !TimerControl::isTargetPositionReached())
            {
                uint32_t currentTime = millis();

                // Print status updates periodically
                if (currentTime - lastPrint >= STATUS_INTERVAL)
                {
                    SerialDebug.print("Position: ");
                    SerialDebug.print(_currentPosition);
                    SerialDebug.print(" Target: ");
                    SerialDebug.print(_targetPosition);
                    SerialDebug.print(" Speed: ");
                    SerialDebug.println(_currentSpeed);
                    lastPrint = currentTime;
                }

                // Check for timeout
                if (currentTime - startTime > MOVEMENT_TIMEOUT)
                {
                    SerialDebug.println("ERROR: Movement timeout!");
                    stop();
                    return false;
                }
                delay(1);
            }
            return TimerControl::isTargetPositionReached();
        }
        return true;
    }

    void Stepper::setSpeed(uint32_t speed)
    {
        speed = validateSpeed(speed);
        _currentSpeed = speed;

        if (_enabled && _running)
        {
            TimerControl::updateTimerFrequency(speed);
        }
    }

    void Stepper::enable()
    {
        if (_enabled)
            return;

        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, GPIO_PIN_7,
                          RuntimeConfig::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false;
    }

    void Stepper::disable()
    {
        if (!_enabled)
            return;

        stop();
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, GPIO_PIN_7, GPIO_PIN_SET);
        _enabled = false;
        _running = false;
    }

    void Stepper::stop()
    {
        if (!_running)
            return;

        _running = false;
        TimerControl::stop();
        // Don't reset _currentSpeed here to preserve the last set speed
    }

    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // Configure direction pin
        GPIO_InitStruct.Pin = 1 << (_dirPin & 0x0F);
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

        // Initialize direction pin
        HAL_GPIO_Init(PinConfig::DirPin::PORT, &GPIO_InitStruct);

        // Configure enable pin
        GPIO_InitStruct.Pin = 1 << (_enablePin & 0x0F);
        HAL_GPIO_Init(PinConfig::EnablePin::PORT, &GPIO_InitStruct);

        // Set initial pin states
        HAL_GPIO_WritePin(PinConfig::DirPin::PORT,
                          1 << (_dirPin & 0x0F),
                          GPIO_PIN_RESET);

        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT,
                          1 << (_enablePin & 0x0F),
                          RuntimeConfig::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    void Stepper::setOperationMode(OperationMode mode)
    {
        if (static_cast<int>(_mode) == static_cast<int>(mode))
            return;

        _mode = mode;
        _config.mode = mode;
        enforceModeLimits();
    }

    void Stepper::setMicrosteps(uint32_t microsteps)
    {
        // Validate microstep values
        static const uint32_t validMicrosteps[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 1600};
        bool isValid = false;

        for (uint32_t valid : validMicrosteps)
        {
            if (microsteps == valid)
            {
                isValid = true;
                break;
            }
        }

        if (!isValid)
            return;

        _config.microsteps = microsteps;
        RuntimeConfig::current_microsteps = microsteps;
        _stepsPerFullStep = microsteps;
    }

    void Stepper::setMaxSpeed(uint32_t speed)
    {
        if (speed >= MotorDefaults::MIN_FREQ && speed <= MotorDefaults::MAX_FREQ)
        {
            _config.maxSpeed = speed;
            RuntimeConfig::current_max_speed = speed;
            enforceModeLimits();
        }
    }

    void Stepper::enforceModeLimits()
    {
        uint32_t modeLimit = calculateMaxSpeedForMode();

        if (_config.maxSpeed > modeLimit)
        {
            _config.maxSpeed = modeLimit;
            RuntimeConfig::current_max_speed = modeLimit;
        }
    }

    uint32_t Stepper::calculateMaxSpeedForMode() const
    {
        uint32_t modeLimit;

        switch (_mode)
        {
        case OperationMode::THREADING:
            modeLimit = OperationLimits::THREADING_MAX;
            break;
        case OperationMode::TURNING:
            modeLimit = OperationLimits::TURNING_MAX;
            break;
        case OperationMode::RAPIDS:
            modeLimit = OperationLimits::RAPIDS_MAX;
            break;
        default:
            modeLimit = _config.maxSpeed;
        }

        return (modeLimit < _config.maxSpeed) ? modeLimit : _config.maxSpeed;
    }

    uint32_t Stepper::validateSpeed(uint32_t speed) const
    {
        if (speed > RuntimeConfig::current_max_speed)
            return RuntimeConfig::current_max_speed;
        if (speed < MotorDefaults::MIN_FREQ)
            return MotorDefaults::MIN_FREQ;
        return speed;
    }

    void Stepper::setDirection(bool dir)
    {
        bool finalDir = RuntimeConfig::invert_direction ? !dir : dir;

        HAL_GPIO_WritePin(PinConfig::DirPin::PORT,
                          1 << (_dirPin & 0x0F),
                          finalDir ? GPIO_PIN_SET : GPIO_PIN_RESET);

        _directionChanged = true;

        // Wait for direction setup time
        if (_directionChanged)
        {
            delayMicroseconds(TimingConfig::DIR_SETUP);
        }
    }

    StepperStatus Stepper::getStatus() const
    {
        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        status.currentSpeed = _currentSpeed;
        status.stepsRemaining = abs(_targetPosition - _currentPosition);
        return status;
    }

    Stepper::~Stepper()
    {
        disable();
    }

} // namespace STM32Step
