#include "Motion/SyncTimer.h"
#include "Config/serial_debug.h"
#include "Hardware/EncoderTimer.h"
#include <cmath>

#define ENCODER_MAX_COUNT 0xFFFFFFFF

SyncTimer *SyncTimer::instance = nullptr;

SyncTimer::SyncTimer() : _timer(nullptr),
                         _enabled(false),
                         _error(false),
                         _lastUpdateTime(0),
                         _initialized(false),
                         _timerFrequency(1000),
                         _encoder(nullptr),
                         _stepper(nullptr),
                         _desiredSteps_scaled_accumulated(0),
                         _isr_lastEncoderCount(0),
                         _debug_interrupt_count(0),
                         _debug_last_steps(0),
                         _debug_isr_spindle_pos(0),
                         _debug_isr_previous_pos(0)
{
}

SyncTimer::~SyncTimer()
{
    end();
    if (instance == this)
    {
        instance = nullptr;
    }
    if (_timer)
    {
        delete _timer;
        _timer = nullptr;
    }
}

bool SyncTimer::begin(EncoderTimer *encoder, STM32Step::Stepper *stepper)
{
    if (_initialized)
        return true;

    instance = this;
    this->_encoder = encoder;
    this->_stepper = stepper;

    if (!this->_encoder || !this->_stepper)
    {
        _error = true;
        return false;
    }

    if (!initTimer())
    {
        _error = true;
        return false;
    }

    _initialized = true;
    _error = false;
    return true;
}

bool SyncTimer::initTimer()
{
    _timer = new HardwareTimer(TIM6);
    if (!_timer)
    {
        _error = true;
        return false;
    }

    uint32_t prescaler, period;
    calculateTimerParameters(_timerFrequency, prescaler, period);
    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);
    _timer->attachInterrupt([this]()
                            { this->handleInterrupt(); });
    return true;
}

void SyncTimer::calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period)
{
    if (freq == 0)
    {
        prescaler = 1;
        period = 0xFFFF;
        return;
    }
    uint32_t timerClock = SystemCoreClock; // This needs to be the actual TIM6 clock
    uint32_t targetTicks = timerClock / freq;
    prescaler = 1;
    period = targetTicks;
    while (period > 0xFFFF)
    {
        prescaler++;
        if (prescaler == 0)
        {
            period = 0xFFFF;
            break;
        }
        period = targetTicks / prescaler;
    }
}

void SyncTimer::end()
{
    if (!_initialized)
        return;
    if (_timer)
    {
        _timer->pause();
        _timer->detachInterrupt();
    }
    _initialized = false;
    _enabled = false;
}

void SyncTimer::enable(bool enable)
{
    if (!_initialized || !_timer)
    {
        _error = true;
        return;
    }

    _enabled = enable;
    if (enable)
    {
        if (_encoder)
        {
            _isr_lastEncoderCount = _encoder->getCount();
        }
        else
        {
            _error = true;
            return;
        }
        _previousSpindlePosition = _encoder->getRawCounter();
        _desiredSteps_scaled_accumulated = 0;
        _timer->resume();
    }
    else
    {
        _timer->pause();
    }
}

void SyncTimer::setConfig(const SyncConfig &new_config)
{
    bool was_enabled = _enabled;
    if (was_enabled)
    {
        this->enable(false);
    }

    _config = new_config;
    setSyncFrequency(_config.update_freq);

    if (was_enabled)
    {
        this->enable(true);
    }
}

void SyncTimer::setSyncFrequency(uint32_t freq)
{
    if (!_initialized || !_timer || freq == 0)
    {
        if (freq == 0 && _initialized && _timer)
            _timer->pause();
        return;
    }

    uint32_t prescaler, period;
    calculateTimerParameters(freq, prescaler, period);
    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);
    _timerFrequency = freq;
}

