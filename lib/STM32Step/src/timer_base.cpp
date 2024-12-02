#include "timer_base.h"
#include "stepper.h"
#include "Config/serial_debug.h"
#include "Config/system_config.h"

namespace STM32Step
{
    // Static member initialization
    HardwareTimer *TimerControl::htim = nullptr;
    volatile bool TimerControl::running = false;
    Stepper *TimerControl::currentStepper = nullptr;
    volatile bool TimerControl::positionReached = false;
    volatile TimerControl::MotorState TimerControl::currentState = TimerControl::MotorState::IDLE;
    volatile bool TimerControl::emergencyStop = false;

    void onUpdateCallback()
    {
        if (TimerControl::getCurrentState() == TimerControl::MotorState::RUNNING &&
            TimerControl::currentStepper != nullptr)
        {
            TimerControl::currentStepper->ISR();
        }
    }

    void TimerControl::init()
    {
        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();

        // Initialize Timer
        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // Get timer clock frequency (APB2 = 100MHz)
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();

        // Calculate base frequency taking into account pulse width
        // Base frequency should allow for complete pulse cycles
        uint32_t minCycleTime = SystemConfig::Limits::Stepper::PULSE_WIDTH_US * 2; // Double the pulse width for complete cycle
        uint32_t baseFreq = 1000000 / minCycleTime;                                // Convert microseconds to frequency

        // Calculate period for the base frequency
        uint32_t cyclesPerPeriod = timerClock / baseFreq;

        // Configure timer for calculated frequency
        htim->setPrescaleFactor(1); // No prescaler needed at 100MHz
        htim->setOverflow(cyclesPerPeriod);
        htim->setMode(1, TIMER_OUTPUT_COMPARE);

        // Configure timer parameters
        TIM_HandleTypeDef *handle = htim->getHandle();
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // Attach interrupt callback
        htim->attachInterrupt(onUpdateCallback);

        SerialDebug.print("Timer Config - Base Freq: ");
        SerialDebug.print(baseFreq);
        SerialDebug.print(" Hz, Timer Clock: ");
        SerialDebug.print(timerClock / 1000000);
        SerialDebug.print(" MHz, Period: ");
        SerialDebug.print(cyclesPerPeriod);
        SerialDebug.print(", Pulse Width: ");
        SerialDebug.print(SystemConfig::Limits::Stepper::PULSE_WIDTH_US);
        SerialDebug.println(" us");

        currentState = MotorState::IDLE;
    }

    void TimerControl::start(Stepper *stepper)
    {
        if (!stepper || currentState != MotorState::IDLE)
            return;

        currentStepper = stepper;
        running = true;
        positionReached = false;
        currentState = MotorState::RUNNING;

        // Start the timer
        if (htim)
        {
            htim->resume();
        }
    }

    void TimerControl::stop()
    {
        if (!running || !htim)
            return;

        htim->pause();

        running = false;
        positionReached = true;

        if (currentStepper)
        {
            currentStepper->_running = false;
        }

        currentStepper = nullptr;
        currentState = MotorState::IDLE;
    }

    bool TimerControl::checkTargetPosition()
    {
        if (!currentStepper)
            return true;
        return currentStepper->getCurrentPosition() == currentStepper->getTargetPosition();
    }

    bool TimerControl::isTargetPositionReached()
    {
        return positionReached;
    }

    void TimerControl::emergencyStopRequest()
    {
        emergencyStop = true;
        stop();
    }

} // namespace STM32Step