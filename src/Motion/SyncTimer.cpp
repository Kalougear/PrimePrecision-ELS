#include "Motion/SyncTimer.h"
#include "Config/serial_debug.h"   // For error printing if necessary
#include "Hardware/EncoderTimer.h" // For EncoderTimer type
#include <STM32Step.h>             // For Stepper type
#include <cmath>                   // For std::round

// Initialize static instance pointer for ISR callback
SyncTimer *SyncTimer::instance = nullptr;

/**
 * @brief Constructor for SyncTimer.
 * Initializes member variables to their default states.
 */
SyncTimer::SyncTimer() : _timer(nullptr),
                         _enabled(false),
                         _error(false),
                         _lastUpdateTime(0),
                         _initialized(false),
                         _timerFrequency(1000), // Default 1kHz, will be overridden by setConfig
                         _encoder(nullptr),
                         _stepper(nullptr),
                         _accumulatedFractionalSteps(0.0f),
                         _isr_lastEncoderCount(0)
{
}

/**
 * @brief Destructor for SyncTimer.
 * Calls end() to release timer resources and cleans up the timer object.
 */
SyncTimer::~SyncTimer()
{
    end(); // Ensure timer is stopped and detached
    if (instance == this)
    {
        instance = nullptr; // Clear singleton if this was it
    }
    if (_timer)
    {
        delete _timer; // Free the HardwareTimer object
        _timer = nullptr;
    }
}

/**
 * @brief Initializes the SyncTimer with dependent EncoderTimer and Stepper objects.
 * Sets up the hardware timer (TIM6) for periodic interrupts.
 * @param encoder Pointer to the EncoderTimer instance.
 * @param stepper Pointer to the STM32Step::Stepper instance.
 * @return True if initialization is successful, false otherwise.
 */
bool SyncTimer::begin(EncoderTimer *encoder, STM32Step::Stepper *stepper)
{
    if (_initialized)
    {
        return true; // Already initialized
    }

    instance = this; // Set singleton for ISR callback

    this->_encoder = encoder;
    this->_stepper = stepper;

    if (!this->_encoder || !this->_stepper)
    {
        // SerialDebug.println("ERROR: SyncTimer requires valid encoder and stepper instances."); // Optional error print
        _error = true;
        return false;
    }

    if (!initTimer())
    {
        // SerialDebug.println("ERROR: SyncTimer hardware timer initialization failed."); // Optional error print
        _error = true; // initTimer should set _error if it fails internally
        return false;
    }

    _initialized = true;
    _error = false; // Clear error state on successful initialization
    // SerialDebug.println("SyncTimer initialized successfully."); // Optional success print
    return true;
}

/**
 * @brief Initializes the hardware timer (TIM6) used for synchronization.
 * Configures the timer with an initial frequency and attaches the interrupt handler.
 * @return True if timer initialization is successful, false otherwise.
 */
bool SyncTimer::initTimer()
{
    _timer = new HardwareTimer(TIM6); // Use TIM6 for SyncTimer
    if (!_timer)
    {
        // SerialDebug.println("ERROR: SyncTimer failed to allocate HardwareTimer for TIM6."); // Optional error print
        _error = true;
        return false;
    }

    // Calculate initial timer parameters based on default _timerFrequency
    uint32_t prescaler, period;
    calculateTimerParameters(_timerFrequency, prescaler, period);

    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);

    // Attach the ISR: lambda calls member function handleInterrupt
    _timer->attachInterrupt([this]()
                            { this->handleInterrupt(); });

    // Timer is configured but not started yet (paused by default or explicit resume in enable())
    return true;
}

/**
 * @brief Calculates the prescaler and auto-reload period for the timer to achieve a target frequency.
 * @param freq The desired frequency in Hz.
 * @param[out] prescaler The calculated prescaler value (to be set in PSC register - 1).
 * @param[out] period The calculated auto-reload period value (ARR register).
 * Assumes SystemCoreClock is the input clock to the timer (may need adjustment if APB prescalers differ for TIM6).
 */
