#include "timer_base.h"
#include "stepper.h"
#include "Config/serial_debug.h" // For SystemConfig::Limits::Stepper::PULSE_WIDTH_US, not for printing
#include "Config/SystemConfig.h" // For SystemConfig constants
#include "stm32h7xx_hal.h"
#include "Hardware/SystemClock.h" // For SystemClock::GetInstance().GetPClk2Freq()

namespace STM32Step
{
    // Static member initialization
    HardwareTimer *TimerControl::htim = nullptr;
    volatile bool TimerControl::running = false;
    Stepper *TimerControl::currentStepper = nullptr;
    volatile bool TimerControl::positionReached = false; // Should be managed carefully if used
    volatile TimerControl::MotorState TimerControl::currentState = TimerControl::MotorState::IDLE;
    volatile bool TimerControl::emergencyStop = false;
    uint32_t TimerControl::_configuredPulseWidthUs = 0; // Initialization for the new static member

    // g_isr_call_count and onUpdateCallback are removed as they are for the old interrupt-driven software stepping.

    /**
     * @brief Initializes the hardware timer (TIM1) for PWM-based stepper motor control.
     * Configures TIM1_CH1 (PE9) for PWM output.
     * This method must be called once before any stepper operations.
     */
    void TimerControl::init()
    {
        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();
        // Enable GPIOE clock (for PE9)
        __HAL_RCC_GPIOE_CLK_ENABLE();

        // Configure PE9 for TIM1_CH1 Alternate Function
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = (1 << STM32Step::PinConfig::StepPin::PIN); // Corrected to use PIN
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL; // Or GPIO_PULLDOWN if signal integrity requires it
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1; // AF1 for TIM1_CH1 on PE9
        HAL_GPIO_Init(STM32Step::PinConfig::StepPin::PORT, &GPIO_InitStruct);

        // Initialize Timer object for TIM1
        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init ERROR - Failed to allocate HardwareTimer for TIM1.");
            return;
        }

        TIM_HandleTypeDef *handle = htim->getHandle();
        handle->Instance = TIM1;

        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq(); // APB2 clock for TIM1
        uint32_t pwmFrequency = 1000;                                    // Target 1kHz PWM frequency (1000 steps/sec for initial test)
        uint32_t pulseWidthUs = SystemConfig::Limits::Stepper::PULSE_WIDTH_US;
        if (pulseWidthUs == 0)
            pulseWidthUs = 5; // Default 5us pulse width if not set

        _configuredPulseWidthUs = pulseWidthUs; // Store the configured pulse width

        // Calculate Prescaler and ARR for PWM Frequency
        // ARR = (timerClock / (PSC + 1) / PWM_Frequency) - 1
        // We want to keep PSC as low as possible for better resolution.
        // Max ARR is 65535 for 16-bit TIM1.
        uint32_t prescaler = 0; // Start with no prescaler (PSC register value)
        uint32_t period_cycles = (timerClock / pwmFrequency) - 1;

        if (period_cycles > 0xFFFF)
        {                                         // If period exceeds 16-bit limit, calculate prescaler
            prescaler = (period_cycles / 0xFFFF); // Integer division gives a suitable prescaler factor
            period_cycles = (timerClock / (prescaler + 1) / pwmFrequency) - 1;
        }

