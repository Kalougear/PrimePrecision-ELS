#pragma once
#include <Arduino.h>
#include "stm32h7xx_hal.h"

namespace STM32Step
{
#define STEPPER_CYCLE_US 10 // 5 microseconds per cycle
    // Operation Mode Definition
    enum class OperationMode
    {
        THREADING,
        TURNING,
        RAPIDS,
        IDLE
    };

    // GPIO Pin Configuration
    struct PinConfig
    {
        // Timer Output Pin (PWM)
        struct StepPin
        {
            static constexpr uint8_t PIN = 9; // PE9 - TIM1_CH1
            static GPIO_TypeDef *const PORT;
        };

        // Direction Control
        struct DirPin
        {
            static constexpr uint8_t PIN = 8; // PE8
            static GPIO_TypeDef *const PORT;
        };

        // Enable Control
        struct EnablePin
        {
            static constexpr uint8_t PIN = 7; // PE7
            static GPIO_TypeDef *const PORT;
        };
    };

    // Timer Configuration
    struct TimerConfig
    {
        static constexpr uint32_t TIMER_INSTANCE = 1;          // TIM1
        static constexpr uint32_t PWM_CHANNEL = TIM_CHANNEL_1; // Changed to HAL constant
        static constexpr uint32_t SAFE_STOP_FREQ = 1000;       // 1kHz safe stop frequency
        static constexpr uint32_t GPIO_AF = GPIO_AF1_TIM1;
    };

    // Motor Configuration Structure (for runtime configuration)
    struct MotorConfig
    {
        OperationMode mode;
        uint32_t maxSpeed;
        uint32_t microsteps;
        bool invertDirection;
        bool invertEnable;
    };

    // Motor Default Values
    struct MotorDefaults
    {
        // Speed Limits
        static constexpr uint32_t MAX_FREQ = 20000;        // Maximum frequency 20kHz
        static constexpr uint32_t MIN_FREQ = 100;          // Minimum frequency 100Hz
        static constexpr uint32_t DEFAULT_FREQ = 1000;     // Default 1kHz
        static constexpr uint32_t DEFAULT_MICROSTEPS = 16; // Default microsteps
    };

    // Timing Parameters
    struct TimingConfig
    {
        // All times in microseconds
        static constexpr uint32_t PULSE_WIDTH = 5;  // Step pulse width
        static constexpr uint32_t DIR_SETUP = 6;    // Direction setup time
        static constexpr uint32_t ENABLE_SETUP = 5; // Enable setup time
    };

    // Operation Mode Limits
    struct OperationLimits
    {
        static constexpr uint32_t THREADING_MAX = 20000; // 20kHz
        static constexpr uint32_t TURNING_MAX = 20000;   // 20kHz
        static constexpr uint32_t RAPIDS_MAX = 20000;    // 20kHz
    };

    // Variables that can be modified runtime (for Nextion control)
    struct RuntimeConfig
    {
        static uint32_t current_pulse_width;
        static uint32_t current_microsteps;
        static uint32_t current_max_speed;
        static bool invert_direction;
        static bool invert_enable;
    };

} // namespace STM32Step
