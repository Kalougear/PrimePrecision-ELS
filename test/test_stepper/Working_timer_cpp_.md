/**
 * Timer Base Implementation with DMA-driven PWM
 *
 * This implementation provides high-precision PWM control for stepper motors using
 * TIM1 with DMA support on STM32H7. The system operates as follows:
 *
 * Architecture Overview:
 * 1. Timer (TIM1) - Configured in center-aligned PWM mode for symmetric pulses
 * 2. DMA - Automatically updates PWM compare value (CCR1) from memory
 * 3. PWM Output - Generated on PE9 (TIM1_CH1)
 *
 * Why DMA?
 * - DMA (Direct Memory Access) allows automatic updates of PWM settings
 * - CPU is free to handle other tasks while DMA manages PWM
 * - More precise timing as updates happen in hardware
 *
 * Why Center-Aligned Mode?
 * - Produces symmetrical pulses (important for stepper motors)
 * - Reduces electromagnetic noise due to both edges being centered
 * - Better current control in stepper motor windings
 *
 * Timing Calculations:
 * 1. Frequency = Clock / ((prescaler + 1) * (period + 1) * 2)
 *    - The *2 is because center-aligned counts up and down
 * 2. Pulse Width = 5μs (fixed for reliable stepper operation)
 *    - Converted to timer ticks based on clock frequency
 *
 * Example:
 * For 1kHz PWM with 100MHz clock:
 * 1. prescaler = 1 (divide clock by 2)
 * 2. period = 49999 (gives 1kHz in center-aligned mode)
 * 3. pulse = 250 (gives 5μs pulse at 50MHz timer frequency)
 */

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

    /**
     * Calculate optimal timer prescaler for desired frequency
     *
     * The prescaler divides the timer clock to achieve lower frequencies
     * while maintaining resolution. For example:
     * - 100MHz clock with prescaler=1 gives 50MHz timer frequency
     * - 100MHz clock with prescaler=99 gives 1MHz timer frequency
     *
     * @param desiredFreq Target PWM frequency in Hz
     * @return Optimal prescaler value
     */
    uint32_t TimerControl::calculatePrescaler(uint32_t desiredFreq)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        uint32_t prescaler = (timerClock / (desiredFreq * 65536UL)) - 1;
        return prescaler;
    }

    /**
     * Calculate timer period for desired frequency and prescaler
     *
     * The period determines how many timer ticks make up one PWM cycle.
     * In center-aligned mode, the timer counts up to period then down to 0.
     *
     * Example: For 1kHz PWM with 50MHz timer clock (after prescaler):
     * period = 50MHz / (1kHz * 2) - 1 = 24999
     * (the *2 is because we count up then down)
     *
     * @param desiredFreq Target PWM frequency in Hz
     * @param prescaler Current prescaler value
     * @return Timer period value
     */
    uint32_t TimerControl::calculatePeriod(uint32_t desiredFreq, uint32_t prescaler)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        return (timerClock / (desiredFreq * (prescaler + 1)));
    }

    /**
     * Calculate PWM pulse width to maintain 5μs duration
     *
     * Converts 5μs to timer ticks based on the timer frequency.
     * Example: With 50MHz timer frequency (after prescaler):
     * 5μs = 5 * 50 = 250 ticks
     *
     * @param prescaler Current prescaler value
     * @return Pulse width in timer ticks
     */
    uint32_t TimerControl::calculatePulseWidth(uint32_t prescaler)
    {
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();
        uint32_t clocksPerMicro = timerClock / 1000000;
        return (RuntimeConfig::current_pulse_width * clocksPerMicro) / (prescaler + 1);
    }

    /**
     * Initialize DMA for PWM updates
     *
     * DMA is configured to automatically update the timer's CCR1 register,
     * which controls the PWM pulse width. Key settings:
     * - Memory to Peripheral: Transfers data from dmaBuffer to CCR1
     * - Circular mode: Automatically restarts at buffer beginning
     * - FIFO enabled: Buffers transfers for better efficiency
     * - Word size: 32-bit transfers match CCR1 register size
     */
    void TimerControl::setupDMA()
    {
        __HAL_RCC_DMA2_CLK_ENABLE();

        hdma.Instance = DMA2_Stream0;
        hdma.Init.Request = 1; // TIM1_CH1 request
        hdma.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma.Init.MemInc = DMA_MINC_ENABLE;
        hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        hdma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        hdma.Init.Mode = DMA_CIRCULAR;
        hdma.Init.Priority = DMA_PRIORITY_HIGH;
        hdma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
        hdma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
        hdma.Init.MemBurst = DMA_MBURST_SINGLE;
        hdma.Init.PeriphBurst = DMA_PBURST_SINGLE;

        HAL_DMA_Init(&hdma);

        HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }

    /**
     * Initialize timer and PWM system
     *
     * Setup sequence:
     * 1. Configure PE9 pin for PWM output (alternate function)
     * 2. Initialize TIM1 with basic settings
     * 3. Setup DMA for automatic PWM updates
     * 4. Configure initial PWM at 1kHz
     */
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
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Set up initial timer base configuration
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
            return;

        setupDMA();
        configurePWM(1000); // Start at 1kHz
    }

    /**
     * Configure PWM with specified frequency
     *
     * This function sets up the PWM system with:
     * 1. Calculated prescaler and period for the target frequency
     * 2. Fixed 5μs pulse width (converted to timer ticks)
     * 3. Center-aligned PWM mode for symmetric pulses
     * 4. DMA enabled to update CCR1 (pulse width) automatically
     *
     * The process:
     * 1. Calculate timing parameters (prescaler, period, pulse width)
     * 2. Configure timer in center-aligned mode
     * 3. Setup PWM channel
     * 4. Enable DMA updates
     * 5. Start PWM output
     *
     * @param freq Desired PWM frequency in Hz
     */
    void TimerControl::configurePWM(uint32_t freq)
    {
        if (!htim || freq == 0)
            return;

        TIM_HandleTypeDef *handle = htim->getHandle();

        // Stop timer and DMA
        HAL_TIM_PWM_Stop_DMA(handle, TIM_CHANNEL_1);

        // Calculate timer parameters for fixed pulse width
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();

        // For center-aligned mode, frequency is: timerClock / ((prescaler + 1) * (period + 1) * 2)
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
        uint32_t pulseWidth = pulseClocks / prescaler;

        // Update timer parameters
        handle->Init.Prescaler = prescaler - 1;
        handle->Init.Period = period;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
            return;

        // Configure PWM
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

        // Fill DMA buffer with pulse width
        for (uint32_t i = 0; i < DMAConfig::BUFFER_SIZE; i++)
        {
            dmaBuffer[i] = pulseWidth;
        }

        // Link DMA with Timer and enable updates
        __HAL_LINKDMA(handle, hdma[TIM_DMA_ID_CC1], hdma);
        handle->Instance->DIER |= TIM_DMA_CC1;

        // Start PWM with DMA
        HAL_TIM_PWM_Start_DMA(handle, TIM_CHANNEL_1, (uint32_t *)dmaBuffer, DMAConfig::BUFFER_SIZE);
        __HAL_DMA_ENABLE(&hdma);
    }

    /**
     * Start stepper motor operation
     *
     * Initializes motor control and starts PWM at the motor's current speed.
     * The DMA system will automatically maintain PWM output.
     *
     * @param stepper Pointer to stepper motor object
     */
    void TimerControl::start(Stepper *stepper)
    {
        if (!stepper || running)
            return;

        currentStepper = stepper;
        running = true;
        lastUpdateTime = HAL_GetTick();
        configurePWM(stepper->getCurrentSpeed());
    }

    /**
     * Stop stepper motor operation
     *
     * Stops PWM output and DMA transfers. The motor will stop
     * immediately at its current position.
     */
    void TimerControl::stop()
    {
        if (!running || !htim)
            return;

        HAL_TIM_PWM_Stop_DMA(htim->getHandle(), TIM_CHANNEL_1);
        running = false;
        currentStepper = nullptr;
    }

    /**
     * Update timer frequency with safety constraints
     *
     * Changes PWM frequency while ensuring it stays within safe limits.
     * Used during acceleration and deceleration.
     *
     * @param freq New frequency in Hz
     */
    void TimerControl::updateTimerFrequency(uint32_t freq)
    {
        if (!htim)
            return;
        freq = constrainFrequency(freq);
        configurePWM(freq);
    }

    /**
     * Constrain frequency within safe limits
     *
     * Ensures frequency stays between minimum (to maintain torque)
     * and maximum (to prevent missed steps) limits.
     *
     * @param freq Input frequency
     * @return Constrained frequency
     */
    uint32_t TimerControl::constrainFrequency(uint32_t freq)
    {
        if (freq < MotorDefaults::MIN_FREQ)
            return MotorDefaults::MIN_FREQ;
        if (freq > RuntimeConfig::current_max_speed)
            return RuntimeConfig::current_max_speed;
        return freq;
    }

    /**
     * Update stepper position based on direction
     *
     * Called after each step to track motor position.
     * Stops motor when target position is reached.
     */
    void TimerControl::updatePosition()
    {
        if (!currentStepper || !running)
            return;

        if (currentStepper->_currentDirection)
            currentStepper->_currentPosition++;
        else
            currentStepper->_currentPosition--;

        if (checkTargetPosition())
            stop();
    }

    /**
     * Handle speed updates based on acceleration profile
     *
     * Calculates next speed based on acceleration parameters
     * and updates PWM frequency accordingly.
     *
     * @param timeSlice Time elapsed since last update
     */
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

    /**
     * Check if target position is reached
     *
     * @return true if current position equals target position
     */
    bool TimerControl::checkTargetPosition()
    {
        return (currentStepper) ? (currentStepper->getCurrentPosition() == currentStepper->_targetPosition) : false;
    }

} // namespace STM32Step

/**
 * DMA Interrupt Handler
 *
 * System callback for DMA events. Used by HAL library
 * to manage DMA transfers.
 */
extern "C" void DMA2_Stream0_IRQHandler(void)
{
    using namespace STM32Step;
    HAL_DMA_IRQHandler(&TimerControl::hdma);
}
