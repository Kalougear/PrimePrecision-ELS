// src/Core/encoder_timer.cpp
#include "Core/encoder_timer.h"
#include "Config/serial_debug.h"

EncoderTimer *EncoderTimer::instance = nullptr;

EncoderTimer::EncoderTimer() : _currentCount(0),
                               _lastUpdateTime(0),
                               _error(false),
                               _initialized(false),
                               _syncEnabled(false)
{
    memset(static_cast<void *>(&htim2), 0, sizeof(htim2));
    memset(static_cast<void *>(&_syncConfig), 0, sizeof(_syncConfig));
}

EncoderTimer::~EncoderTimer()
{
    end();
    if (instance == this)
    {
        instance = nullptr;
    }
}

bool EncoderTimer::begin()
{
    if (_initialized)
        return true;

    SerialDebug.println("Starting encoder timer initialization...");
    instance = this;

    // Initialize components one by one with error checking
    SerialDebug.println("Initializing GPIO...");
    if (!initGPIO())
    {
        SerialDebug.println("GPIO initialization failed");
        return false;
    }
    SerialDebug.println("GPIO initialized");

    SerialDebug.println("Initializing Timer...");
    if (!initTimer())
    {
        SerialDebug.println("Timer initialization failed");
        return false;
    }
    SerialDebug.println("Timer initialized");

    // Enable timer update interrupt for overflow detection
    SerialDebug.println("Enabling timer interrupts...");
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        SerialDebug.println("Failed to start timer interrupts");
        return false;
    }
    SerialDebug.println("Timer base started with interrupts");

    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
    SerialDebug.println("Timer update interrupt enabled");

    _initialized = true;
    SerialDebug.println("Encoder timer initialization complete");
    return true;
}

bool EncoderTimer::initGPIO()
{
    SerialDebug.println("Enabling GPIO clock...");
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio_config = {0};
    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_config.Alternate = GPIO_AF1_TIM2;

    SerialDebug.println("Configuring GPIO pins...");
    HAL_GPIO_Init(GPIOA, &gpio_config);
    SerialDebug.println("GPIO pins configured");
    return true;
}

bool EncoderTimer::initTimer()
{
    SerialDebug.println("Enabling Timer2 clock...");
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

    SerialDebug.println("Initializing encoder timer...");
    if (HAL_TIM_Encoder_Init(&htim2, &encoder_config) != HAL_OK)
    {
        SerialDebug.println("Timer initialization failed");
        return false;
    }

    SerialDebug.println("Starting encoder timer...");
    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
    {
        SerialDebug.println("Timer start failed");
        return false;
    }

    SerialDebug.println("Timer configuration complete");
    return true;
}

void EncoderTimer::end()
{
    if (!_initialized)
        return;

    HAL_NVIC_DisableIRQ(TIM2_IRQn);
    HAL_TIM_Encoder_Stop(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Base_DeInit(&htim2);
    _initialized = false;
}

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

int32_t EncoderTimer::getCount()
{
    if (!_initialized)
        return 0;

    __disable_irq();
    int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    __enable_irq();
    return count;
}

void EncoderTimer::setSyncConfig(const SyncConfig &config)
{
    __disable_irq();
    _syncConfig = config;
    _lastSyncCount = _currentCount;
    __enable_irq();
}

EncoderTimer::SyncPosition EncoderTimer::getSyncPosition()
{
    SyncPosition pos;
    pos.valid = false;

    if (!_initialized || !_syncEnabled)
        return pos;

    __disable_irq();
    pos.encoder_count = getCount();
    pos.required_steps = calculateRequiredSteps(pos.encoder_count);
    pos.timestamp = HAL_GetTick();
    pos.valid = !_error;
    __enable_irq();

    return pos;
}

float EncoderTimer::calculateStepperFrequency(int16_t rpm)
{
    if (rpm == 0)
        return 0;

    float steps_per_rev = _syncConfig.stepper_steps * _syncConfig.microsteps;
    float sync_ratio = calculateSyncRatio();
    float rps = rpm / 60.0f;

    return rps * steps_per_rev * sync_ratio;
}

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

float EncoderTimer::calculateSyncRatio() const
{
    return _syncConfig.thread_pitch / _syncConfig.leadscrew_pitch;
}

void EncoderTimer::updateCallback()
{
    if (instance)
    {
        instance->handleOverflow();
    }
}

void EncoderTimer::handleOverflow()
{
    if (!_initialized)
        return;

    _currentCount = getCount();
    _lastUpdateTime = HAL_GetTick();
}

EncoderTimer::Position EncoderTimer::getPosition()
{
    Position pos;
    pos.valid = false;

    if (!_initialized)
        return pos;

    __disable_irq();
    pos.count = getCount();
    pos.timestamp = HAL_GetTick();
    pos.direction = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2);
    pos.rpm = calculateRPM();
    pos.valid = !_error;
    __enable_irq();

    return pos;
}

int16_t EncoderTimer::calculateRPM()
{
    static uint32_t lastCount = 0;
    static uint32_t lastTime = 0;
    uint32_t currentTime = HAL_GetTick();
    uint32_t deltaTime = currentTime - lastTime;

    if (deltaTime < 10)
        return 0;

    int32_t currentCount = getCount();
    int32_t deltaCounts = currentCount - lastCount;

    lastCount = currentCount;
    lastTime = currentTime;

    return ((int32_t)(deltaCounts * 15000)) /
           (EncoderConfig::RuntimeConfig::ppr * deltaTime);
}

int16_t EncoderTimer::getRPM()
{
    return calculateRPM();
}
