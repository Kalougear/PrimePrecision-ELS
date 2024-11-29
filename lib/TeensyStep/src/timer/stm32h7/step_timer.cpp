#include "step_timer.h"
#include <Arduino.h>
#include "TimerField.h"
#include <HardwareTimer.h>
#include "config/serial_debug.h"

static HardwareTimer *timer = nullptr;

void StepTimer::init()
{
    // Create HardwareTimer instance for TIM1
    timer = new HardwareTimer(TIM1);

    // Configure timer base
    timer->setPrescaleFactor(100); // 200MHz/100 = 2MHz timer clock
    timer->setOverflow(1000);      // Initial period

    // Configure channel 1 in Output Compare Toggle Mode
    timer->setMode(1, TIMER_OUTPUT_COMPARE_TOGGLE, PE9);

    // Set compare value to 50% duty cycle
    timer->setCaptureCompare(1, 500, TICK_COMPARE_FORMAT);

    // Attach callback
    timer->attachInterrupt(1, []()
                           { TeensyStep::TimerField::stepTimerCallback(); });
}

void StepTimer::setFrequency(uint32_t freq)
{
    if (!timer)
        return;

    if (freq == 0)
    {
        timer->pause();
        return;
    }

    // For toggle mode, we need twice the frequency since each step needs two toggles
    // At 2MHz timer clock, for 10000 steps/sec we need 20000 toggles/sec
    // So period = 2000000/(2*freq)
    uint32_t period = 2000000 / (2 * freq);

    // Ensure minimum period
    if (period < 2)
        period = 2;

    timer->pause();
    timer->setOverflow(period);
    timer->setCaptureCompare(1, period / 2, TICK_COMPARE_FORMAT); // 50% duty cycle
    timer->refresh();
    timer->resume();
}

void StepTimer::start()
{
    SerialDebug.println("Timer start called");
    if (timer)
    {
        timer->resume();
    }
    SerialDebug.printf("Timer started\n");
}

void StepTimer::stop()
{
    if (timer)
    {
        timer->pause();
    }
}

uint32_t StepTimer::getTimerClock()
{
    return 200000000; // 200MHz timer input clock
}

bool StepTimer::isRunning()
{
    return timer ? timer->isRunning() : false;
}

void StepTimer::refresh()
{
    if (timer)
    {
        timer->refresh();
    }
}
