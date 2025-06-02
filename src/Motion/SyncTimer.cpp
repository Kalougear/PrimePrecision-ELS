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
                         // _accumulatedFractionalSteps(0.0f), // Removed
                         _currentAccumulatedEncoderPos(0LL), // Added initialization for new member
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
    // From SystemClock.cpp: PCLK1 = HCLK / 2; HCLK = SYSCLK / 2. SYSCLK = 400MHz.
    // So, PCLK1 = 100MHz. APB1CLKDivider = RCC_APB1_DIV2, so TIM6CLK = 2 * PCLK1 = 200MHz.
    uint32_t pclk1Freq = HAL_RCC_GetPCLK1Freq();
    uint32_t tim6Clock;
    if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) == RCC_APB1_DIV1) // Check APB1 Prescaler
    {
        tim6Clock = pclk1Freq;
    }
    else
    {
        tim6Clock = pclk1Freq * 2;
    }

    SerialDebug.print("SyncTimer::calcParams - TIM6CLK: ");
    SerialDebug.print(tim6Clock / 1000000);
    SerialDebug.print("MHz, Target Freq: ");
    SerialDebug.print(freq);
    SerialDebug.println("Hz");

    uint32_t targetTicks = tim6Clock / freq;

    prescaler = 1; // HardwareTimer lib expects actual factor, so 1 means PSC=0.
                   // We will set PSC register directly (value - 1) if needed.
    period = targetTicks;

    // Adjust prescaler if period exceeds 16-bit timer limit (0xFFFF)
    // HardwareTimer's setPrescaleFactor takes the factor (1, 2, ...), register is factor-1
    // HardwareTimer's setOverflow takes the ARR value directly.
    uint32_t actual_prescaler_reg_val = 0; // This is what goes into PSC register
    period = targetTicks - 1;              // ARR is targetTicks - 1 if prescaler is 0 (factor 1)

    if (period > 0xFFFF)
    {
        actual_prescaler_reg_val = (targetTicks / 0xFFFF); // Calculate required PSC value
        if (actual_prescaler_reg_val > 0xFFFF)
            actual_prescaler_reg_val = 0xFFFF; // Cap PSC
        period = (targetTicks / (actual_prescaler_reg_val + 1)) - 1;
        if (period > 0xFFFF)
            period = 0xFFFF; // Cap ARR
    }
    prescaler = actual_prescaler_reg_val + 1; // Store the factor for setPrescaleFactor

    SerialDebug.print("SyncTimer::calcParams - Calculated PSC_REG_VAL: ");
    SerialDebug.print(actual_prescaler_reg_val);
    SerialDebug.print(", PrescalerFactor: ");
    SerialDebug.print(prescaler);
    SerialDebug.print(", Period (ARR): ");
    SerialDebug.println(period);
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

    if (enable && !_enabled) // Only reset accumulator when transitioning from disabled to enabled
    {
        if (_encoder)
        {
            _isr_lastEncoderCount = _encoder->getCount();
            _currentAccumulatedEncoderPos = _isr_lastEncoderCount; // Initialize accumulated position
        }
        else
        {
            _isr_lastEncoderCount = 0;
            _currentAccumulatedEncoderPos = 0;
            _error = true;
            _enabled = false; // Ensure it's marked disabled if error
            return;
        }
        // _accumulatedFractionalSteps = 0.0f; // Removed
        _timer->resume();
    }
    else if (enable && _enabled)
    {
        // Already enabled, just ensure timer is running (it should be)
        _timer->resume();
    }
    else if (!enable)
    {
        _timer->pause(); // Pause the hardware timer
    }
    _enabled = enable; // Update the enabled state last
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
        // _currentAccumulatedEncoderPos and _isr_lastEncoderCount will be reset by enable(true) when motion starts.
        // SerialDebug.println("SyncTimer::setConfig - Config changed (timer was not enabled), accumulator will be reset on enable.");
        // _accumulatedFractionalSteps = 0.0f; // Removed
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

    static uint32_t isr_call_count_debug = 0; // Renamed to avoid conflict if other ISRs use similar name
    // Use _config.update_freq if available and non-zero, otherwise default to a high number to avoid spam
    uint32_t print_interval_debug = (_config.update_freq > 0) ? _config.update_freq : 10000;
    if (++isr_call_count_debug % print_interval_debug == 0) // Approx once per second
    {
        SerialDebug.println("SyncTimer ISR firing");
    }
    // The above block is for debugging, ensure it's commented for release.

    if (!_enabled || !_encoder || !_stepper)
    {
        return;
    }

    int32_t raw_current_encoder_count = _encoder->getCount();
    int32_t raw_delta = raw_current_encoder_count - _isr_lastEncoderCount;
    // raw_delta handles 32-bit wrap around correctly due to 2's complement arithmetic

    _currentAccumulatedEncoderPos += raw_delta;
    _isr_lastEncoderCount = raw_current_encoder_count;

    double targetStepperPosDouble = static_cast<double>(_currentAccumulatedEncoderPos) * _config.steps_per_encoder_tick;
    int32_t targetStepperPosInt = static_cast<int32_t>(std::round(targetStepperPosDouble));

    int32_t currentStepperPos = _stepper->getCurrentPosition(); // Assumes Stepper tracks its absolute position
    int32_t errorSteps = targetStepperPosInt - currentStepperPos;

    int32_t stepsToCommandThisCycle = 0;
    if (errorSteps > 0)
    {
        stepsToCommandThisCycle = 1;
    }
    else if (errorSteps < 0)
    {
        stepsToCommandThisCycle = -1;
    }
    // For very high ISR rates, commanding more than 1 step might be needed if error accumulates.
    // Example: if (std::abs(errorSteps) > SOME_THRESHOLD) stepsToCommandThisCycle = errorSteps > 0 ? SOME_THRESHOLD : -SOME_THRESHOLD;
    // For now, stick to single step correction.

    static int debug_print_counter_sync_isr = 0;
    if (++debug_print_counter_sync_isr % (_config.update_freq / 10) == 0) // Print approx 10 times per second
    {
        if (_config.update_freq < 100)
            debug_print_counter_sync_isr = 0; // Prevent div by zero if freq is too low
        SerialDebug.print("SyncISR: accEnc=");
        SerialDebug.print(static_cast<long>(_currentAccumulatedEncoderPos)); // Cast to long for Serial.print if int64_t not supported directly
        SerialDebug.print(", targetStep=");
        SerialDebug.print(targetStepperPosInt);
        SerialDebug.print(", currentStep=");
        SerialDebug.print(currentStepperPos);
        SerialDebug.print(", error=");
        SerialDebug.print(errorSteps);
        SerialDebug.print(", cmd=");
        SerialDebug.println(stepsToCommandThisCycle);
    }

    if (stepsToCommandThisCycle != 0)
    {
        // Call with syncTimerPeriodUs = 0 to use fixed-rate OPM burst in Stepper::executePwmSteps
        _stepper->setRelativePosition(stepsToCommandThisCycle, 0);
    }

    _lastUpdateTime = HAL_GetTick();
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
