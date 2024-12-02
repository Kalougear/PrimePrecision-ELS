/**
 * @file config.cpp
 * @brief Hardware configuration implementation for STM32Step library
 *
 * Contains only hardware-specific initializations.
 * Runtime configurations are handled by SystemConfig.
 */

#include "config.h"

namespace STM32Step
{
    // Initialize static GPIO port definitions
    GPIO_TypeDef *const PinConfig::StepPin::PORT = GPIOE;   // PE9 - TIM1_CH1
    GPIO_TypeDef *const PinConfig::DirPin::PORT = GPIOE;    // PE8
    GPIO_TypeDef *const PinConfig::EnablePin::PORT = GPIOE; // PE7

} // namespace STM32Step
