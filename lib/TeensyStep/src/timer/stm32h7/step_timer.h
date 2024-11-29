#pragma once

#include <cstdint>
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_cortex.h"

class StepTimer
{
public:
    static TIM_HandleTypeDef htim1;

    static void init();
    static void setFrequency(uint32_t freq);
    static void start();
    static void stop();
    static uint32_t getTimerClock();
    static bool isRunning();
    static void refresh();
};
