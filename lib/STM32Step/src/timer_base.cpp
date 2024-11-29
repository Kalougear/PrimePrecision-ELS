#include "timer_base.h"
#include "stepper.h"
#include "Config/serial_debug.h"

namespace STM32Step
{
    // Static member initialization
    HardwareTimer *TimerControl::htim = nullptr;
    volatile uint32_t TimerControl::lastUpdateTime = 0;
    volatile bool TimerControl::running = false;
    Stepper *TimerControl::currentStepper = nullptr;
    volatile bool TimerControl::positionReached = false;
    volatile int32_t TimerControl::stepsRemaining = 0;
    volatile TimerControl::MotorState TimerControl::currentState = TimerControl::MotorState::IDLE;
    volatile bool TimerControl::emergencyStop = false;

    void onUpdateCallback()
    {
        if (TimerControl::getCurrentState() == TimerControl::MotorState::RUNNING)
        {
            TimerControl::updatePosition();
        }
    }

    void TimerControl::init()
    {
        // Configure GPIO for Timer Output
        __HAL_RCC_GPIOE_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = (1 << (PinConfig::StepPin::PIN & 0x0F));
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = TimerConfig::GPIO_AF;
        HAL_GPIO_Init(PinConfig::StepPin::PORT, &GPIO_InitStruct);

        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();

        // Initialize Timer
        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            currentState = MotorState::ERROR;
            return;
        }

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Set up initial timer base configuration
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // Attach interrupt callback
        htim->attachInterrupt(onUpdateCallback);
        currentState = MotorState::IDLE;
    }

    void TimerControl::configurePWM(uint32_t freq)
    {
        if (!htim || freq == 0)
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Stop timer
        HAL_TIM_PWM_Stop(handle, TIM_CHANNEL_1);

        // Get timer clock frequency
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();

        // For center-aligned mode, multiply freq by 2 since we count up and down
        uint32_t targetFreq = freq * 2;

        // Calculate timer parameters
        uint32_t prescaler = 1;
        uint32_t period;

        // Find suitable prescaler while keeping period within 16-bit range
        do
        {
            period = (timerClock / (targetFreq * prescaler)) - 1;
            prescaler++;
        } while (period > 0xFFFF);

        prescaler--; // Correct for last increment

        // Calculate pulse width (5μs fixed width)
        uint32_t clocksPerMicro = timerClock / 1000000;
        uint32_t pulseClocks = clocksPerMicro * RuntimeConfig::current_pulse_width;
        uint32_t pulseWidth = pulseClocks / prescaler;

        // Configure timer parameters
        handle->Init.Prescaler = prescaler - 1;
        handle->Init.Period = period;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
            return;

        // Configure PWM channel
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

        // Start PWM
        HAL_TIM_PWM_Start(handle, TIM_CHANNEL_1);
    }

    void TimerControl::start(Stepper *stepper)
    {
        if (!stepper || currentState != MotorState::IDLE)
            return;

        currentStepper = stepper;
        running = true;
        positionReached = false;
        stepsRemaining = abs(stepper->_targetPosition - stepper->_currentPosition);
        currentState = MotorState::RUNNING;

        uint32_t startFreq = stepper->getCurrentSpeed();
        if (startFreq == 0 || startFreq < MotorDefaults::MIN_FREQ)
        {
            startFreq = MotorDefaults::MIN_FREQ;
        }

        lastUpdateTime = HAL_GetTick();
        configurePWM(startFreq);
    }

    void TimerControl::stop()
    {
        if (!running || !htim)
            return;

        HAL_TIM_PWM_Stop(htim->getHandle(), TIM_CHANNEL_1);

        running = false;
        positionReached = true;

        if (currentStepper)
        {
            currentStepper->_running = false;
            currentStepper->_currentSpeed = 0;
        }

        currentStepper = nullptr;
        currentState = MotorState::IDLE;
    }

    void TimerControl::updatePosition()
    {
        if (!running || !currentStepper)
            return;

        // Get timer handle
        TIM_HandleTypeDef *handle = htim->getHandle();

        // Only update on up-counting in center-aligned mode
        if ((handle->Instance->CR1 & TIM_CR1_DIR) == 0)
        {
            // Update position based on direction
            if (currentStepper->_currentDirection)
                currentStepper->_currentPosition++;
            else
                currentStepper->_currentPosition--;

            // Check if we've reached the target position
            if (currentStepper->_currentPosition == currentStepper->_targetPosition)
            {
                stop();
                return;
            }

            // Update remaining steps
            if (stepsRemaining > 0)
            {
                stepsRemaining--;
                if (stepsRemaining == 0)
                {
                    stop();
                }
            }
        }
    }

    void TimerControl::updateTimerFrequency(uint32_t freq)
    {
        if (currentState == MotorState::RUNNING)
        {
            configurePWM(constrainFrequency(freq));
        }
    }

    uint32_t TimerControl::constrainFrequency(uint32_t freq)
    {
        if (freq < MotorDefaults::MIN_FREQ)
            return MotorDefaults::MIN_FREQ;
        if (freq > RuntimeConfig::current_max_speed)
            return RuntimeConfig::current_max_speed;
        return freq;
    }

    bool TimerControl::checkTargetPosition()
    {
        if (!currentStepper)
            return true;
        return currentStepper->_currentPosition == currentStepper->_targetPosition;
    }

    bool TimerControl::isTargetPositionReached()
    {
        return positionReached;
    }

    int32_t TimerControl::getRemainingSteps()
    {
        return stepsRemaining;
    }

    void TimerControl::emergencyStopRequest()
    {
        emergencyStop = true;
        stop();
    }

} // namespace STM32Step
