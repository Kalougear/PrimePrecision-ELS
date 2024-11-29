#pragma once

#include <cstdint>
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_cortex.h"

class AccelerationTimer
{
public:
    static TIM_HandleTypeDef htim6;

    static void init();
    static void setUpdatePeriod(uint32_t microseconds);
    static void start();
    static void stop();
    static void refresh();
};
