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

    /**
     * @brief Callback function executed by the timer interrupt.
     * Calls the ISR of the current stepper if the system is in a running state.
     */
    // Add a global ISR counter for debugging
    volatile uint32_t g_isr_call_count = 0;

    void onUpdateCallback()
    {
        g_isr_call_count++; // Increment on every timer interrupt

        if (TimerControl::getCurrentState() == TimerControl::MotorState::RUNNING &&
            TimerControl::currentStepper != nullptr)
        {
            // SerialDebug.println("TCB: onUpdateCallback -> Calling Stepper ISR"); // Too frequent for serial
            TimerControl::currentStepper->ISR();
        }
        // else {
        // SerialDebug.println("TCB: onUpdateCallback - NOT Calling Stepper ISR"); // Too frequent
        // }
    }

    /**
     * @brief Initializes the hardware timer (TIM1) for stepper motor control.
     * This function configures TIM1 to generate interrupts at a frequency suitable
     * for calling the Stepper::ISR() to produce step pulses.
     * It calculates the timer period based on the system clock and desired pulse characteristics.
     * This method must be called once before any stepper operations.
     */
    void TimerControl::init()
    {
        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();

        // Initialize Timer object for TIM1
        htim = new HardwareTimer(TIM1);
        if (!htim)
        {
            currentState = MotorState::ERROR; // Failed to allocate timer object
            return;
        }

        // Get timer clock frequency (APB2 clock for TIM1 on STM32H7, typically 100MHz or 200MHz based on SystemClock config)
        uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();

        // Calculate base ISR frequency.
        // The ISR needs to be called twice per step pulse (once for HIGH, once for LOW).
        // minCycleTime represents the period of one full step pulse (HIGH + LOW duration).
        // SystemConfig::Limits::Stepper::PULSE_WIDTH_US defines the duration of the HIGH part of the pulse.
        // Assuming symmetrical pulse, LOW part is also PULSE_WIDTH_US.
        uint32_t minCycleTime = SystemConfig::Limits::Stepper::PULSE_WIDTH_US * 2; // Total period for one step pulse in microseconds
        if (minCycleTime == 0)
            minCycleTime = 10; // Avoid division by zero, default to 10us (100kHz ISR / 50kHz step rate)

        uint32_t baseFreq = 1000000 / minCycleTime; // ISR frequency in Hz (e.g., 100kHz for 5us PULSE_WIDTH_US)

        // Calculate timer auto-reload period (ARR value)
        // cyclesPerPeriod = TimerInputClock / DesiredISR vertexList
        uint32_t cyclesPerPeriod = timerClock / baseFreq;

        // Configure timer prescaler and period (overflow value)
        // For STM32H7, TIM1 is a 16-bit counter, max ARR is 65535.
        // Assuming no prescaler (PSC=0, so prescaler factor is 1) if cyclesPerPeriod fits.
        // If cyclesPerPeriod is too large, a prescaler would be needed, but current values
        // (e.g., 100MHz clock, 100kHz ISR_Freq -> period = 1000) fit well within 16-bit.
        htim->setPrescaleFactor(1);         // Prescaler register will be 0
        htim->setOverflow(cyclesPerPeriod); // Auto-Reload Register (ARR)

        // Set timer mode. TIMER_OUTPUT_COMPARE might not be strictly necessary if only using update interrupt
        // for bit-banging, but it's a common setup. Channel 1 is specified but not actively used for PWM output here.
        htim->setMode(1, TIMER_OUTPUT_COMPARE);

        // Configure advanced timer parameters via HAL
        TIM_HandleTypeDef *handle = htim->getHandle();
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0;                             // Not used for basic timers or software stepping
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // Enable ARR buffering

        if (HAL_TIM_Base_Init(handle) != HAL_OK)
        {
            currentState = MotorState::ERROR; // HAL initialization failed
            return;
        }

        // Attach the interrupt callback function to the timer's update event
        htim->attachInterrupt(onUpdateCallback);

        // Optional: Debug prints for verification (kept commented for production)
        /*
        SerialDebug.print("TimerControl Init - ISR Freq: "); SerialDebug.print(baseFreq);
        SerialDebug.print(" Hz, Timer Clock: "); SerialDebug.print(timerClock / 1000000);
        SerialDebug.print(" MHz, ARR: "); SerialDebug.print(cyclesPerPeriod);
        SerialDebug.print(", Pulse Width (High): "); SerialDebug.print(SystemConfig::Limits::Stepper::PULSE_WIDTH_US);
        SerialDebug.println(" us");
        */

        currentState = MotorState::IDLE; // Successfully initialized
    }

    /**
     * @brief Starts or resumes the timer to generate ISR calls for the specified stepper.
     * @param stepper Pointer to the Stepper instance that will be serviced by the ISR.
     * Sets the system to RUNNING state if currently IDLE.
     */
    void TimerControl::start(Stepper *stepper)
    {
        // Only start if a valid stepper is provided and the system is IDLE.
        // If already RUNNING, this call might be to switch steppers, but current logic
        // assumes one stepper is controlled at a time by this static TimerControl.
        if (!stepper || (currentState != MotorState::IDLE && currentStepper == stepper))
        {
            // If already running with the same stepper, no action needed.
            // If trying to start with no stepper, or in ERROR state, do nothing.
            return;
        }

        // If switching steppers while running, or starting from non-IDLE, could be an issue.
        // For simplicity, this function primarily handles starting from IDLE or resuming for the same stepper.
        // If currentState is RUNNING but stepper is different, it implies a context switch not fully handled here.

        currentStepper = stepper;
        running = true;          // TimerControl's flag indicating it's active
        positionReached = false; // Reset position reached flag
        currentState = MotorState::RUNNING;

        // Resume the timer (starts it if it was paused or not yet started)
        if (htim)
        {
            htim->resume();
        }
        // else: htim should always be valid after init(), error if not.
    }

    /**
     * @brief Stops or pauses the timer, ceasing ISR calls.
     * Sets the system to IDLE state and updates the associated stepper's running status.
     */
    void TimerControl::stop()
    {
        if (!running || !htim) // Only act if timer is running and valid
            return;

        htim->pause(); // Pause the hardware timer

        running = false;        // TimerControl is no longer actively driving a stepper
        positionReached = true; // Assume target is reached or motion is intentionally stopped

        if (currentStepper)
        {
            currentStepper->_running = false; // Inform the stepper it's no longer being actively pulsed
        }

        // currentStepper = nullptr; // Keep currentStepper for potential resume, or clear if stop is definitive.
        // Clearing here means start() must always be called with a stepper.
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
