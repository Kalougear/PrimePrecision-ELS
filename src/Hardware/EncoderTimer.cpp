#include "Hardware/EncoderTimer.h"
#include "Config/serial_debug.h" // For error printing
#include "Config/SystemConfig.h" // For SystemConfig::RuntimeConfig::Encoder values

// Initialize static instance pointer for ISR callback
EncoderTimer *EncoderTimer::instance = nullptr;

/**
 * @brief Constructor for EncoderTimer.
 * Initializes member variables to default states.
 */
EncoderTimer::EncoderTimer() : _currentCount(0), // Not strictly used for software extension currently
                               _lastUpdateTime(0),
                               _error(false),
                               _initialized(false),
                               _indexPulseOccurred(false) // Initialize new member
{
    // Ensure htim2 structure is zeroed out before use by HAL
    memset(static_cast<void *>(&htim2), 0, sizeof(htim2));
}

/**
 * @brief Destructor for EncoderTimer.
 * Calls end() to ensure resources are released.
 */
EncoderTimer::~EncoderTimer()
{
    end();
    if (instance == this) // Clear singleton instance if this was it
    {
        instance = nullptr;
    }
}

/**
 * @brief Initializes the GPIO pins and TIM2 for encoder mode.
 * Sets up the hardware timer (TIM2) as a quadrature encoder counter.
 * Enables interrupts for timer updates (overflows/underflows).
 * @return True if initialization was successful, false otherwise.
 */
bool EncoderTimer::begin()
{
    if (_initialized)
        return true; // Already initialized

    instance = this; // Set singleton instance for ISR callback

    if (!initGPIO())
    {
        _error = true;
        // SerialDebug.println("CRITICAL: EncoderTimer GPIO initialization failed!"); // Optional critical error print
        return false;
    }

    if (!initTimer())
    {
        _error = true;
        // SerialDebug.println("CRITICAL: EncoderTimer TIM2 initialization failed!"); // Optional critical error print
        return false;
    }

    // Enable timer update interrupt (for overflow/underflow, though not strictly used for count extension yet)
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        _error = true;
        // SerialDebug.println("CRITICAL: EncoderTimer failed to start TIM2 interrupts!");
        return false;
    }
    // __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE); // Redundant if HAL_TIM_Base_Start_IT enables it. Check HAL docs.
    // Typically HAL_TIM_Base_Start_IT is sufficient.

    _initialized = true;
    _error = false; // Clear error flag on successful initialization
    // SerialDebug.println("EncoderTimer initialized successfully."); // Optional success print
    return true;
}

/**
 * @brief Initializes GPIO pins (PA0, PA1) for TIM2 encoder input channels (CH1, CH2).
 * Configures pins for Alternate Function Push-Pull with pull-up resistors.
 * @return True if GPIO initialization was successful, false otherwise.
 */
bool EncoderTimer::initGPIO()
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); // Ensure GPIOA clock is enabled

    GPIO_InitTypeDef gpio_config = {0};
    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1; // TIM2_CH1 (PA0), TIM2_CH2 (PA1)
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP; // Pull-ups are common for encoder inputs
    gpio_config.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_config.Alternate = GPIO_AF1_TIM2; // Alternate function for TIM2
    HAL_GPIO_Init(GPIOA, &gpio_config);

    // Configure PA5 for TIM2_ETR // Temporarily Commenting out PA5 GPIO config as well
    // GPIO_InitTypeDef gpio_etr_config = {0};
    // gpio_etr_config.Pin = GPIO_PIN_5; // PA5 for TIM2_ETR
    // gpio_etr_config.Mode = GPIO_MODE_AF_PP;
    // gpio_etr_config.Pull = GPIO_NOPULL; // External 5V pull-up will be used as PA5 is 5V tolerant
    // gpio_etr_config.Speed = GPIO_SPEED_FREQ_HIGH;
    // gpio_etr_config.Alternate = GPIO_AF1_TIM2; // AF1 for TIM2_ETR on PA5
    // HAL_GPIO_Init(GPIOA, &gpio_etr_config);

    return true; // Assume HAL_GPIO_Init doesn't return status, or check specific HAL version
}

/**
 * @brief Initializes TIM2 in encoder interface mode.
 * Configures TIM2 to count on TI1 and TI2 edges (quadrature mode).
 * Sets the counter period to maximum (32-bit) and enables the encoder.
 * @return True if timer initialization and start was successful, false otherwise.
 */
