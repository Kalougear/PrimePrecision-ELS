#include "acceleration_timer.h"
#include "TimerField.h"
#include <HardwareTimer.h>

// Define static member
static HardwareTimer *timer = nullptr;

void AccelerationTimer::init()
{
    // Create HardwareTimer instance for TIM6
    timer = new HardwareTimer(TIM6);

    // Configure timer
    timer->setPrescaleFactor((SystemCoreClock / 1000000)); // 1MHz timer clock for microsecond precision
    timer->setOverflow(1000);                              // 1kHz default update rate

    // Attach callback
    timer->attachInterrupt([]()
                           { TeensyStep::TimerField::accTimerCallback(); });
}

void AccelerationTimer::setUpdatePeriod(uint32_t microseconds)
{
    if (!timer)
        return;

    if (microseconds < 100)
        microseconds = 100; // Minimum 100µs period

    timer->setOverflow(microseconds);
}

void AccelerationTimer::start()
{
    if (timer)
        timer->resume();
}

void AccelerationTimer::stop()
{
    if (timer)
        timer->pause();
}

void AccelerationTimer::refresh()
{
    if (timer)
        timer->refresh();
}
