// src/Core/encoder_timer.cpp
/**
 * @file encoder_timer.cpp
 * @brief Quadrature encoder implementation using hardware timer and DMA
 *
 * This implementation uses TIM2 configured in encoder mode to directly count
 * quadrature encoder pulses. The timer handles all quadrature decoding in hardware,
 * while DMA is used to continuously monitor timer updates for reliable position tracking
 * at high speeds.
 *
 * Features:
 * - Hardware quadrature decoding using TIM2
 * - DMA-based position monitoring
 * - Automatic overflow handling
 * - RPM calculation
 * - Support for electronic leadscrew synchronization
 */

#include "Core/encoder_timer.h"
#include "Config/serial_debug.h"

// Static instance pointer initialization
EncoderTimer *EncoderTimer::instance = nullptr;

// Constructor / Destructor
EncoderTimer::EncoderTimer() : _currentCount(0),
                               _lastUpdateTime(0),
                               _error(false),
                               _initialized(false),
                               _syncEnabled(false)
{
    memset(&_hdma, 0, sizeof(_hdma));
    memset(&htim2, 0, sizeof(htim2));
    memset(&_syncConfig, 0, sizeof(_syncConfig));
    memset((void *)_dmaBuffer, 0, sizeof(_dmaBuffer));
}

EncoderTimer::~EncoderTimer()
{
    end();
    if (instance == this)
    {
        instance = nullptr;
    }
}

/**
 * @brief Initialize encoder hardware
 *
 * Configures GPIO pins, timer, and DMA for quadrature encoder operation.
 * Uses TIM2 in encoder mode with maximum filtering for noise immunity.
 * DMA is configured to monitor timer updates for reliable position tracking.
 */
bool EncoderTimer::begin()
{
    if (_initialized)
        return true;

    instance = this;
    SerialDebug.println("Initializing Encoder...");

    if (!initGPIO() || !initTimer() || !initDMA())
    {
        SerialDebug.println("Encoder initialization failed");
        return false;
    }

    // Enable timer counter DMA request
    TIM2->DIER |= TIM_DIER_UDE; // Enable Update DMA request
    TIM2->DIER |= TIM_DIER_UIE; // Enable Update interrupt for overflow detection

    // Start DMA transfer from counter register
    if (HAL_DMA_Start_IT(&_hdma, (uint32_t)&TIM2->CNT,
                         (uint32_t)(void *)_dmaBuffer, DMA_BUFFER_SIZE) != HAL_OK)
    {
        SerialDebug.println("DMA start failed");
        return false;
    }

    // Enable DMA stream
    __HAL_DMA_ENABLE(&_hdma);

    _initialized = true;
    SerialDebug.println("Encoder initialized successfully");
    return true;
}

/**
 * @brief Configure GPIO pins for encoder input
 *
 * Sets up PA0 and PA1 as timer inputs with pull-up resistors
 * and maximum speed for reliable signal capture.
 */
bool EncoderTimer::initGPIO()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio_config = {0};
    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_config.Alternate = GPIO_AF1_TIM2;

    HAL_GPIO_Init(GPIOA, &gpio_config);
    return true;
}

/**
 * @brief Configure timer for encoder mode
 *
 * Sets up TIM2 in quadrature encoder mode with:
 * - Maximum input filtering
 * - Both edges counting
 * - 32-bit counter range
 * - No prescaler for maximum resolution
 */
bool EncoderTimer::initTimer()
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef encoder_config = {0};
    encoder_config.EncoderMode = TIM_ENCODERMODE_TI12;
    encoder_config.IC1Polarity = TIM_ICPOLARITY_RISING;
    encoder_config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    encoder_config.IC1Prescaler = TIM_ICPSC_DIV1;
    encoder_config.IC1Filter = 0xF;
    encoder_config.IC2Polarity = TIM_ICPOLARITY_RISING;
    encoder_config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    encoder_config.IC2Prescaler = TIM_ICPSC_DIV1;
    encoder_config.IC2Filter = 0xF;

    if (HAL_TIM_Encoder_Init(&htim2, &encoder_config) != HAL_OK)
    {
        SerialDebug.println("Timer init failed");
        return false;
    }

    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
    {
        SerialDebug.println("Timer start failed");
        return false;
    }

    return true;
}

/**
 * @brief Configure DMA for timer monitoring
 *
 * Sets up DMA to monitor timer counter value:
 * - Uses circular mode for continuous operation
 * - High priority for reliable tracking
 * - Word-size transfers for 32-bit counter
 */
bool EncoderTimer::initDMA()
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    _hdma.Instance = DMA1_Stream1;
    _hdma.Init.Request = DMA_REQUEST_TIM2_UP;
    _hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    _hdma.Init.PeriphInc = DMA_PINC_DISABLE;
    _hdma.Init.MemInc = DMA_MINC_ENABLE;
    _hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    _hdma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    _hdma.Init.Mode = DMA_CIRCULAR;
    _hdma.Init.Priority = DMA_PRIORITY_HIGH;
    _hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&_hdma) != HAL_OK)
    {
        SerialDebug.println("DMA init failed");
        return false;
    }

    _hdma.XferCpltCallback = dmaCallback;
    __HAL_LINKDMA(&htim2, hdma[TIM_DMA_ID_UPDATE], _hdma);

    // Enable DMA IRQ
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

    SerialDebug.println("DMA initialized successfully");
    return true;
}

/**
 * @brief Clean shutdown of encoder hardware
 */
