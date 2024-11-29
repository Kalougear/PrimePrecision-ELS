#include "pulse_width_timer.h"
#include "TimerField.h"

// Define static member
TIM_HandleTypeDef PulseWidthTimer::htim4;

// Static callback function
static void pulseTimerCallback()
{
    TeensyStep::TimerField::pulseTimerCallback();
}

void PulseWidthTimer::init()
{
    // Enable TIM4 clock
    __HAL_RCC_TIM4_CLK_ENABLE();

    // Initialize Timer with low-level configuration
    htim4 = {0};
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = (SystemCoreClock / 1000000) - 1; // 1MHz timer clock for microsecond precision
    htim4.Init.Period = 5 - 1;                              // Default 5µs pulse width
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.RepetitionCounter = 0;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
    {
        return;
    }

    // Configure interrupt with high priority
    HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

void PulseWidthTimer::setPulseWidth(uint32_t microseconds)
{
    if (microseconds < 1)
        microseconds = 1; // Minimum 1µs pulse width
    else if (microseconds > 100)
        microseconds = 100; // Maximum 100µs pulse width

    // Set period directly
    TIM4->ARR = microseconds - 1;
}

void PulseWidthTimer::start()
{
    HAL_TIM_Base_Start_IT(&htim4);
}

void PulseWidthTimer::stop()
{
    HAL_TIM_Base_Stop_IT(&htim4);
}

void PulseWidthTimer::refresh()
{
    TIM4->EGR = TIM_EGR_UG; // Generate update event
}