void SyncTimer::handleInterrupt()
{
    if (!_enabled || !_encoder || !_stepper)
    {
        return;
    }

    _debug_interrupt_count++;

    // read the encoder
    uint32_t spindlePosition = _encoder->getRawCounter();

    _debug_isr_spindle_pos = spindlePosition;
    _debug_isr_previous_pos = _previousSpindlePosition;

    // Calculate encoder delta with rollover handling
    int64_t delta_encoder = static_cast<int64_t>(spindlePosition) - static_cast<int64_t>(_previousSpindlePosition);

    // Handle overflow/underflow (rollover)
    // Cast to int64_t first to ensure signed arithmetic works correctly
    const int64_t MAX_ENC_VAL = static_cast<int64_t>(ENCODER_MAX_COUNT);
    const int64_t HALF_MAX_ENC_VAL = MAX_ENC_VAL / 2;

    // If change is very large negative, it wrapped forward (e.g. MAX -> 0) -> Add MAX
    if (delta_encoder < -HALF_MAX_ENC_VAL)
    {
        delta_encoder += (MAX_ENC_VAL + 1);
    }
    // If change is very large positive, it wrapped backward (e.g. 0 -> MAX) -> Subtract MAX
    else if (delta_encoder > HALF_MAX_ENC_VAL)
    {
        delta_encoder -= (MAX_ENC_VAL + 1);
    }

    // Apply direction
    if (_config.reverse_direction)
    {
        delta_encoder = -delta_encoder;
    }

    // Scale
    int64_t delta_scaled = delta_encoder * _config.steps_per_encoder_tick_scaled;

    // Add to accumulator
    _desiredSteps_scaled_accumulated += delta_scaled;

    // calculate the number of whole steps to move
    int32_t stepsToMove = static_cast<int32_t>(_desiredSteps_scaled_accumulated / _config.scaling_factor);
    _debug_last_steps = stepsToMove;

    if (stepsToMove != 0)
    {
        // Calculate required speed to complete these steps within one timer period
        // Speed (Hz) = Steps / Time(s) = Steps * Frequency(Hz)
        float speedHz = static_cast<float>(std::abs(stepsToMove)) * static_cast<float>(_timerFrequency);

        // Clamp minimum speed to ensure timer runs
        if (speedHz < 10.0f)
            speedHz = 10.0f;

        _stepper->setSpeedHz(speedHz);

        // remove the steps we are about to command from the accumulator
        _desiredSteps_scaled_accumulated -= static_cast<int64_t>(stepsToMove) * _config.scaling_factor;

        // command the move
        _stepper->setRelativePosition(stepsToMove);
    }

    // remember values for next time
    _previousSpindlePosition = spindlePosition;

    _lastUpdateTime = HAL_GetTick();

    // Kick the stepper if needed
    _stepper->ISR();
}

void SyncTimer::printDebugInfo()
{
    static uint32_t last_print = 0;
    if (HAL_GetTick() - last_print > 1000)
    {
        last_print = HAL_GetTick();
        SerialDebug.println("--- SyncTimer Debug ---");
        SerialDebug.print("Interrupts: ");
        SerialDebug.println(_debug_interrupt_count);
        SerialDebug.print("Encoder Raw: ");
        SerialDebug.println(_encoder ? _encoder->getRawCounter() : 0);
        SerialDebug.print("Last Steps: ");
        SerialDebug.println(_debug_last_steps);
        SerialDebug.print("ISR Spindle: ");
        SerialDebug.println(_debug_isr_spindle_pos);
        SerialDebug.print("ISR Previous: ");
        SerialDebug.println(_debug_isr_previous_pos);
        SerialDebug.print("Accumulator: ");
        SerialDebug.print((long)_desiredSteps_scaled_accumulated); // Cast for printing
        SerialDebug.println();
        SerialDebug.print("Config Scaled: ");
        SerialDebug.println(_config.steps_per_encoder_tick_scaled);
        SerialDebug.print("Reverse: ");
        SerialDebug.println(_config.reverse_direction);
        SerialDebug.println("-----------------------");
    }
}