void SyncTimer::calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period)
{
    if (freq == 0)
    { // Avoid division by zero
        prescaler = 1;
        period = 0xFFFF; // Max period, effectively very slow
        return;
    }
    // For TIM6, the clock source is typically APB1 timer clock.
    // If APB1 prescaler is > 1, timer clock is 2 * PCLK1. Otherwise, PCLK1.
    // SystemCoreClock might be HCLK. Need to get specific timer clock.
    // Assuming SystemCoreClock is a placeholder for the actual TIM6 input clock frequency.
    // For STM32H7, TIM6 clock is from rcc_pclk1 if APB1 prescaler is 1, else 2*PCLK1.
    // Let's assume PCLK1 is SystemCoreClock / APB1_Prescaler.
    // For simplicity here, using SystemCoreClock directly; refine if specific clock source is known and different.
    uint32_t timerClock = SystemCoreClock; // TODO: Confirm actual TIM6 clock source and frequency
    uint32_t targetTicks = timerClock / freq;

    prescaler = 1; // Actual register value will be prescaler - 1
    period = targetTicks;

    // Adjust prescaler if period exceeds 16-bit timer limit (0xFFFF)
    while (period > 0xFFFF)
    {
        prescaler++;
        if (prescaler == 0)
        {                    // Overflowed prescaler, should not happen with practical frequencies
            period = 0xFFFF; // Max out period
            break;
        }
        period = targetTicks / prescaler;
    }
    // HardwareTimer library might handle prescaler as (value) or (value-1).
    // Assuming it expects the factor directly (e.g., 1 for no division beyond clock source).
}

/**
 * @brief Stops the synchronization timer and detaches its interrupt.
 * Marks the SyncTimer as uninitialized.
 */
void SyncTimer::end()
{
    if (!_initialized)
    {
        return;
    }

    if (_timer)
    {
        _timer->pause();
        _timer->detachInterrupt();
        // delete _timer; // Moved to destructor to prevent issues if end() is called multiple times
        // _timer = nullptr;
    }
    _initialized = false;
    _enabled = false; // Ensure it's marked as disabled
}

/**
 * @brief Enables or disables the SyncTimer's operation.
 * When enabled, it resets the internal state for ISR processing (_isr_lastEncoderCount,
 * _accumulatedFractionalSteps) and resumes the hardware timer.
 * When disabled, it pauses the hardware timer.
 * @param enable True to enable synchronization, false to disable.
 */
void SyncTimer::enable(bool enable)
{
    if (!_initialized || !_timer)
    {
        _error = true; // Cannot enable/disable if not initialized
        return;
    }

    _enabled = enable;
    if (enable)
    {
        if (_encoder)
        {
            _isr_lastEncoderCount = _encoder->getCount(); // Initialize last count from current encoder position
        }
        else
        {
            _isr_lastEncoderCount = 0; // Should not happen if begin() checked properly
            _error = true;
            return; // Critical dependency missing
        }
        _accumulatedFractionalSteps = 0.0f; // Reset accumulator for a clean start
        _timer->resume();                   // Start or resume the hardware timer
    }
    else
    {
        _timer->pause(); // Pause the hardware timer
    }
}

/**
 * @brief Sets the synchronization configuration.
 * Updates the internal configuration and adjusts the timer frequency accordingly.
 * @param config A const reference to the SyncConfig struct containing new parameters.
 */
void SyncTimer::setConfig(const SyncConfig &new_config)
{
    if (!_initialized)
    {
        // If not initialized, just store the config. It will be fully applied in begin() or later.
        // However, setSyncFrequency might still be problematic if _timer is null.
        // For safety, perhaps only allow setConfig after begin().
        // For now, assume _config can be updated, but frequency setting might fail if _timer is null.
        _config = new_config;
        if (_timer)
        { // Only try to set frequency if timer object exists
            setSyncFrequency(_config.update_freq);
        }
        SerialDebug.println("SyncTimer::setConfig - Called before fully initialized. Stored config.");
        return;
    }

    double old_steps_per_tick_log = _config.steps_per_encoder_tick;
    bool config_had_significant_change = std::abs(old_steps_per_tick_log - new_config.steps_per_encoder_tick) > 1e-9;

    SerialDebug.print("SyncTimer::setConfig - Old _config.steps_per_tick: ");
    SerialDebug.println(old_steps_per_tick_log, 6);
    SerialDebug.print("SyncTimer::setConfig - New new_config.steps_per_tick: ");
    SerialDebug.println(new_config.steps_per_encoder_tick, 6);
    SerialDebug.print("SyncTimer::setConfig - Config had significant change: ");
    SerialDebug.println(config_had_significant_change);

    bool was_enabled = _enabled;

    if (was_enabled)
    {
        this->enable(false); // Pause timer, set _enabled = false. ISR will not run.
    }

    _config = new_config;                  // Apply the new configuration values
    setSyncFrequency(_config.update_freq); // Update hardware timer's period/prescaler. This is safe as timer is paused.

    // If the timer was originally enabled, re-enable it.
    // enable(true) will reset _accumulatedFractionalSteps and _isr_lastEncoderCount.
    if (was_enabled)
    {
        SerialDebug.println("SyncTimer::setConfig - Re-enabling timer after config change.");
        this->enable(true);
    }
    else if (config_had_significant_change && _initialized)
    {
        // If config changed significantly but timer was not enabled,
        // still good to reset accumulator. _isr_lastEncoderCount will be set by enable(true) later.
        SerialDebug.println("SyncTimer::setConfig - Config changed (timer was not enabled), pre-resetting accumulator.");
        _accumulatedFractionalSteps = 0.0f;
    }
}

