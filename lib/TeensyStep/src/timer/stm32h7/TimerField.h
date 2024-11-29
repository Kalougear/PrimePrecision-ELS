#pragma once

#include "../TF_Handler.h"
#include "stm32h7xx_hal.h"
#include "step_timer.h"
#include "acceleration_timer.h"
#include "pulse_width_timer.h"

namespace TeensyStep
{
  class TimerField
  {
  public:
    static TF_Handler *currentHandler;
    static bool stepTimerRunning;

    inline TimerField(TF_Handler *_handler) : handler(_handler)
    {
      currentHandler = _handler;
      StepTimer::init();
      AccelerationTimer::init();
      PulseWidthTimer::init();
    }

    inline bool begin()
    {
      return true;
    }

    inline void end()
    {
      StepTimer::stop();
      AccelerationTimer::stop();
      PulseWidthTimer::stop();
      stepTimerRunning = false;
    }

    inline void endAfterPulse()
    {
      end();
    }

    inline void stepTimerStart()
    {
      StepTimer::start();
      StepTimer::refresh(); // Using StepTimer's refresh
      stepTimerRunning = true;
    }

    inline void stepTimerStop()
    {
      StepTimer::stop();
      stepTimerRunning = false;
    }

    inline void setStepFrequency(unsigned f)
    {
      if (f == 0)
      {
        stepTimerStop();
        return;
      }
      StepTimer::setFrequency(f);
    }

    inline unsigned getStepFrequency()
    {
      return StepTimer::getTimerClock();
    }

    inline bool stepTimerIsRunning() const
    {
      return StepTimer::isRunning();
    }

    inline void accTimerStart()
    {
      AccelerationTimer::start();
      TIM6->EGR = TIM_EGR_UG; // Direct register access for refresh
    }

    inline void accTimerStop()
    {
      AccelerationTimer::stop();
    }

    inline void setAccUpdatePeriod(unsigned period)
    {
      AccelerationTimer::setUpdatePeriod(period);
    }

    inline void triggerDelay()
    {
      PulseWidthTimer::start();
    }

    inline void setPulseWidth(unsigned pulseWidth)
    {
      PulseWidthTimer::setPulseWidth(pulseWidth);
    }

    // Make callbacks public and static
    static void stepTimerCallback();
    static void accTimerCallback();
    static void pulseTimerCallback();

  protected:
    TF_Handler *handler;
  };

} // namespace TeensyStep