bool EncoderTimer::initTimer()
{
    __HAL_RCC_TIM2_CLK_ENABLE(); // Ensure TIM2 clock is enabled

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;                    // No prescaling, count every valid edge
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP; // Counter mode (UP or UP/DOWN, encoder mode overrides this)
    htim2.Init.Period = 0xFFFFFFFF;              // Full 32-bit range for encoder count
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; // Not critical for encoder mode

    TIM_Encoder_InitTypeDef encoder_config = {0};
    encoder_config.EncoderMode = TIM_ENCODERMODE_TI12;                             // Count on both TI1 and TI2 edges (x4 quadrature)
    encoder_config.IC1Polarity = TIM_ICPOLARITY_RISING;                            // Polarity for TI1
    encoder_config.IC1Selection = TIM_ICSELECTION_DIRECTTI;                        // TI1 connected to CH1 input
    encoder_config.IC1Prescaler = TIM_ICPSC_DIV1;                                  // No prescaler for input
    encoder_config.IC1Filter = SystemConfig::RuntimeConfig::Encoder::filter_level; // Input filter (0x0 to 0xF)
    encoder_config.IC2Polarity = TIM_ICPOLARITY_RISING;                            // Polarity for TI2
    encoder_config.IC2Selection = TIM_ICSELECTION_DIRECTTI;                        // TI2 connected to CH2 input
    encoder_config.IC2Prescaler = TIM_ICPSC_DIV1;                                  // No prescaler for input
    encoder_config.IC2Filter = SystemConfig::RuntimeConfig::Encoder::filter_level; // Input filter

    if (HAL_TIM_Encoder_Init(&htim2, &encoder_config) != HAL_OK)
    {
        // SerialDebug.println("CRITICAL: HAL_TIM_Encoder_Init failed!");
        return false; // HAL encoder initialization failed
    }

    // Configure TIM2 Slave Controller for ETR (Index Pulse) // Temporarily Commenting out Slave Config
    // TIM_SlaveConfigTypeDef sSlaveConfig = {0};
    // sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;               // Reset counter on ETR
    // sSlaveConfig.InputTrigger = TIM_TS_ETRF;                    // External Trigger Filtered (ETR)
    // sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_FALLING; // Active LOW Z-pulse (NPN open-collector with pull-up)
    // sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;  // No prescaler
    // sSlaveConfig.TriggerFilter = 8;                             // Increased filter value (e.g., 8, range 0x0 to 0xF)
    // if (HAL_TIM_SlaveConfigSynchronization(&htim2, &sSlaveConfig) != HAL_OK)
    // {
    //     // SerialDebug.println("CRITICAL: HAL_TIM_SlaveConfigSynchronization for ETR failed!");
    //     return false;
    // }

    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
    {
        // SerialDebug.println("CRITICAL: HAL_TIM_Encoder_Start failed!");
        return false; // Failed to start encoder channels
    }

    // Enable the TIM2 Trigger interrupt for ETR // Keep this commented for Stage 1
    // __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_TRIGGER);
    // SerialDebug.println("DEBUG: TIM2 Trigger Interrupt (ETR) enabled.");

    return true;
}

/**
 * @brief De-initializes the timer and GPIOs used by the encoder.
 * Stops the timer and disables its interrupt.
 */
void EncoderTimer::end()
{
    if (!_initialized)
        return;

    HAL_NVIC_DisableIRQ(TIM2_IRQn);                // Disable TIM2 interrupt in NVIC
    HAL_TIM_Encoder_Stop(&htim2, TIM_CHANNEL_ALL); // Stop encoder channels
    HAL_TIM_Base_DeInit(&htim2);                   // De-initialize TIM2 base
    // GPIO de-initialization could be added here if necessary
    _initialized = false;
}

/**
 * @brief Resets the software encoder count and error status.
 * Also resets the hardware timer's counter register to 0.
 * Interrupts are temporarily disabled during reset for atomicity.
 */
void EncoderTimer::reset()
{
    if (!_initialized)
        return;

    __disable_irq();
    _currentCount = 0; // Reset software state (though not primarily used for count)
    _error = false;
    __HAL_TIM_SET_COUNTER(&htim2, 0); // Reset hardware counter
    _lastUpdateTime = HAL_GetTick();  // Reset time for RPM calculation
    // Also reset static variables in calculateRPM if it's to be fully reset
    // This requires a non-const method or a different approach for calculateRPM's statics.
    // For now, calculateRPM will continue from its last state unless its statics are also reset.
    __enable_irq();
}

/**
 * @brief Gets the current raw encoder count from the hardware timer.
 * @return The 32-bit value of TIM2's counter register.
 */
