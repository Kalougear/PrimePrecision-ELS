#include "stepper.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"
#include "stm32h7xx_hal.h"
#include <cmath> // For fabsf

namespace STM32Step
{
    Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
        : _stepPin(stepPin),
          _dirPin(dirPin),
          _enablePin(enablePin),
          _enabled(false),
          _running(false),
          _currentDirection(false),
          _currentPosition(0),
          _targetPosition(0),
          _desiredPosition(0),
          _steps_pending_for_isr(0),
          _operationMode(OperationMode::IDLE),
          _targetSpeedHz(0.0f),
          _currentSpeedHz(0.0f),
          _accelerationStepsPerS2(1000.0f),
          _lastHardwarePulseCount(0)
    {
        initPins();
    }

    Stepper::~Stepper()
    {
        disable();
    }

    void Stepper::setSpeedHz(float frequency_hz)
    {
        // Sync position before speed change potentially alters timing
        if (_running)
        {
            updatePositionFromHardware();
        }

        _targetSpeedHz = (frequency_hz > 0.0f) ? frequency_hz : 0.0f;
        if (_running)
        {
            TimerControl::setFrequency(static_cast<uint32_t>(_targetSpeedHz));
        }
    }

    void Stepper::setAcceleration(float accel_steps_per_s2)
    {
        _accelerationStepsPerS2 = (accel_steps_per_s2 > 0.0f) ? accel_steps_per_s2 : 1000.0f;
    }

    void Stepper::runContinuous(bool direction)
    {
        if (!_enabled)
            return;

        // Update position before changing direction or starting new move
        updatePositionFromHardware();

        _currentDirection = direction;
        bool zAxisInvertDir = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
        if ((direction && !zAxisInvertDir) || (!direction && zAxisInvertDir))
        {
            GPIO_SET_DIRECTION();
        }
        else
        {
            GPIO_CLEAR_DIRECTION();
        }

        _running = true;
        TimerControl::setFrequency(static_cast<uint32_t>(_targetSpeedHz));
        TimerControl::setPulseCount(0); // 0 for continuous
        TimerControl::start(this);
    }

    void Stepper::moveExact(int32_t steps, uint32_t frequency_hz)
    {
        if (!_enabled || steps == 0)
            return;

        _steps_pending_for_isr = steps; // Store the exact number of steps for the ISR

        bool direction = steps > 0;
        _currentDirection = direction;
        bool zAxisInvertDir = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
        if ((direction && !zAxisInvertDir) || (!direction && zAxisInvertDir))
        {
            GPIO_SET_DIRECTION();
        }
        else
        {
            GPIO_CLEAR_DIRECTION();
        }

        _targetPosition += steps;
        _running = true;
        TimerControl::setFrequency(frequency_hz);
        TimerControl::setPulseCount(std::abs(steps));
        TimerControl::start(this);
    }

    void Stepper::setTargetPosition(int32_t position)
    {
        int32_t stepsToMove = position - _currentPosition;
        if (stepsToMove != 0)
        {
            moveExact(stepsToMove, static_cast<uint32_t>(_targetSpeedHz));
        }
    }

    void Stepper::setRelativePosition(int32_t delta)
    {
        _desiredPosition += delta;
    }

    void Stepper::stop()
    {
        updatePositionFromHardware();
        _running = false;
        TimerControl::stop();
        updatePositionFromHardware(); // Capture any final pulses
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
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, PinConfig::EnablePin::PIN,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false;
    }

    void Stepper::disable()
    {
        if (!_enabled)
            return;
        stop();
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, PinConfig::EnablePin::PIN,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
        _enabled = false;
    }

    void Stepper::setMicrosteps(uint32_t microsteps)
    {
        SystemConfig::RuntimeConfig::Stepper::microsteps = microsteps;
    }

    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE(); // Assumes all pins are on GPIOE
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // Step pin is configured in TimerControl as Alternate Function

        // Configure Direction Pin
        GPIO_InitStruct.Pin = PinConfig::DirPin::PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(PinConfig::DirPin::PORT, &GPIO_InitStruct);

        // Configure Enable Pin
        GPIO_InitStruct.Pin = PinConfig::EnablePin::PIN;
        HAL_GPIO_Init(PinConfig::EnablePin::PORT, &GPIO_InitStruct);

        // Set initial states
        HAL_GPIO_WritePin(PinConfig::DirPin::PORT, PinConfig::DirPin::PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PinConfig::EnablePin::PORT, PinConfig::EnablePin::PIN,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    void Stepper::GPIO_SET_DIRECTION() { HAL_GPIO_WritePin(PinConfig::DirPin::PORT, PinConfig::DirPin::PIN, GPIO_PIN_SET); }
    void Stepper::GPIO_CLEAR_DIRECTION() { HAL_GPIO_WritePin(PinConfig::DirPin::PORT, PinConfig::DirPin::PIN, GPIO_PIN_RESET); }

    StepperStatus Stepper::getStatus() const
    {
        // Update position on read to give live feedback
        // Use const_cast to call non-const method from const method
        const_cast<Stepper *>(this)->updatePositionFromHardware();

        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        return status;
    }

    void Stepper::updatePositionFromHardware()
    {
        uint32_t currentHardwareCount = TimerControl::getPulseCount();
        uint32_t delta = currentHardwareCount - _lastHardwarePulseCount; // Unsigned subtraction handles rollover automatically

        if (delta != 0)
        {
            // Determine direction for position update
            // If we are running, use _currentDirection.
            // If stopped, we assume no movement, but if delta > 0 it means pulses were generated.
            // We'll use the last known direction.

            // NOTE: _currentDirection: false=CW (positive?), true=CCW (negative?)
            // Need to verify direction mapping.
            // In moveExact: bool direction = steps > 0; _currentDirection = direction;
            // So true = Positive, False = Negative?
            // Let's check moveExact:
            // bool direction = steps > 0;
            // _currentDirection = direction;
            // if ((direction && !zAxisInvertDir) || (!direction && zAxisInvertDir)) ... GPIO_SET_DIRECTION

            // If steps > 0, direction is true. Position should increment.
            // If steps < 0, direction is false. Position should decrement.

            if (_currentDirection)
            {
                _currentPosition += delta;
            }
            else
            {
                _currentPosition -= delta;
            }

            _lastHardwarePulseCount = currentHardwareCount;
        }
    }

    void Stepper::incrementCurrentPosition(int32_t increment)
    {
        _currentPosition += increment;
        _targetPosition += increment;
    }

    void Stepper::adjustPosition(int32_t adjustment)
    {
        _currentPosition += adjustment;
        _targetPosition += adjustment;
    }

    void Stepper::onMoveComplete()
    {
        // Sync one last time
        updatePositionFromHardware();

        // For exact moves, we rely on the hardware counter now, so _steps_pending_for_isr
        // might be redundant for position tracking, but good for verification.
        // However, if we mix methods, we might double count.
        // Since we are now counting ALL pulses via TIM5, we should NOT add _steps_pending_for_isr manually.

        _steps_pending_for_isr = 0;
        // _running is set to false in TimerControl::stop() which is called by the ISR
    }

    void Stepper::setDesiredPosition(int32_t position)
    {
        _desiredPosition = position;
    }

    void Stepper::ISR()
    {
        // if we are not currently running a move, see if we need to start one
        if (!_running)
        {
            int32_t steps = _desiredPosition - _targetPosition;
            if (steps != 0)
            {
                moveExact(steps, static_cast<uint32_t>(_targetSpeedHz));
            }
        }
    }

} // namespace STM32Step
