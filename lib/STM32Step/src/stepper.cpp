#include "stepper.h"
#include "Config/serial_debug.h"
#include "Config/system_config.h"
#include <algorithm>

namespace STM32Step
{
    Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
        : _stepPin(stepPin),
          _dirPin(dirPin),
          _enablePin(enablePin),
          _currentPosition(0),
          _targetPosition(0),
          _enabled(false),
          _running(false),
          _currentDirection(false),
          _operationMode(OperationMode::IDLE),
          state(0)
    {
        initPins();
    }

    void Stepper::ISR(void)
    {
        if (!_enabled || !_running)
        {
            // Reset state machine when not running
            state = 0;
            GPIO_CLEAR_STEP();
            return;
        }

        // Check if we need to move
        int32_t positionDifference = _targetPosition - _currentPosition;
        if (positionDifference == 0)
        {
            _running = false;
            state = 0;
            GPIO_CLEAR_STEP();
            return;
        }

        // Determine direction before switch
        bool moveForward = positionDifference > 0;

        // Simple position-based state machine
        switch (state)
        {
        case 0: // Check position and direction
        {
            // If direction change needed
            if (moveForward != _currentDirection)
            {
                _currentDirection = moveForward;
                if (moveForward)
                    GPIO_SET_DIRECTION();
                else
                    GPIO_CLEAR_DIRECTION();
                state = 1; // Go to direction change delay
            }
            else
            {
                GPIO_SET_STEP(); // Start step pulse
                state = 2;
            }
        }
        break;

        case 1:        // Direction change delay
            state = 0; // Return to position check next cycle
            break;

        case 2:                // Step pulse high
            GPIO_CLEAR_STEP(); // End step pulse
            if (_currentDirection)
                _currentPosition++;
            else
                _currentPosition--;
            state = 0; // Return to position check
            break;
        }
    }

    void Stepper::setTargetPosition(int32_t position)
    {
        // Only start running if there's actually a position difference
        if (position != _currentPosition)
        {
            _targetPosition = position;
            if (!_running)
            {
                _running = true;
                TimerControl::start(this);
            }
        }
    }

    void Stepper::setRelativePosition(int32_t delta)
    {
        if (delta != 0)
        {
            setTargetPosition(_targetPosition + delta);
        }
    }

    void Stepper::stop()
    {
        _running = false;
        state = 0;
        GPIO_CLEAR_STEP();
        TimerControl::stop();
    }

    void Stepper::emergencyStop()
    {
        stop();
        disable();
        TimerControl::emergencyStopRequest();
    }

    void Stepper::enable()
    {
        if (_enabled)
            return;

        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, GPIO_PIN_7,
                          SystemConfig::RuntimeConfig::Stepper::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false;
        state = 0;
    }

    void Stepper::disable()
    {
        if (!_enabled)
            return;

        stop();
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, GPIO_PIN_7, GPIO_PIN_SET);
        _enabled = false;
    }

    void Stepper::setMicrosteps(uint32_t microsteps)
    {
        // Validate microstep values
        static const uint32_t validMicrosteps[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
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

        SystemConfig::RuntimeConfig::Stepper::microsteps = microsteps;
    }

    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // Configure step pin as normal GPIO output
        GPIO_InitStruct.Pin = 1 << (_stepPin & 0x0F);
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(PinConfig::StepPin::PORT, &GPIO_InitStruct);

        // Configure direction pin
        GPIO_InitStruct.Pin = 1 << (_dirPin & 0x0F);
        HAL_GPIO_Init(PinConfig::DirPin::PORT, &GPIO_InitStruct);

        // Configure enable pin
        GPIO_InitStruct.Pin = 1 << (_enablePin & 0x0F);
        HAL_GPIO_Init(PinConfig::EnablePin::PORT, &GPIO_InitStruct);

        // Set initial pin states
        GPIO_CLEAR_STEP();
        HAL_GPIO_WritePin(PinConfig::DirPin::PORT,
                          1 << (_dirPin & 0x0F),
                          GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT,
                          1 << (_enablePin & 0x0F),
                          SystemConfig::RuntimeConfig::Stepper::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    void Stepper::GPIO_SET_STEP()
    {
        HAL_GPIO_WritePin(PinConfig::StepPin::PORT, 1 << (_stepPin & 0x0F), GPIO_PIN_SET);
    }

    void Stepper::GPIO_CLEAR_STEP()
    {
        HAL_GPIO_WritePin(PinConfig::StepPin::PORT, 1 << (_stepPin & 0x0F), GPIO_PIN_RESET);
    }

    void Stepper::GPIO_SET_DIRECTION()
    {
        HAL_GPIO_WritePin(PinConfig::DirPin::PORT, 1 << (_dirPin & 0x0F), GPIO_PIN_SET);
    }

    void Stepper::GPIO_CLEAR_DIRECTION()
    {
        HAL_GPIO_WritePin(PinConfig::DirPin::PORT, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET);
    }

    StepperStatus Stepper::getStatus() const
    {
        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        status.stepsRemaining = abs(_targetPosition - _currentPosition);
        return status;
    }

    void Stepper::incrementCurrentPosition(int32_t increment)
    {
        _currentPosition += increment;
        _targetPosition += increment;
    }

    Stepper::~Stepper()
    {
        disable();
    }

} // namespace STM32Step
