#include "timer_base.h"
#include "stepper.h"
#include "Config/serial_debug.h"

namespace STM32Step
{
    // Static member initialization
    HardwareTimer *TimerControl::htim = nullptr;
    volatile bool TimerControl::running = false;
    Stepper *TimerControl::currentStepper = nullptr;
    volatile bool TimerControl::positionReached = false;
    volatile TimerControl::MotorState TimerControl::currentState = TimerControl::MotorState::IDLE;
    volatile bool TimerControl::emergencyStop = false;

    // Fixed timer settings
    const uint32_t FIXED_TIMER_FREQ = 40000; // 40kHz base frequency
    const uint32_t PULSE_WIDTH_US = 5;       // 5μs pulse width

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

        // Calculate period for 40kHz in center-aligned mode
        // For 40kHz, we need a complete up/down cycle in 25μs
        // So each up or down count should take 12.5μs
        // At 100MHz, that's 1250 cycles total (625 up + 625 down)
        uint32_t cyclesPerPeriod = timerClock / (FIXED_TIMER_FREQ);

        // Calculate pulse width for 5μs
        // At 100MHz, 5μs = 500 cycles
        uint32_t clocksPerMicro = timerClock / 1000000;
        uint32_t pulseWidth = clocksPerMicro * PULSE_WIDTH_US;

        // Configure timer for fixed 40kHz frequency
        htim->setPrescaleFactor(1); // No prescaler needed at 100MHz
        htim->setOverflow(cyclesPerPeriod);
        htim->setMode(1, TIMER_OUTPUT_COMPARE);

        // Configure timer parameters
        TIM_HandleTypeDef *handle = htim->getHandle();
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // Configure output compare for pulse generation
        TIM_OC_InitTypeDef sConfig = {0};
        sConfig.OCMode = TIM_OCMODE_PWM1;
        sConfig.Pulse = pulseWidth;
        sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfig.OCNPolarity = TIM_OCNPOLARITY_HIGH;
        sConfig.OCFastMode = TIM_OCFAST_DISABLE;
        sConfig.OCIdleState = TIM_OCIDLESTATE_RESET;
        sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;

        if (HAL_TIM_PWM_ConfigChannel(handle, &sConfig, TIM_CHANNEL_1) != HAL_OK)
            return;

        // Enable outputs for TIM1
        handle->Instance->BDTR |= TIM_BDTR_MOE;
        handle->Instance->CCER |= TIM_CCER_CC1E;

        // Attach interrupt callback
        htim->attachInterrupt(onUpdateCallback);

        SerialDebug.print("Timer Config - Fixed Freq: ");
        SerialDebug.print(FIXED_TIMER_FREQ);
        SerialDebug.print(" Hz, Timer Clock: ");
        SerialDebug.print(timerClock / 1000000);
        SerialDebug.print(" MHz, Period: ");
        SerialDebug.print(cyclesPerPeriod);
        SerialDebug.print(", Pulse Width: ");
        SerialDebug.print(pulseWidth);
        SerialDebug.println(" cycles");

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
            HAL_TIM_PWM_Start(htim->getHandle(), TIM_CHANNEL_1);
        }
    }

    void TimerControl::stop()
    {
        if (!running || !htim)
            return;

        htim->pause();
        HAL_TIM_PWM_Stop(htim->getHandle(), TIM_CHANNEL_1);

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
