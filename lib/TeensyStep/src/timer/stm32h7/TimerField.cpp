#include "TimerField.h"

namespace TeensyStep
{

  bool TimerField::stepTimerRunning = false;
  TF_Handler *TimerField::currentHandler = nullptr;

  void TimerField::stepTimerCallback()
  {
    if (currentHandler)
    {
      currentHandler->stepTimerISR();
    }
  }

  void TimerField::accTimerCallback()
  {
    if (currentHandler)
    {
      currentHandler->accTimerISR();
    }
  }

  void TimerField::pulseTimerCallback()
  {
    if (currentHandler)
    {
      currentHandler->pulseTimerISR();
    }
  }

} // namespace TeensyStep
