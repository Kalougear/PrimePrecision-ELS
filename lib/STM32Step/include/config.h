#pragma once
#include <Arduino.h>
#include "stm32h7xx_hal.h"

/**
 * @brief STM32Step Library Hardware Configuration
 *
 * Contains hardware-specific settings for the stepper motor driver.
 * Runtime configurations are handled by the global SystemConfig.
 */
namespace STM32Step
{
    /**
     * @brief Operation modes for the stepper motor
     */
    enum class OperationMode
    {
        THREADING, ///< Synchronized with encoder for threading
        TURNING,   ///< Regular turning operation
        RAPIDS,    ///< Rapid movement
        IDLE       ///< Motor stopped
    };

    /**
     * @brief Hardware pin configuration
     */
    struct PinConfig
    {
        // Timer Output Pin (PWM)
        struct StepPin
        {
            static constexpr uint8_t PIN = 9; ///< PE9 - TIM1_CH1
            static GPIO_TypeDef *const PORT;  ///< GPIOE
        };

        // Direction Control
        struct DirPin
        {
            static constexpr uint8_t PIN = 8; ///< PE8
            static GPIO_TypeDef *const PORT;  ///< GPIOE
        };

        // Enable Control
        struct EnablePin
        {
            static constexpr uint8_t PIN = 7; ///< PE7
            static GPIO_TypeDef *const PORT;  ///< GPIOE
        };
    };

    /**
     * @brief Timer hardware configuration
     */
    struct TimerConfig
    {
        static constexpr uint32_t TIMER_INSTANCE = 1;          ///< TIM1
        static constexpr uint32_t PWM_CHANNEL = TIM_CHANNEL_1; ///< Timer channel for PWM
        static constexpr uint32_t SAFE_STOP_FREQ = 1000;       ///< 1kHz safe stop frequency
        static constexpr uint32_t GPIO_AF = GPIO_AF1_TIM1;     ///< GPIO alternate function
    };

    /**
     * @brief Hardware timing parameters
     */
    struct TimingConfig
    {
        static constexpr uint32_t STEPPER_CYCLE_US = 3; ///< Base cycle time (μs)
        static constexpr uint32_t PULSE_WIDTH = 5;      ///< Step pulse width (μs)
        static constexpr uint32_t DIR_SETUP = 6;        ///< Direction setup time (μs)
        static constexpr uint32_t ENABLE_SETUP = 5;     ///< Enable setup time (μs)
    };

} // namespace STM32Step
