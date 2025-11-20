#include "timer_base.h"
#include "stepper.h"
#include "Config/SystemConfig.h"
#include "Config/serial_debug.h"
#include "stm32h7xx_hal.h"
#include "Hardware/SystemClock.h"

namespace STM32Step
{

    // Define the static interrupt handler
    void TimerControl::pulse_isr()
    {
        if (currentStepper != nullptr)
        {
            currentStepper->onMoveComplete();
        }
        stop();
    }
    // Static member initialization
    HardwareTimer *TimerControl::htim = nullptr;
    Stepper *TimerControl::currentStepper = nullptr;
    volatile TimerControl::MotorState TimerControl::currentState = TimerControl::MotorState::IDLE;
    volatile bool TimerControl::emergencyStop = false;

    void TimerControl::init()
    {
        if (htim)
            return; // Already initialized

        // This assumes the step pin is defined in SystemConfig and is compatible with TIM1_CH1
        // A more robust implementation would pass the pin from the Stepper object.
        initGPIO_PWM();

        __HAL_RCC_TIM1_CLK_ENABLE();

        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            currentState = MotorState::ERROR;
            return;
        }

        TIM_HandleTypeDef *handle = htim->getHandle();

        // --- Start of Known-Good Configuration from Dry Test ---

        // 4. Configure TIM1 Base
        // PCLK2 for TIM1 is expected to be high. Let's assume 200MHz for this calculation.
        // Target timer clock: 10 MHz. Prescaler = (200MHz / 10MHz) - 1 = 19
        handle->Init.Prescaler = 19;
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.Period = 9999; // Default to 1kHz, will be overwritten by setFrequency
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
        if (HAL_TIM_PWM_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // 5. Configure PWM Channel
        TIM_OC_InitTypeDef ocConfig = {0};
        ocConfig.OCMode = TIM_OCMODE_PWM1;
        ocConfig.Pulse = 5000; // Default to 50% duty, will be overwritten
        ocConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
        ocConfig.OCNPolarity = TIM_OCNPOLARITY_HIGH;
        ocConfig.OCFastMode = TIM_OCFAST_DISABLE;
        ocConfig.OCIdleState = TIM_OCIDLESTATE_RESET;
        ocConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
        if (HAL_TIM_PWM_ConfigChannel(handle, &ocConfig, TIM_CHANNEL_1) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // 6. Configure Break and Dead Time (CRITICAL for TIM1)
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE; // Enable AOE
        if (HAL_TIMEx_ConfigBreakDeadTime(handle, &sBreakDeadTimeConfig) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // 7. Configure TIM1 Master Mode Selection to Trigger TIM5
        TIM_MasterConfigTypeDef sMasterConfig = {0};
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE; // Generate TRGO on Update Event (Counter Overflow)
        sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(handle, &sMasterConfig) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // --- End of Known-Good Configuration ---

        // Attach the interrupt using the Arduino framework method for move completion.
        htim->attachInterrupt(TimerControl::pulse_isr);

        // Do NOT manually enable UIE here. Let setPulseCount manage it.
        // handle->Instance->DIER |= TIM_DIER_UIE;

        // --- Configure TIM5 as Slave Counter ---
        __HAL_RCC_TIM5_CLK_ENABLE();

        // TIM5 Handle (Local, as we only need it for init and simple reading)
        TIM_HandleTypeDef htim5 = {0};
        htim5.Instance = TIM5;
        htim5.Init.Prescaler = 0;
        htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim5.Init.Period = 0xFFFFFFFF; // Max 32-bit count
        htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        TIM_SlaveConfigTypeDef sSlaveConfig = {0};
        sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1; // Count on TRGO rising edge
        sSlaveConfig.InputTrigger = TIM_TS_ITR0;          // ITR0 connects TIM1 to TIM5 on STM32H7
        sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING;
        sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
        sSlaveConfig.TriggerFilter = 0;
        if (HAL_TIM_SlaveConfigSynchro(&htim5, &sSlaveConfig) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        // Start TIM5
        if (HAL_TIM_Base_Start(&htim5) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            return;
        }

        currentState = MotorState::IDLE;
    }

    uint32_t TimerControl::getPulseCount()
    {
        // Direct register access to TIM5 counter.
        // TIM5 is configured as a slave to TIM1, so it counts pulses generated by TIM1.
        return TIM5->CNT;
    }

    void TimerControl::initGPIO_PWM()
    {
        // Configure the step pin using the centralized config.h definitions.
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        __HAL_RCC_GPIOE_CLK_ENABLE(); // Assuming Port E for TIM1

        GPIO_InitStruct.Pin = PinConfig::StepPin::PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = TimerConfig::GPIO_AF;
        HAL_GPIO_Init(PinConfig::StepPin::PORT, &GPIO_InitStruct);
    }

    void TimerControl::setFrequency(uint32_t frequency_hz)
    {
        if (!htim)
            return;

        if (frequency_hz == 0)
        {
            // If frequency is 0, we can stop the timer channel, but not the whole timer base.
            // The accel_isr will call start() again when speed ramps up.
            HAL_TIM_PWM_Stop(htim->getHandle(), TIM_CHANNEL_1);
            return;
        }

        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq() / (htim->getHandle()->Init.Prescaler + 1);
        uint32_t arr = (timerClock / frequency_hz) - 1;

        if (arr > 0xFFFF)
            arr = 0xFFFF; // Clamp to 16-bit max

        __HAL_TIM_SET_AUTORELOAD(htim->getHandle(), arr);
        __HAL_TIM_SET_COMPARE(htim->getHandle(), TIM_CHANNEL_1, arr / 2); // 50% duty cycle
    }

    void TimerControl::setPulseCount(uint32_t pulses)
    {
        if (!htim)
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();

        if (pulses > 0)
        {
            // Set repetition counter for finite moves
            handle->Instance->RCR = pulses - 1;
            // Enable Update Interrupt to catch completion
            __HAL_TIM_ENABLE_IT(handle, TIM_IT_UPDATE);
        }
        else
        {
            // Continuous mode (infinite pulses)
            // RCR doesn't matter much here for infinite, but set to 0
            handle->Instance->RCR = 0;
            // Disable Update Interrupt so it doesn't stop
            __HAL_TIM_DISABLE_IT(handle, TIM_IT_UPDATE);
        }
    }

    void TimerControl::start(Stepper *stepper)
    {
        if (!stepper || !htim)
            return;

        currentStepper = stepper;
        currentState = MotorState::RUNNING;

        // For advanced timers like TIM1, the main output must be explicitly enabled.
        __HAL_TIM_MOE_ENABLE(htim->getHandle());
        HAL_TIM_PWM_Start(htim->getHandle(), TIM_CHANNEL_1);
    }

    void TimerControl::stop()
    {
        if (!htim || currentState == MotorState::IDLE)
            return;

        HAL_TIM_PWM_Stop(htim->getHandle(), TIM_CHANNEL_1);

        if (currentStepper)
        {
            currentStepper->_running = false;
        }
        currentState = MotorState::IDLE;
    }

    void TimerControl::emergencyStopRequest()
    {
        emergencyStop = true;
        stop();
    }

} // namespace STM32Step
