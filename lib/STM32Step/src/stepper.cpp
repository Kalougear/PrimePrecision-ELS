#include "stepper.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"
#include "stm32h7xx_hal.h"
#include <algorithm> // For std::abs if used
#include <cmath>     // For fabsf

// Make SerialDebug available
#ifndef SERIAL_DEBUG_INSTANCE_EXTERNED
#define SERIAL_DEBUG_INSTANCE_EXTERNED
extern HardwareSerial SerialDebug;
#endif

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
          _currentDirection(false), // Will be set by runContinuous
          _operationMode(OperationMode::IDLE),
          state(0),
          _isContinuousMode(false),
          _targetSpeedHz(0.0f),
          _currentSpeedHz(0.0f),
          _accelerationStepsPerS2(1000.0f),
          _nextStepTimeMicros(0),
          _isrAccumulatedSteps(0.0f),
          _lastStepTimeMicros(0)
    {
        initPins();
    }

    Stepper::~Stepper()
    {
        disable();
    }

    void Stepper::setSpeedHz(float frequency_hz)
    {
        _targetSpeedHz = (frequency_hz > 0.0f) ? frequency_hz : 0.0f;
    }

    void Stepper::setAcceleration(float accel_steps_per_s2)
    {
        // Acceleration is currently bypassed in ISR for jog mode testing
        _accelerationStepsPerS2 = (accel_steps_per_s2 > 0.0f) ? accel_steps_per_s2 : 1000.0f;
    }

    void Stepper::runContinuous(bool direction)
    {
        if (!_enabled)
        {
            SerialDebug.println("Stepper::runContinuous - Stepper not enabled, cannot run.");
            return;
        }
        if (!_isContinuousMode && _running) // If ELS was running
        {
            stop(); // Stop ELS first
        }

        _currentDirection = direction; // Store the logical direction

        // Set physical direction pin state ONCE here
        bool zAxisInvertDir = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
        bool physicalDirPinState;
        if (_currentDirection) // true for AWAY_FROM_CHUCK
        {
            physicalDirPinState = zAxisInvertDir ? LOW : HIGH;
        }
        else // false for TOWARDS_CHUCK
        {
            physicalDirPinState = zAxisInvertDir ? HIGH : LOW;
        }

        SerialDebug.print("Stepper::runContinuous - Setting DIR: _curDir=");
        SerialDebug.print(_currentDirection);
        SerialDebug.print(" ZInv=");
        SerialDebug.print(zAxisInvertDir);
        SerialDebug.print(" physState=");
        SerialDebug.println(physicalDirPinState);

        if (physicalDirPinState == HIGH)
        {
            GPIO_SET_DIRECTION();
        }
        else
        {
            GPIO_CLEAR_DIRECTION();
        }

        // Small delay after setting direction, before starting pulses (optional, but good practice)
        // HAL_Delay(1); // 1ms might be too long, consider a microsecond delay if needed, e.g. using a NOP loop or DWT cycle counter

        _isContinuousMode = true;
        _currentSpeedHz = 0.0f; // ISR will set this from _targetSpeedHz (accel bypassed)
        _isrAccumulatedSteps = 0.0f;
        _lastStepTimeMicros = micros();
        _nextStepTimeMicros = micros(); // Schedule first step check immediately
        state = 0;                      // Reset pulse state machine

        _running = true;
        TimerControl::start(this);
    }

    volatile uint32_t g_stepper_isr_entry_count = 0;
    // static uint32_t s_dir_debug_print_counter = 0; // No longer needed in ISR

    void Stepper::ISR(void)
    {
        g_stepper_isr_entry_count++;

        if (!_enabled || !_running)
        {
            state = 0;
            GPIO_CLEAR_STEP();
            if (_isContinuousMode)
            {
                _isContinuousMode = false;
                _currentSpeedHz = 0.0f;
            }
            return;
        }

        if (_isContinuousMode)
        {
            uint32_t currentTimeMicros = micros();

            // NO ACCELERATION - Direct speed control
            if (_targetSpeedHz > 0.01f)
            {
                _currentSpeedHz = _targetSpeedHz;
            }
            else
            {
                _currentSpeedHz = 0.0f;
            }

            if (_currentSpeedHz > 0.01f)
            {
                float timePerStepMicros = (1.0f / _currentSpeedHz) * 1e6f;

                // DIR PIN IS NO LONGER SET HERE for continuous mode. It's set once in runContinuous().

                // Pulse generation state machine
                if (state == 0 && (int32_t)(currentTimeMicros - _nextStepTimeMicros) >= 0)
                {
                    GPIO_SET_STEP();
                    _nextStepTimeMicros = currentTimeMicros + static_cast<uint32_t>(timePerStepMicros);
                    state = 1;
                }
                else if (state == 1)
                {
                    GPIO_CLEAR_STEP();
                    state = 0;
                }
            }
            else
            {
                GPIO_CLEAR_STEP();
                state = 0;

                if (_targetSpeedHz < 0.01f)
                {
                    _running = false;
                    _isContinuousMode = false;
                }
            }
        }
        else
        {
            // ELS Position Mode Logic (unchanged)
            int32_t positionDifference = _targetPosition - _currentPosition;
            if (positionDifference == 0)
            {
                _running = false;
                state = 0;
                GPIO_CLEAR_STEP();
                return;
            }
            bool moveForward = positionDifference > 0;
            switch (state)
            {
            case 0:
            {
                if (moveForward != _currentDirection)
                {
                    _currentDirection = moveForward;
                    bool set_dir_pin_high = moveForward ? !SystemConfig::RuntimeConfig::Z_Axis::invert_direction : SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
                    if (set_dir_pin_high)
                        GPIO_SET_DIRECTION();
                    else
                        GPIO_CLEAR_DIRECTION();
                    state = 1;
                }
                else
                {
                    GPIO_SET_STEP();
                    state = 2;
                }
            }
            break;
            case 1:
                state = 0;
                break;
            case 2:
                GPIO_CLEAR_STEP();
                if (_currentDirection)
                    _currentPosition++;
                else
                    _currentPosition--;
                state = 0;
                break;
            }
        }
    }

    void Stepper::setTargetPosition(int32_t position)
    {
        if (_isContinuousMode)
        {
            stop();
        }
        _isContinuousMode = false;
        if (position != _currentPosition)
        {
            _targetPosition = position;
            if (!_running)
            {
                _running = true;
                TimerControl::start(this);
            }
        }
        else if (_running && position == _currentPosition)
        {
            stop();
        }
    }

    void Stepper::setRelativePosition(int32_t delta)
    {
        if (_isContinuousMode)
        {
            stop();
        }
        _isContinuousMode = false;
        if (delta != 0)
        {
            setTargetPosition(_targetPosition + delta);
        }
        else if (!_running && delta == 0)
        {
            // Do nothing
        }
        else if (_running && delta == 0 && _targetPosition == _currentPosition)
        {
            stop();
        }
    }

    void Stepper::stop()
    {
        if (_isContinuousMode)
        {
            _targetSpeedHz = 0.0f;
        }
        // _running should be set to false by ISR when _currentSpeedHz becomes 0 if _targetSpeedHz is 0
        // However, to ensure a command to stop is immediate:
        _running = false;
        _isContinuousMode = false;
        state = 0;
        GPIO_CLEAR_STEP();
        TimerControl::stop(); // This will stop ISR calls
    }

    void Stepper::emergencyStop()
    {
        _isContinuousMode = false;
        _currentSpeedHz = 0.0f;
        _targetSpeedHz = 0.0f;
        stop();
        disable();
        TimerControl::emergencyStopRequest();
    }

    void Stepper::enable()
    {
        if (_enabled)
            return;
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false;
        state = 0;
    }

    void Stepper::disable()
    {
        if (!_enabled)
            return;
        stop();
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
        _enabled = false;
    }

    void Stepper::setMicrosteps(uint32_t microsteps)
    {
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
        {
            return;
        }
        // This updates the SystemConfig, which is used by MotionControl to calculate steps_per_mm etc.
        // The Stepper class itself, for its ISR, primarily cares about pulse timing based on Hz.
        SystemConfig::RuntimeConfig::Stepper::microsteps = microsteps;
    }

    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = 1 << (_stepPin & 0x0F);
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = 1 << (_dirPin & 0x0F);
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = 1 << (_enablePin & 0x0F);
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
        GPIO_CLEAR_STEP();
        HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, 1 << (_enablePin & 0x0F),
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    void Stepper::GPIO_SET_STEP() { HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_SET); }
    void Stepper::GPIO_CLEAR_STEP() { HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_RESET); }
    void Stepper::GPIO_SET_DIRECTION() { HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_SET); }
    void Stepper::GPIO_CLEAR_DIRECTION() { HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET); }

    StepperStatus Stepper::getStatus() const
    {
        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        status.stepsRemaining = _targetPosition - _currentPosition;
        return status;
    }

    void Stepper::incrementCurrentPosition(int32_t increment)
    {
        _currentPosition += increment;
        _targetPosition += increment;
    }

} // namespace STM32Step