        handle->Init.Prescaler = prescaler;
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1; // Reverted to Center-Aligned Mode 1
        handle->Init.Period = period_cycles;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0; // Not used for basic PWM unless using OPM with RCR for ELS bursts
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_PWM_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init ERROR - HAL_TIM_PWM_Init failed.");
            return;
        }

        // Configure PWM Channel 1 (TIM1_CH1 for PE9)
        TIM_OC_InitTypeDef sConfigOC = {0};
        sConfigOC.OCMode = TIM_OCMODE_PWM1; // Output active as long as CNT < CCR1

        // Calculate CCR1 for Pulse Width
        // Pulse_us = (CCR / (timerClock_MHz * (PSC+1)))
        // CCR = Pulse_us * timerClock_MHz * (PSC+1) -> This is wrong
        // CCR = (Pulse_us * (timerClock / (PSC+1))) / 1,000,000
        // CCR = (Pulse_us / 1e6) * (timerClock / (prescaler + 1))
        // CCR = (pulseWidthUs * (timerClock / (prescaler + 1))) / 1000000;
        // Simplified: CCR is a fraction of the Period (ARR).
        // DutyCycle = CCR / (ARR+1)
        // PulseWidth_sec = DutyCycle * Period_sec = (CCR / (ARR+1)) * ( (ARR+1) / PWM_Freq_TimerClock_Adjusted )
        // PulseWidth_sec = CCR / PWM_Freq_TimerClock_Adjusted
        // CCR = PulseWidth_sec * PWM_Freq_TimerClock_Adjusted
        // PWM_Freq_TimerClock_Adjusted = timerClock / (prescaler + 1)
        // CCR = (pulseWidthUs / 1e6) * (timerClock / (prescaler + 1))

        uint32_t ccr_value = static_cast<uint32_t>((static_cast<double>(pulseWidthUs) / 1000000.0) * (static_cast<double>(timerClock) / (prescaler + 1.0)));

        if (ccr_value > period_cycles)
            ccr_value = period_cycles; // CCR cannot exceed Period
        if (ccr_value == 0 && pulseWidthUs > 0)
            ccr_value = 1; // Ensure at least 1 cycle for very short pulses if possible

        sConfigOC.Pulse = ccr_value;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;   // Step pulse is active high
        sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH; // Not used for non-complementary
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET; // Output low when idle
        sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

        if (HAL_TIM_PWM_ConfigChannel(handle, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init ERROR - HAL_TIM_PWM_ConfigChannel for CH1 failed.");
            return;
        }

        // Break-feature and dead-time configuration (Main Output Enable)
        // For TIM1, this is important.
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
        sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
        sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
        sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
        sBreakDeadTimeConfig.DeadTime = 0;
        sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE; // No break input used
        sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
        sBreakDeadTimeConfig.BreakFilter = 0;
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE; // Must be enabled for TIM1/TIM8 PWM outputs
        if (HAL_TIMEx_ConfigBreakDeadTime(handle, &sBreakDeadTimeConfig) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init ERROR - HAL_TIMEx_ConfigBreakDeadTime failed.");
            return;
        }

        // Start the PWM channel first to ensure CCxE bits are set
        if (HAL_TIM_PWM_Start(handle, TIM_CHANNEL_1) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init ERROR - HAL_TIM_PWM_Start (pre-MOE) failed for CH1.");
            return;
        }

        // Attempt to explicitly enable the Main Output Enable (MOE) bit
        __HAL_TIM_MOE_ENABLE(handle);

        // Verify MOE bit is set
        if (READ_BIT(handle->Instance->BDTR, TIM_BDTR_MOE) == 0)
        {
            SerialDebug.println("TimerControl::init WARNING - MOE bit is NOT SET after ConfigBreakDeadTime and explicit enable!");
        }
        else
        {
            SerialDebug.println("TimerControl::init - MOE bit is SET after explicit enable.");
        }

        // Stop the PWM channel immediately after setting MOE; it will be started by TimerControl::start() when needed.
        if (HAL_TIM_PWM_Stop(handle, TIM_CHANNEL_1) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::init WARNING - HAL_TIM_PWM_Stop (post-MOE) failed for CH1.");
            // Continue, as MOE might still be set.
        }

        // The PWM channel is configured but not started yet.
        // HAL_TIM_PWM_Start(handle, TIM_CHANNEL_1) will be called by TimerControl::start().

        SerialDebug.print("TimerControl PWM Init: TimerClock=");
        SerialDebug.print(timerClock / 1000000.0f, 2);
        SerialDebug.print("MHz, PWM_Freq=");
        SerialDebug.print(pwmFrequency);
        SerialDebug.print("Hz, PSC=");
        SerialDebug.print(prescaler);
        SerialDebug.print(", ARR=");
        SerialDebug.print(period_cycles);
        SerialDebug.print(", PulseWidth=");
        SerialDebug.print(pulseWidthUs);
        SerialDebug.print("us, CCR1=");
        SerialDebug.println(ccr_value);

        currentState = MotorState::IDLE;
    }

    /**
     * @brief Starts or resumes the PWM output on TIM1_CH1 for the specified stepper.
     * @param stepper Pointer to the Stepper instance. (Currently not used by PWM start, but kept for consistency).
     * Sets the system to RUNNING state.
     */
    void TimerControl::start(Stepper *stepper)
    {
        if (currentState == MotorState::ERROR || !htim)
        {
            SerialDebug.println("TimerControl::start ERROR - Cannot start, system in error or timer not initialized.");
            return;
        }

        // currentStepper is still relevant if other TimerControl functions need to know which stepper is "active",
        // even if its ISR is not directly called for PWM.
        if (stepper)
        {
            currentStepper = stepper;
        }
        else if (!currentStepper)
        {
            SerialDebug.println("TimerControl::start WARNING - Starting PWM without a specific currentStepper assigned.");
        }

        if (HAL_TIM_PWM_Start(htim->getHandle(), TIM_CHANNEL_1) != HAL_OK)
        {
            currentState = MotorState::ERROR;
            SerialDebug.println("TimerControl::start ERROR - HAL_TIM_PWM_Start failed for CH1.");
            return;
        }

        running = true;
        positionReached = false;
        currentState = MotorState::RUNNING;
        SerialDebug.println("TimerControl::start - PWM CH1 Started.");
        if (htim && htim->getHandle())
        {
            TIM_TypeDef *timInstance = htim->getHandle()->Instance;
            SerialDebug.print("TimerControl::start - TIM1 CCER after HAL_TIM_PWM_Start: 0x");
            SerialDebug.println(timInstance->CCER, HEX);

            // Explicitly enable Channel 1 output
            SET_BIT(timInstance->CCER, TIM_CCER_CC1E);
            // Explicitly set Channel 1 polarity to active high (though HAL_TIM_PWM_ConfigChannel should do this)
            CLEAR_BIT(timInstance->CCER, TIM_CCER_CC1P);
            SerialDebug.print("TimerControl::start - TIM1 CCER after explicit CC1E set & CC1P clear: 0x");
            SerialDebug.println(timInstance->CCER, HEX);
        }
    }

    /**
     * @brief Stops or pauses the PWM output on TIM1_CH1.
     * Sets the system to IDLE state.
     */
    void TimerControl::stop()
    {
        if (currentState == MotorState::ERROR || !htim)
        {
            // SerialDebug.println("TimerControl::stop - System in error or timer not init, no stop action.");
            return;
        }

        if (HAL_TIM_PWM_Stop(htim->getHandle(), TIM_CHANNEL_1) != HAL_OK)
        {
            // Even if stop fails, attempt to update state.
            currentState = MotorState::ERROR; // Or some other state indicating stop failure
            SerialDebug.println("TimerControl::stop WARNING - HAL_TIM_PWM_Stop failed for CH1.");
        }
        else
        {
            SerialDebug.println("TimerControl::stop - PWM CH1 Stopped.");
        }

        running = false;
        positionReached = true;

        if (currentStepper)
        {
            currentStepper->_running = false; // Inform the stepper it's no longer being actively pulsed by this timer.
        }
        currentState = MotorState::IDLE;
    }

    /**
     * @brief Checks if the current stepper has reached its target position.
     * @return True if no stepper is active or the current stepper is at its target.
     * @deprecated This method's utility is limited. Prefer Stepper::getStatus().
     */
    bool TimerControl::checkTargetPosition()
    {
        if (!currentStepper)
            return true; // No active stepper, so "target reached"
        return currentStepper->getCurrentPosition() == currentStepper->getTargetPosition();
    }

    /**
     * @brief Indicates if the target position was considered reached.
     * @return True if positionReached flag is set.
     * @note The `positionReached` flag is set by `stop()`. Its reliability depends on usage context.
     * @deprecated Prefer Stepper::getStatus() for more comprehensive status.
     */
    bool TimerControl::isTargetPositionReached()
    {
        return positionReached;
    }

    /**
     * @brief Handles an emergency stop request.
     * Sets an internal e-stop flag and calls stop() to halt timer operations.
     * The Stepper class itself handles disabling the motor driver.
     */
    void TimerControl::emergencyStopRequest()
    {
        emergencyStop = true; // Set e-stop flag
        stop();               // Stop timer and normal operations
    }

} // namespace STM32Step