/**
 * @brief Sets or updates the frequency of the synchronization timer ISR.
 * Recalculates and applies new prescaler and period values to the hardware timer.
 * @param freq The desired ISR frequency in Hz.
 */
void SyncTimer::setSyncFrequency(uint32_t freq)
{
    if (!_initialized || !_timer || freq == 0)
    {
        // Cannot set frequency if not initialized, timer is null, or freq is zero
        if (freq == 0 && _initialized && _timer)
            _timer->pause(); // Pause if freq is zero
        return;
    }

    uint32_t prescaler, period;
    calculateTimerParameters(freq, prescaler, period);

    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);

    _timerFrequency = freq; // Store the new frequency

    // If timer was already enabled, it continues with new frequency.
    // If it was paused, it remains paused but configured.
}

/**
 * @brief Hardware timer interrupt handler for TIM6.
 * This is the core synchronization logic, executed at `_timerFrequency`.
 * It reads the encoder delta, calculates required stepper steps (with fractional accumulation),
 * and commands the stepper motor. This method must be efficient as it runs in ISR context.
 */
void SyncTimer::handleInterrupt()
{
    // Optional: Infrequent debug print for ISR firing confirmation
    /*
    static uint32_t isr_call_count = 0;
    if (++isr_call_count % _timerFrequency == 0) // Approx once per second
    {
        SerialDebug.println("SyncTimer ISR firing");
    }
    */

    if (!_enabled || !_encoder || !_stepper)
    {
        // Critical dependencies missing or not enabled, cannot proceed.
        return;
    }

    // Read current encoder count. EncoderTimer::getCount() is assumed ISR-safe
    // as it directly reads the hardware counter register.
    int32_t currentCount = _encoder->getCount();
    int32_t encoderDelta = currentCount - _isr_lastEncoderCount;

    if (encoderDelta != 0)
    {
        // Use the pre-calculated steps_per_encoder_tick factor (now double precision)
        // This factor now carries the sign based on thread_pitch.
        double floatStepsToCommand = static_cast<double>(encoderDelta) * _config.steps_per_encoder_tick;

        // The _config.reverse_direction logic is removed from here,
        // as MotionControl now sets it to false, and Stepper::ISR handles physical inversion.

        // Accumulate fractional steps (now double precision)
        _accumulatedFractionalSteps += floatStepsToCommand;

        // Determine whole integer steps to command
        // Use std::round for proper rounding to nearest integer
        int32_t integerStepsToCommand = static_cast<int32_t>(std::round(_accumulatedFractionalSteps));

        if (integerStepsToCommand != 0)
        {
            // Command the stepper. Stepper::setRelativePosition and its chain must be ISR-safe.
            // This involves HAL calls which are generally ISR-safe if brief.
            _stepper->setRelativePosition(integerStepsToCommand);

            // Subtract the commanded whole steps from the accumulator (now double precision)
            _accumulatedFractionalSteps -= static_cast<double>(integerStepsToCommand);
        }
        _isr_lastEncoderCount = currentCount; // Update last count for next ISR cycle
    }

    _lastUpdateTime = HAL_GetTick(); // Record time of this ISR execution
}

/**
 * @brief Calculates the synchronization ratio based on configured pitches.
 * This ratio determines the electronic gearing between encoder counts and stepper steps.
 * @return The calculated sync ratio. Returns 0 if `leadscrew_pitch` is zero to prevent division by zero.
 */
double SyncTimer::calculateSyncRatio() const // Changed return type to double
{
    // This function is now effectively replaced by using _config.steps_per_encoder_tick directly.
    // It can be removed or modified to return _config.steps_per_encoder_tick if still called from somewhere,
    // though direct usage in handleInterrupt is cleaner.
    // For now, let's make it return the new factor if it were to be used.
    // However, it's better to remove calls to this and use the factor directly.
    // SerialDebug.println("Warning: calculateSyncRatio() called, should use _config.steps_per_encoder_tick directly.");
    return _config.steps_per_encoder_tick;
    // Original logic:
    // if (_config.leadscrew_pitch == 0.0f) // Check for zero to prevent division by zero
    //     return 0.0f;
    // return _config.thread_pitch / _config.leadscrew_pitch;
}
