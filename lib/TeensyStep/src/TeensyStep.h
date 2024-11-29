#pragma once

#include "RotateControlBase.h"
#include "StepControlBase.h"
#include "Stepper.h"
#include "accelerators/LinRotAccelerator.h"
#include "accelerators/LinStepAccelerator.h"
#include "version.h"

#include "timer/generic/TimerField.h"

// TEENSY 3.0 - Teensy 3.6
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
#include "timer/teensy3/TimerField2.h"

// TEENSY 4
#elif defined(__IMXRT1052__)
#include "timer/teensy4/TimerField.h"

// STM32F4
#elif defined(STM32F4xx)
#include "timer/stm32/TimerField.h"

// STM32H7
#elif defined(STM32H7xx)
#include "timer/stm32h7/TimerField.h"

#elif defined(__someHardware_TBD__)
#include "timers/someHardware/TimerField2.h"
#endif

namespace TeensyStep
{
    // Linear acceleration
    using RotateControl = RotateControlBase<LinRotAccelerator, TimerField>;
    using StepControl = StepControlBase<LinStepAccelerator, TimerField>;

    using StepControlTick = StepControlBase<LinStepAccelerator, TimerField>;
    using RotateControlTick = RotateControlBase<LinStepAccelerator, TimerField>;
}

using TeensyStep::RotateControl;
using TeensyStep::RotateControlTick;
using TeensyStep::StepControl;
using TeensyStep::StepControlTick;
using TeensyStep::Stepper;