int32_t EncoderTimer::getCount() const
{
    if (!_initialized)
        return 0;
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

/**
 * @brief Static ISR callback function, registered with the timer interrupt vector.
 * Calls the handleOverflow method of the singleton EncoderTimer instance.
 */
void EncoderTimer::updateCallback() // Static
{
    if (instance) // Check if instance is valid
    {
        instance->handleOverflow();
    }
}

/**
 * @brief Handles the timer update interrupt (typically overflow/underflow).
 * This method is called from the ISR context via updateCallback.
 * It updates the _lastUpdateTime timestamp.
 * Note: With a 32-bit hardware counter, overflows are rare unless the spindle runs
 * at extremely high speeds for very long periods without the count being read or reset.
 * This ISR is primarily for future extension or specific overflow handling if needed.
 */
void EncoderTimer::handleOverflow()
{
    if (!_initialized)
        return;
    _lastUpdateTime = HAL_GetTick();
    // Add any specific logic for handling a 32-bit counter overflow if necessary.
    // For typical ELS usage, the 32-bit count range is usually sufficient.
}

/**
 * @brief Internal callback called when an index pulse (ETR) occurs.
 * Sets a flag that can be checked by the main application.
 * This function is called from the HAL_TIM_TriggerCallback.
 */
void EncoderTimer::IndexPulse_Callback_Internal()
{
    _indexPulseOccurred = true;
    // SerialDebug.println("DEBUG ISR: Index Pulse (TIM2_ETR) Detected!");
}

/**
 * @brief Checks if an index pulse has occurred since the last check.
 * Clears the flag after reading.
 * @return True if an index pulse was detected, false otherwise.
 */
bool EncoderTimer::hasIndexPulseOccurred()
{
    if (_indexPulseOccurred)
    {
        _indexPulseOccurred = false; // Clear flag after checking
        return true;
    }
    return false;
}

/**
 * @brief Gets the complete current position and speed data from the encoder.
 * @return EncoderTimer::Position struct populated with current count, timestamp, RPM, direction, and validity.
 */
EncoderTimer::Position EncoderTimer::getPosition() const
{
    Position pos;
    pos.valid = false;

    if (!_initialized)
        return pos;

    pos.count = getCount();                                 // Hardware count
    pos.timestamp = HAL_GetTick();                          // Current timestamp
    pos.direction = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2); // True if counting down
    pos.rpm = calculateRPM();
    pos.valid = !_error;

    return pos;
}

/**
 * @brief Calculates the current Revolutions Per Minute (RPM) of the spindle.
 * This method uses static local variables to store the previous count and time,
 * allowing calculation of RPM based on the change in count over the change in time.
 * It respects the `SystemConfig::RuntimeConfig::Encoder::invert_direction` flag.
 * @return Calculated RPM as a signed 16-bit integer. Returns 0 if deltaTime is too small.
 */
int16_t EncoderTimer::calculateRPM() const
{
    static uint32_t lastStaticCount = 0; // Renamed to avoid confusion with member _currentCount
    static uint32_t lastStaticTime = 0;  // Using 'static' here means these persist across calls

    uint32_t currentTime = HAL_GetTick();
    uint32_t deltaTime = currentTime - lastStaticTime;

    // Prevent division by zero or noisy RPM from very small deltaTime
    if (deltaTime < SystemConfig::Limits::Encoder::MIN_RPM_DELTA_TIME_MS) // e.g., 10ms
        return 0;

    int32_t currentHardwareCount = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    int32_t deltaCounts = currentHardwareCount - lastStaticCount;

    // Update static variables for the next call
    lastStaticCount = currentHardwareCount;
    lastStaticTime = currentTime;

    if (deltaCounts == 0) // No change in count, so RPM is 0
        return 0;

    // Calculate RPM magnitude
    // RPM = (deltaCounts / (PPR * QuadratureFactor)) / (deltaTime_seconds) * 60
    // Note: TIM_ENCODERMODE_TI12 already provides x4 quadrature count.
    // So, SystemConfig::RuntimeConfig::Encoder::ppr should be the true PPR of the encoder disk.
    // The hardware counter gives (PPR * 4) counts per revolution.
    // RPM = (deltaCounts_quad / (PPR_true * 4)) / (deltaTime_ms / 1000.0) * 60.0
    // RPM = (deltaCounts_quad * 60000.0) / (PPR_true * 4 * deltaTime_ms)
    // RPM = (deltaCounts_quad * 15000.0) / (PPR_true * deltaTime_ms)
    // This matches the original scaling if SystemConfig::RuntimeConfig::Encoder::ppr is true PPR.

    // Ensure PPR is not zero to avoid division by zero
    uint32_t ppr_val = SystemConfig::RuntimeConfig::Encoder::ppr;
    if (ppr_val == 0)
        return 0; // Or handle as an error

    // Use floating point for precision in intermediate calculation
    double rpm_double = (static_cast<double>(deltaCounts) * 15000.0) / (static_cast<double>(ppr_val) * static_cast<double>(deltaTime));
    int16_t rpmMagnitude = static_cast<int16_t>(round(rpm_double)); // Round to nearest integer

    // Apply direction based on timer counting direction
    bool isCountingDown = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2);
    int16_t finalRpm = isCountingDown ? -rpmMagnitude : rpmMagnitude;

    // Apply software inversion if configured
    if (SystemConfig::RuntimeConfig::Encoder::invert_direction)
    {
        finalRpm = -finalRpm;
    }
    return finalRpm;
}

/**
 * @brief Convenience method to get the calculated RPM.
 * @return Current RPM.
 */
int16_t EncoderTimer::getRPM() const
{
    return calculateRPM();
}

// Implementation for new accessor methods from header
uint32_t EncoderTimer::getRawCounter() const
{
    if (!_initialized)
        return 0;
    return __HAL_TIM_GET_COUNTER(&htim2);
}

uint32_t EncoderTimer::getTimerStatus() const
{
    if (!_initialized)
        return 0;
    return __HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE);
}

uint32_t EncoderTimer::getTimerCR1() const
{
    if (!_initialized)
        return 0;
    return htim2.Instance->CR1;
}
