#include "timer_base.h"
#include "stepper.h"
#include "Config/serial_debug.h"

namespace STM32Step
{
// Static member initialization
HardwareTimer *TimerControl::htim = nullptr;
DMA_HandleTypeDef TimerControl::hdma = {};
LinearAcceleration TimerControl::accelerationProfile;
volatile uint32_t TimerControl::currentAcceleration = RuntimeConfig::current_acceleration;
volatile uint32_t TimerControl::lastUpdateTime = 0;
volatile uint32_t TimerControl::dmaBuffer[DMAConfig::BUFFER_SIZE];
volatile bool TimerControl::running = false;
Stepper *TimerControl::currentStepper = nullptr;
static volatile uint32_t dmaTransferCount = 0; // Track number of DMA transfers

    // DMA transfer complete callback
    void onDMACallback(DMA_HandleTypeDef *hdma)
    {
        dmaTransferCount++;
        if ((dmaTransferCount % 1000) == 0)
        {
            SerialDebug.print("DMA Transfer #");
            SerialDebug.println(dmaTransferCount);
        }

        if (TimerControl::currentStepper && TimerControl::running)
        {
            TimerControl::updatePosition();
        }
    }

    // Calculate optimal timer prescaler for desired frequency
    uint32_t TimerControl::calculatePrescaler(uint32_t desiredFreq)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        uint32_t prescaler = (timerClock / (desiredFreq * 65536UL)) - 1; // For 16-bit timer
        return prescaler;
    }

    // Calculate timer period based on frequency and prescaler
    uint32_t TimerControl::calculatePeriod(uint32_t desiredFreq, uint32_t prescaler)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        return (timerClock / (desiredFreq * (prescaler + 1)));
    }

    // Calculate PWM pulse width based on prescaler
    uint32_t TimerControl::calculatePulseWidth(uint32_t prescaler)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        uint32_t clocksPerMicro = timerClock / 1000000;
        return (RuntimeConfig::current_pulse_width * clocksPerMicro) / (prescaler + 1);
    }

    void TimerControl::printTimerStatus()
    {
        if (!htim)
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();
        SerialDebug.println("\nTimer Status:");
        SerialDebug.print("State: 0x");
        SerialDebug.println(handle->State, HEX);
        SerialDebug.print("CR1: 0x");
        SerialDebug.println(handle->Instance->CR1, HEX);
        SerialDebug.print("CCER: 0x");
        SerialDebug.println(handle->Instance->CCER, HEX);
        SerialDebug.print("BDTR: 0x");
        SerialDebug.println(handle->Instance->BDTR, HEX);
        SerialDebug.print("CCR1: 0x");
        SerialDebug.println(handle->Instance->CCR1, HEX);
        SerialDebug.print("ARR: 0x");
        SerialDebug.println(handle->Instance->ARR, HEX);
        SerialDebug.print("PSC: 0x");
        SerialDebug.println(handle->Instance->PSC, HEX);
    }

    // Configure PWM with specified frequency
    void TimerControl::init()
    {
        SerialDebug.println("TimerControl::init starting...");
        // Configure GPIO for Timer Output
        __HAL_RCC_GPIOE_CLK_ENABLE();
        SerialDebug.println("GPIO clock enabled");

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = (1 << (PinConfig::StepPin::PIN & 0x0F));
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = TimerConfig::GPIO_AF;
        HAL_GPIO_Init(PinConfig::StepPin::PORT, &GPIO_InitStruct);
        SerialDebug.println("GPIO initialized");

        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();
        SerialDebug.println("Timer clock enabled");

        // Initialize Timer
        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            SerialDebug.println("Timer creation failed");
            return;
        }
        SerialDebug.println("Timer created");

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Set up initial timer base configuration
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
        {
            SerialDebug.println("Timer base init failed");
            return;
        }
        SerialDebug.println("Timer base initialized");

        // Now configure PWM for initial 1kHz
        configurePWM(1000);
    }

    void TimerControl::configurePWM(uint32_t freq)
    {
        SerialDebug.print("Configuring PWM for freq: ");
        SerialDebug.println(freq);

        if (!htim || freq == 0)
        {
            SerialDebug.println("Invalid timer or frequency");
            return;
        }

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Stop timer
        HAL_TIM_PWM_Stop(handle, TIM_CHANNEL_1);

        // Calculate timer parameters for fixed pulse width
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        SerialDebug.print("Timer clock: ");
        SerialDebug.println(timerClock);

        // For center-aligned mode, frequency is: timerClock / ((prescaler + 1) * (period + 1) * 2)
        // Solve for period: period = timerClock / (2 * freq * (prescaler + 1)) - 1

        uint32_t prescaler = 1;
        uint32_t period;

        // Find suitable prescaler
        do
        {
            period = (timerClock / (freq * prescaler)) - 1;
            prescaler++;
        } while (period > 0xFFFF); // 16-bit timer limit

        prescaler--; // Correct for last increment

        // Calculate pulse width for 5μs
        uint32_t clocksPerMicro = timerClock / 1000000;
        uint32_t pulseClocks = clocksPerMicro * RuntimeConfig::current_pulse_width;
        uint32_t pulseWidth = pulseClocks / prescaler; // Divide by 2 for center-aligned

        SerialDebug.print("Prescaler: ");
        SerialDebug.println(prescaler - 1);
        SerialDebug.print("Period: ");
        SerialDebug.println(period);
        SerialDebug.print("Pulse Width: ");
        SerialDebug.println(pulseWidth);

        // Actual frequency will be
        float actual_freq = (float)timerClock / (2.0f * prescaler * (period + 1));
        SerialDebug.print("Actual Frequency: ");
        SerialDebug.println(actual_freq);

        // Update timer parameters
        handle->Init.Prescaler = prescaler - 1;
        handle->Init.Period = period;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        HAL_StatusTypeDef status = HAL_TIM_Base_Init(handle);
        if (status != HAL_OK)
        {
            SerialDebug.println("Timer base init failed");
            return;
        }

        // Configure PWM
        TIM_OC_InitTypeDef sConfig = {0};
        sConfig.OCMode = TIM_OCMODE_PWM1;
        sConfig.Pulse = pulseWidth;
        sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfig.OCNPolarity = TIM_OCNPOLARITY_HIGH;
        sConfig.OCFastMode = TIM_OCFAST_DISABLE;
        sConfig.OCIdleState = TIM_OCIDLESTATE_RESET;
        sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;

        status = HAL_TIM_PWM_ConfigChannel(handle, &sConfig, TIM_CHANNEL_1);
        if (status != HAL_OK)
        {
            SerialDebug.print("PWM config failed with status: ");
            SerialDebug.println(status);
            printTimerStatus();
            return;
        }

        // Enable outputs for TIM1
        handle->Instance->BDTR |= TIM_BDTR_MOE;
        handle->Instance->CCER |= TIM_CCER_CC1E;

        // Start PWM
        if (HAL_TIM_PWM_Start(handle, TIM_CHANNEL_1) != HAL_OK)
        {
            SerialDebug.println("PWM start failed");
            return;
        }

        SerialDebug.println("PWM configuration complete");
        printTimerStatus();
    }

    void TimerControl::setupDMA()
    {
        SerialDebug.println("Setting up DMA...");

        // Enable DMA clock using config
        if (DMAConfig::DMA == 2)
        {
            __HAL_RCC_DMA2_CLK_ENABLE();
        }

        // Initialize DMA using config.h settings
        hdma.Instance = (DMAConfig::DMA == 2) ? DMA2_Stream0 : DMA1_Stream0;
        hdma.Init.Request = DMAConfig::REQUEST;
        hdma.Init.Direction = DMAConfig::DIRECTION;
        hdma.Init.PeriphInc = DMAConfig::PERIPH_INC;
        hdma.Init.MemInc = DMAConfig::MEM_INC;
        hdma.Init.PeriphDataAlignment = DMAConfig::PERIPH_DATA_ALIGN;
        hdma.Init.MemDataAlignment = DMAConfig::MEM_DATA_ALIGN;
        hdma.Init.Mode = DMAConfig::MODE;
        hdma.Init.Priority = DMAConfig::PRIORITY;
        hdma.Init.FIFOMode = DMAConfig::FIFO_MODE;

        if (HAL_DMA_Init(&hdma) != HAL_OK)
        {
            SerialDebug.println("DMA Init failed");
            return;
        }

        // Register callback
        hdma.XferCpltCallback = onDMACallback;

        // Enable DMA interrupt
        if (DMAConfig::DMA == 2)
        {
            HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
        }

        SerialDebug.println("DMA setup complete");
    }

    // Start stepper motor operation
    void TimerControl::start(Stepper *stepper)
    {
        if (!stepper || running)
            return;

        currentStepper = stepper;
        running = true;
        lastUpdateTime = HAL_GetTick();
        dmaTransferCount = 0; // Reset transfer counter

        // Configure initial PWM frequency
        configurePWM(stepper->getCurrentSpeed());
    }

    // Stop stepper motor operation
    void TimerControl::stop()
    {
        if (!running || !htim)
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Stop PWM and DMA
        HAL_TIM_PWM_Stop(handle, TIM_CHANNEL_1);
        HAL_DMA_Abort(&hdma);

        SerialDebug.print("Total DMA transfers: ");
        SerialDebug.println(dmaTransferCount);

        running = false;
        currentStepper = nullptr;
    }

    // Update timer frequency with safety constraints
    void TimerControl::updateTimerFrequency(uint32_t freq)
    {
        if (!htim)
            return;
        freq = constrainFrequency(freq);
        configurePWM(freq);
    }

    // Constrain frequency within safe limits
    uint32_t TimerControl::constrainFrequency(uint32_t freq)
    {
        if (freq < MotorDefaults::MIN_FREQ)
            return MotorDefaults::MIN_FREQ;
        if (freq > RuntimeConfig::current_max_speed)
            return RuntimeConfig::current_max_speed;
        return freq;
    }

    // Update stepper position based on direction
    void TimerControl::updatePosition()
    {
        if (!currentStepper || !running)
            return;

        if (currentStepper->_currentDirection)
        {
            currentStepper->_currentPosition++;
        }
        else
        {
            currentStepper->_currentPosition--;
        }

        if (checkTargetPosition())
        {
            stop();
        }
    }

    // Handle speed updates based on acceleration profile
    void TimerControl::handleSpeedUpdate(uint32_t timeSlice)
    {
        if (!currentStepper)
            return;

        uint32_t targetSpeed = currentStepper->getTargetSpeed();
        uint32_t currentSpeed = currentStepper->getCurrentSpeed();
        uint32_t nextSpeed = accelerationProfile.calculateNextSpeed(
            currentSpeed,
            targetSpeed,
            currentAcceleration,
            timeSlice);

        if (nextSpeed != currentSpeed)
        {
            currentStepper->setSpeed(nextSpeed);
            updateTimerFrequency(nextSpeed);
        }
    }

    // Check if target position is reached
    bool TimerControl::checkTargetPosition()
    {
        return (currentStepper) ? (currentStepper->getCurrentPosition() == currentStepper->_targetPosition) : false;
    }

} // namespace STM32Step

// DMA Interrupt Handler
extern "C" void DMA2_Stream0_IRQHandler(void)
{
using namespace STM32Step;
HAL_DMA_IRQHandler(&TimerControl::hdma);
}