void EncoderTimer::end()
{
    if (!_initialized)
        return;

    // Stop DMA transfer
    HAL_DMA_Abort(&_hdma);
    __HAL_DMA_DISABLE(&_hdma);

    HAL_TIM_Encoder_Stop(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Base_DeInit(&htim2);
    HAL_DMA_DeInit(&_hdma);
    _initialized = false;
}

/**
 * @brief Reset encoder position to zero
 */
void EncoderTimer::reset()
{
    if (!_initialized)
        return;

    __disable_irq();
    _currentCount = 0;
    _error = false;
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __enable_irq();
}

/**
 * @brief Get current encoder count
 * @return Raw timer counter value
 */
int32_t EncoderTimer::getCount()
{
    if (!_initialized)
        return 0;

    return (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

/**
 * @brief Configure synchronization parameters
 * Used for electronic leadscrew operation
 */
void EncoderTimer::setSyncConfig(const SyncConfig &config)
{
    __disable_irq();
    _syncConfig = config;
    _lastSyncCount = _currentCount;
    __enable_irq();
}

/**
 * @brief Get synchronized position data
 * @return Position data for leadscrew synchronization
 */
EncoderTimer::SyncPosition EncoderTimer::getSyncPosition()
{
    SyncPosition pos;
    pos.valid = false;

    if (!_initialized || !_syncEnabled)
        return pos;

    __disable_irq();
    _currentCount = getCount();
    pos.encoder_count = _currentCount;
    pos.required_steps = calculateRequiredSteps(_currentCount);
    pos.timestamp = HAL_GetTick();
    pos.valid = !_error;
    __enable_irq();

    return pos;
}

/**
 * @brief Calculate stepper frequency for given RPM
 * Used for speed synchronization
 */
float EncoderTimer::calculateStepperFrequency(int16_t rpm)
{
    if (rpm == 0)
        return 0;

    float steps_per_rev = _syncConfig.stepper_steps * _syncConfig.microsteps;
    float sync_ratio = calculateSyncRatio();
    float rps = rpm / 60.0f;

    return rps * steps_per_rev * sync_ratio;
}

/**
 * @brief Calculate required stepper steps
 * Used for position synchronization
 */
int32_t EncoderTimer::calculateRequiredSteps(int32_t encoderCount)
{
    int32_t counts_per_rev = EncoderConfig::RuntimeConfig::ppr * 4;
    float sync_ratio = calculateSyncRatio();

    float steps_per_count = (_syncConfig.stepper_steps * _syncConfig.microsteps * sync_ratio) /
                            counts_per_rev;

    int32_t delta_counts = encoderCount - _lastSyncCount;
    float required_steps = delta_counts * steps_per_count;

    return _syncConfig.reverse_sync ? -(int32_t)required_steps : (int32_t)required_steps;
}

/**
 * @brief Calculate synchronization ratio
 * Based on thread and leadscrew pitch
 */
float EncoderTimer::calculateSyncRatio() const
{
    return _syncConfig.thread_pitch / _syncConfig.leadscrew_pitch;
}

/**
 * @brief Handle timer overflow via DMA
 */
void EncoderTimer::updateCallback(void)
{
    if (instance)
        instance->handleOverflow();
}

/**
 * @brief Process timer overflow
 * Updates position tracking on counter wrap
 */
void EncoderTimer::handleOverflow()
{
    if (!_initialized)
        return;

    _currentCount = getCount();

    // Process latest DMA buffer value
    uint32_t latest_count = _dmaBuffer[DMA_BUFFER_SIZE - 1];

    if (SerialDebug.available()) // Only print if debug is enabled
    {
        SerialDebug.print("Counter: ");
        SerialDebug.print(_currentCount);
        SerialDebug.print(" DMA Count: ");
        SerialDebug.print(latest_count);
        SerialDebug.print(" Direction: ");
        SerialDebug.println(__HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2) ? "DOWN" : "UP");
    }
}

/**
 * @brief Get detailed position information
 * @return Position structure with count, direction, and speed
 */
EncoderTimer::Position EncoderTimer::getPosition()
{
    Position pos;
    pos.valid = false;

    if (!_initialized)
        return pos;

    __disable_irq();
    _currentCount = getCount();
    pos.count = _currentCount;
    pos.timestamp = HAL_GetTick();
    pos.direction = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2);
    pos.rpm = calculateRPM();
    pos.valid = !_error;
    __enable_irq();

    return pos;
}

/**
 * @brief Calculate current RPM
 * Uses time-delta method for speed calculation
 */
int16_t EncoderTimer::calculateRPM()
{
    static uint32_t lastCount = 0;
    static uint32_t lastTime = 0;
    uint32_t currentTime = HAL_GetTick();
    uint32_t deltaTime = currentTime - lastTime;

    if (deltaTime < 10) // Minimum 10ms between calculations
        return 0;

    int32_t currentCount = getCount();
    int32_t deltaCounts = currentCount - lastCount;

    lastCount = currentCount;
    lastTime = currentTime;

    // RPM = (delta counts * 60000) / (pulses per rev * time in ms)
    return ((int32_t)(deltaCounts * 15000)) /
           (EncoderConfig::RuntimeConfig::ppr * deltaTime);
}

/**
 * @brief Get current RPM
 */
int16_t EncoderTimer::getRPM()
{
    return calculateRPM();
}

/**
 * @brief DMA transfer complete callback
 * Updates timestamp for overflow tracking
 */
void EncoderTimer::dmaCallback(DMA_HandleTypeDef *hdma)
{
    if (!instance || !hdma)
        return;

    if (hdma->Instance == instance->_hdma.Instance)
    {
        instance->_lastUpdateTime = HAL_GetTick();

        // Process latest DMA buffer values
        instance->handleOverflow();
    }
}
