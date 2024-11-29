#include "config.h"

namespace STM32Step
{
    // Define static GPIO ports
    GPIO_TypeDef *const PinConfig::StepPin::PORT = GPIOE;
    GPIO_TypeDef *const PinConfig::DirPin::PORT = GPIOE;
    GPIO_TypeDef *const PinConfig::EnablePin::PORT = GPIOE;

    // Define static RuntimeConfig variables
    uint32_t RuntimeConfig::current_pulse_width = TimingConfig::PULSE_WIDTH;
    uint32_t RuntimeConfig::current_microsteps = MotorDefaults::DEFAULT_MICROSTEPS;
    uint32_t RuntimeConfig::current_max_speed = MotorDefaults::DEFAULT_FREQ;
    bool RuntimeConfig::invert_direction = false;
    bool RuntimeConfig::invert_enable = false;
}
