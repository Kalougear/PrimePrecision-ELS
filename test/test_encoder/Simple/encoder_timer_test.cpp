// src/Core/encoder_timer.cpp

#include "encoder_timer_test.h"

EncoderTimer::EncoderTimer(void) : initialized_(false)

{

    memset(&h_tim2_, 0, sizeof(h_tim2_));
}

EncoderTimer::~EncoderTimer(void)

{

    end();
}

bool EncoderTimer::begin(void)

{

    if (initialized_)

    {

        return true;
    }

    // Enable timer clock

    __HAL_RCC_TIM2_CLK_ENABLE();

    if (!init_gpio() || !init_timer())

    {

        return false;
    }

    initialized_ = true;

    return true;
}

void EncoderTimer::end(void)

{

    if (!initialized_)

    {

        return;
    }

    HAL_TIM_Encoder_DeInit(&h_tim2_);

    initialized_ = false;
}

bool EncoderTimer::init_gpio(void)

{

    // Enable GPIO clock

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure encoder pins

    GPIO_InitTypeDef gpio_config = {0};

    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;

    gpio_config.Mode = GPIO_MODE_AF_PP;

    gpio_config.Pull = GPIO_PULLUP;

    gpio_config.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio_config.Alternate = GPIO_AF1_TIM2;

    HAL_GPIO_Init(GPIOA, &gpio_config);

    return true;
}

bool EncoderTimer::init_timer(void)

{

    // Configure timer base

    h_tim2_.Instance = TIM2;

    h_tim2_.Init.Prescaler = 0;

    h_tim2_.Init.CounterMode = TIM_COUNTERMODE_UP;

    h_tim2_.Init.Period = 0xFFFFFFFF;

    h_tim2_.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    h_tim2_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    // Configure encoder mode

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

    if (HAL_TIM_Encoder_Init(&h_tim2_, &encoder_config) != HAL_OK)

    {

        return false;
    }

    // Start encoder

    if (HAL_TIM_Encoder_Start(&h_tim2_, TIM_CHANNEL_ALL) != HAL_OK)

    {

        return false;
    }

    return true;
}

int32_t EncoderTimer::get_count(void)

{

    if (!initialized_)

    {

        return 0;
    }

    return (int32_t)__HAL_TIM_GET_COUNTER(&h_tim2_);
}

void EncoderTimer::reset_count(void)

{

    if (!initialized_)

    {

        return;
    }

    __HAL_TIM_SET_COUNTER(&h_tim2_, 0);
}