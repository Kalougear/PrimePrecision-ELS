#include "stepper.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"
#include "stm32h7xx_hal.h"
#include <algorithm> // For std::abs if used
#include <cmath>     // For fabsf

// Make SerialDebug available
#ifndef SERIAL_DEBUG_INSTANCE_EXTERNED
#define SERIAL_DEBUG_INSTANCE_EXTERNED
extern HardwareSerial SerialDebug;
#endif

namespace STM32Step
{
    Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
        : _stepPin(stepPin),
          _dirPin(dirPin),
          _enablePin(enablePin),
          _currentPosition(0),
          _targetPosition(0),
          _enabled(false),
          _running(false),
          _currentDirection(false),
          _operationMode(OperationMode::IDLE),
          _softIsrState(0),
          _isContinuousMode(false),
          _targetSpeedHz(0.0f),
          _currentSpeedHz(0.0f),
          _accelerationStepsPerS2(1000.0f),
          _nextStepTimeMicros(0),
          _isrAccumulatedSteps(0.0f),
          _lastStepTimeMicros(0)
    {
        initPins();
    }

    Stepper::~Stepper()
    {
        disable();
    }

    void Stepper::setSpeedHz(float frequency_hz)
    {
        _targetSpeedHz = (frequency_hz > 0.0f) ? frequency_hz : 0.0f;

        if (TimerControl::htim && TimerControl::getCurrentState() != TimerControl::MotorState::ERROR)
        {
            TIM_HandleTypeDef *handle = TimerControl::htim->getHandle();
            uint32_t timerClock = SystemClock::GetInstance().GetPClk2Freq();

            if (_targetSpeedHz > 0.01f)
            {
                uint32_t pwmFrequency = static_cast<uint32_t>(_targetSpeedHz);
                if (pwmFrequency == 0)
                    pwmFrequency = 1;

                uint32_t prescaler = handle->Init.Prescaler;
                uint32_t period_cycles = (timerClock / (prescaler + 1) / pwmFrequency) - 1;

                if (period_cycles > 0xFFFF)
                {
                    prescaler = ((timerClock / pwmFrequency) / 0xFFFF);
                    if (prescaler > 0xFFFF)
                        prescaler = 0xFFFF;
                    period_cycles = (timerClock / (prescaler + 1) / pwmFrequency) - 1;
                }
                if (period_cycles > 0xFFFF)
                    period_cycles = 0xFFFF;

                __HAL_TIM_SET_PRESCALER(handle, prescaler);
                __HAL_TIM_SET_AUTORELOAD(handle, period_cycles);

                uint32_t pulseWidthUs = SystemConfig::Limits::Stepper::PULSE_WIDTH_US;
                if (pulseWidthUs == 0)
                    pulseWidthUs = 5;
                uint32_t ccr_value = static_cast<uint32_t>((static_cast<double>(pulseWidthUs) / 1000000.0) * (static_cast<double>(timerClock) / (prescaler + 1.0)));
                if (ccr_value > period_cycles)
                    ccr_value = period_cycles;
                if (ccr_value == 0 && pulseWidthUs > 0)
                    ccr_value = 1;
                __HAL_TIM_SET_COMPARE(handle, TIM_CHANNEL_1, ccr_value);
            }
        }
    }

    void Stepper::setAcceleration(float accel_steps_per_s2)
    {
        _accelerationStepsPerS2 = (accel_steps_per_s2 > 0.0f) ? accel_steps_per_s2 : 1000.0f;
    }

    void Stepper::runContinuous(bool direction)
    {
        startPwm(direction);
    }

    void Stepper::startPwm(bool direction)
    {
        if (!_enabled)
        {
            return;
        }
        if (!_isContinuousMode && _running)
        {
            _running = false;
        }

        _currentDirection = direction;

        bool zAxisInvertDir = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
        bool physicalDirPinState = (_currentDirection) ? (zAxisInvertDir ? LOW : HIGH) : (zAxisInvertDir ? HIGH : LOW);

        if (physicalDirPinState == HIGH)
            GPIO_SET_DIRECTION();
        else
            GPIO_CLEAR_DIRECTION();

        _isContinuousMode = true;

        if (_targetSpeedHz < 0.01f)
        {
            stopPwm();
            return;
        }

        _running = true;
        TimerControl::start(this);
    }

    void Stepper::stopPwm()
    {
        TimerControl::stop();
        _running = false;
        _isContinuousMode = false;
    }

    volatile uint32_t g_stepper_isr_entry_count = 0;

    void Stepper::ISR(void)
    {
        g_stepper_isr_entry_count++;
    }

    void Stepper::setTargetPosition(int32_t position)
    {
        if (_isContinuousMode)
        {
            stopPwm();
        }
        _isContinuousMode = false;
        _targetPosition = position;
        _running = false;
    }

    void Stepper::setRelativePosition(int32_t delta, uint32_t syncTimerPeriodUs)
    {
        if (delta == 0)
        {
            return;
        }
        if (_isContinuousMode)
        {
            stopPwm();
        }
        _isContinuousMode = false;

        executePwmSteps(delta, syncTimerPeriodUs);
    }

    void Stepper::executePwmSteps(int32_t numSteps, uint32_t syncTimerPeriodUs)
    {
        SerialDebug.print("executePwmSteps: Entry. numSteps=");
        SerialDebug.print(numSteps);
        SerialDebug.print(", syncTimerPeriodUs=");
        SerialDebug.print(syncTimerPeriodUs);
        SerialDebug.print(", _enabled=");
        SerialDebug.println(_enabled ? "T" : "F");

        if (!_enabled || numSteps == 0)
        {
            return;
        }

        TIM_HandleTypeDef *htim1 = TimerControl::htim ? TimerControl::htim->getHandle() : nullptr;
        if (!htim1)
        {
            return;
        }

        // Common initial setup
        HAL_TIM_PWM_Stop(htim1, TIM_CHANNEL_1);

        bool directionIsPositive = numSteps > 0;
        _currentDirection = directionIsPositive;

        bool zAxisInvertDir = SystemConfig::RuntimeConfig::Z_Axis::invert_direction;
        bool physicalDirPinState = (_currentDirection) ? (zAxisInvertDir ? LOW : HIGH) : (zAxisInvertDir ? HIGH : LOW);

        if (physicalDirPinState == HIGH)
            GPIO_SET_DIRECTION();
        else
            GPIO_CLEAR_DIRECTION();

        uint32_t stepsToExecuteAbs = static_cast<uint32_t>(std::abs(numSteps));
        uint32_t configuredPulseWidthUs = TimerControl::getConfiguredPulseWidthUs(); // Declare once for the function

        // --- Software Bit-Bang for ELS Single Steps (syncTimerPeriodUs == 0 indicates ELS call) ---
        if (syncTimerPeriodUs == 0 && stepsToExecuteAbs == 1)
        {
            SerialDebug.println("executePwmSteps: Using SOFTWARE BIT-BANG for single ELS step.");
            // configuredPulseWidthUs is already available from above

            // DWT Cycle Counter for precise microsecond delays (must be enabled once at startup in main.cpp)
            // CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
            // DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
            // SystemCoreClockUpdate(); // Ensure SystemCoreClock is up-to-date

            GPIO_SET_STEP();

            uint32_t start_tick = DWT->CYCCNT;
            uint32_t delay_ticks = configuredPulseWidthUs * (SystemCoreClock / 1000000U);
            while ((DWT->CYCCNT - start_tick) < delay_ticks)
                ;

            GPIO_CLEAR_STEP();

            _currentPosition += numSteps; // numSteps is +/-1
            return;                       // IMPORTANT: Bypass OPM logic for bit-banged steps
        }

        // --- OPM Logic (for non-ELS, multi-step, or if syncTimerPeriodUs > 0 for OPM-based ELS) ---
        SerialDebug.println("executePwmSteps: Using OPM logic.");
        uint32_t timerClockHz = SystemClock::GetInstance().GetPClk2Freq();
        uint32_t prescaler_to_set;
        uint32_t period_to_set;
        uint32_t ccr_to_set;
        // configuredPulseWidthUs is already declared and available from the function scope

        if (syncTimerPeriodUs > 0 && stepsToExecuteAbs > 0) // Paced ELS move using OPM (dynamic timing)
        {
            SerialDebug.println("executePwmSteps: Using DYNAMIC PACING logic (Revised).");

            double desired_avg_freq_hz_double = static_cast<double>(stepsToExecuteAbs) * 1000000.0 / static_cast<double>(syncTimerPeriodUs);
            uint32_t desired_avg_freq_hz = static_cast<uint32_t>(desired_avg_freq_hz_double);
            if (desired_avg_freq_hz == 0)
                desired_avg_freq_hz = 1;

            uint32_t max_system_freq_hz = SystemConfig::Limits::Stepper::MAX_STEP_FREQ_HZ;
            if (max_system_freq_hz == 0)
                max_system_freq_hz = 20000;

            uint32_t freq_limit_from_pulse_width = max_system_freq_hz;
            if (configuredPulseWidthUs > 0)
            {
                uint32_t temp_freq_limit = 1000000U / (configuredPulseWidthUs * 2);
                if (temp_freq_limit == 0)
                    temp_freq_limit = 1;
                freq_limit_from_pulse_width = temp_freq_limit;
            }

            uint32_t actual_opm_burst_freq_hz = desired_avg_freq_hz;
            if (actual_opm_burst_freq_hz > max_system_freq_hz)
            {
                actual_opm_burst_freq_hz = max_system_freq_hz;
            }
            if (actual_opm_burst_freq_hz > freq_limit_from_pulse_width)
            {
                actual_opm_burst_freq_hz = freq_limit_from_pulse_width;
            }
            if (actual_opm_burst_freq_hz == 0)
                actual_opm_burst_freq_hz = 1;

            SerialDebug.print("executePwmSteps OPM Dynamic Timing: syncTimerPeriodUs=");
            SerialDebug.print(syncTimerPeriodUs);
            SerialDebug.print(", stepsToExecute=");
            SerialDebug.print(stepsToExecuteAbs); // Corrected
            SerialDebug.print(", desired_avg_freq_hz=");
            SerialDebug.print(desired_avg_freq_hz);
            SerialDebug.print(", max_system_freq_hz=");
            SerialDebug.print(max_system_freq_hz);
            SerialDebug.print(", freq_limit_from_pulse_width=");
            SerialDebug.print(freq_limit_from_pulse_width);
            SerialDebug.print(", actual_opm_burst_freq_hz=");
            SerialDebug.println(actual_opm_burst_freq_hz);

            uint32_t totalDivisor = timerClockHz / actual_opm_burst_freq_hz;
            if (totalDivisor == 0)
                totalDivisor = 1;

            prescaler_to_set = 0;
            period_to_set = totalDivisor - 1;

            if (period_to_set > 0xFFFF)
            {
                prescaler_to_set = (totalDivisor / (0xFFFF + 1));
                if (prescaler_to_set > 0xFFFF)
                    prescaler_to_set = 0xFFFF;
                period_to_set = (totalDivisor / (prescaler_to_set + 1));
                if (period_to_set > 0)
                    period_to_set -= 1;
            }
            if (period_to_set > 0xFFFF)
                period_to_set = 0xFFFF;
            if (period_to_set == 0 && actual_opm_burst_freq_hz > 0)
                period_to_set = 1;

            ccr_to_set = static_cast<uint32_t>((static_cast<double>(configuredPulseWidthUs) / 1000000.0) * (static_cast<double>(timerClockHz) / (static_cast<double>(prescaler_to_set) + 1.0)));
        }
        else // Non-paced move (syncTimerPeriodUs == 0, but stepsToExecuteAbs > 1), use fixed MAX_STEP_FREQ_HZ
        {
            SerialDebug.println("executePwmSteps: Using FIXED RATE BURST logic (for multi-step or non-ELS OPM).");
            uint32_t pulseBurstFreqFixed = SystemConfig::Limits::Stepper::MAX_STEP_FREQ_HZ;
            if (pulseBurstFreqFixed == 0)
                pulseBurstFreqFixed = 20000;

            prescaler_to_set = htim1->Init.Prescaler;
            period_to_set = (timerClockHz / (prescaler_to_set + 1) / pulseBurstFreqFixed);
            if (period_to_set > 0)
                period_to_set -= 1;

            if (period_to_set > 0xFFFF || period_to_set == 0)
            {
                prescaler_to_set = (timerClockHz / pulseBurstFreqFixed / (0xFFFF + 1));
                if (prescaler_to_set > 0xFFFF)
                    prescaler_to_set = 0xFFFF;

                period_to_set = (timerClockHz / (prescaler_to_set + 1) / pulseBurstFreqFixed);
                if (period_to_set > 0)
                    period_to_set -= 1;
            }
            if (period_to_set > 0xFFFF)
                period_to_set = 0xFFFF;
            if (period_to_set == 0 && pulseBurstFreqFixed > 0)
                period_to_set = 1;

            ccr_to_set = static_cast<uint32_t>((static_cast<double>(configuredPulseWidthUs) / 1000000.0) * (static_cast<double>(timerClockHz) / (static_cast<double>(prescaler_to_set) + 1.0)));
        }

        // Common CCR clamping logic for OPM
        if (ccr_to_set >= period_to_set && period_to_set > 0)
            ccr_to_set = period_to_set - 1;
        else if (ccr_to_set == 0 && configuredPulseWidthUs > 0)
            ccr_to_set = 1;
        if (ccr_to_set > period_to_set && period_to_set == 0)
            ccr_to_set = 0;
        else if (ccr_to_set > period_to_set)
            ccr_to_set = period_to_set;

        __HAL_TIM_SET_PRESCALER(htim1, prescaler_to_set);
        __HAL_TIM_SET_AUTORELOAD(htim1, period_to_set);
        __HAL_TIM_SET_COMPARE(htim1, TIM_CHANNEL_1, ccr_to_set);

        uint32_t rcr_val = (stepsToExecuteAbs > 0) ? ((stepsToExecuteAbs > 0xFFFF + 1) ? 0xFFFF : (stepsToExecuteAbs - 1)) : 0;
        htim1->Instance->RCR = rcr_val;

        SerialDebug.print("executePwmSteps: Commanding OPM: stepsToExecute=");
        SerialDebug.print(stepsToExecuteAbs);
        SerialDebug.print(", RCR_val_set=");
        SerialDebug.println(rcr_val);

        SerialDebug.print("executePwmSteps OPM Diag: PSC=");
        SerialDebug.print(prescaler_to_set);
        SerialDebug.print(", ARR=");
        SerialDebug.print(period_to_set);
        SerialDebug.print(", CCR1=");
        SerialDebug.print(ccr_to_set);
        SerialDebug.print(", RCR=");
        SerialDebug.println(rcr_val);
        SerialDebug.print("executePwmSteps OPM Diag: ConfiguredPulseWidthUs=");
        SerialDebug.println(configuredPulseWidthUs);

        _running = true;

        SerialDebug.print("executePwmSteps OPM Diag: Before UG: CR1=0x");
        SerialDebug.print(htim1->Instance->CR1, HEX);
        SerialDebug.print(", CCER=0x");
        SerialDebug.print(htim1->Instance->CCER, HEX);
        SerialDebug.print(", CNT=");
        SerialDebug.println(htim1->Instance->CNT);

        __HAL_TIM_SET_COUNTER(htim1, 0);
        htim1->Instance->EGR = TIM_EGR_UG;

        SerialDebug.print("executePwmSteps OPM Diag: After UG, Before OPM Clear: CR1=0x");
        SerialDebug.print(htim1->Instance->CR1, HEX);
        SerialDebug.print(", CCER=0x");
        SerialDebug.print(htim1->Instance->CCER, HEX);
        SerialDebug.print(", CNT=");
        SerialDebug.println(htim1->Instance->CNT);

        htim1->Instance->CR1 &= ~TIM_CR1_OPM;
        SerialDebug.print("executePwmSteps OPM Diag: After OPM Clear: CR1=0x");
        SerialDebug.println(htim1->Instance->CR1, HEX);

        HAL_StatusTypeDef status = HAL_TIM_PWM_Start(htim1, TIM_CHANNEL_1);
        SerialDebug.print("executePwmSteps OPM Diag: HAL_TIM_PWM_Start status: ");
        SerialDebug.println(status == HAL_OK ? "OK" : "FAIL");
        SerialDebug.print("executePwmSteps OPM Diag: After PWM_Start: CR1=0x");
        SerialDebug.print(htim1->Instance->CR1, HEX);
        SerialDebug.print(", CCER=0x");
        SerialDebug.println(htim1->Instance->CCER, HEX);

        htim1->Instance->CR1 |= TIM_CR1_OPM;
        SerialDebug.print("executePwmSteps OPM Diag: After OPM Set: CR1=0x");
        SerialDebug.println(htim1->Instance->CR1, HEX);

        _currentPosition += numSteps; // numSteps contains the sign
    }

    void Stepper::stop()
    {
        stopPwm();
    }

    void Stepper::emergencyStop()
    {
        _isContinuousMode = false;
        _currentSpeedHz = 0.0f;
        _targetSpeedHz = 0.0f;
        stop();
        disable();
        TimerControl::emergencyStopRequest();
    }

    void Stepper::enable()
    {
        if (_enabled)
            return;
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false;
        _softIsrState = 0;
        SerialDebug.print("Stepper::enable() - Pin E");
        SerialDebug.print(_enablePin);
        SerialDebug.print(" set to: ");
        SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? "HIGH (for active high)" : "LOW (for active low)");
    }

    void Stepper::disable()
    {
        if (!_enabled)
            return;
        stop();
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
        _enabled = false;
        SerialDebug.print("Stepper::disable() - Pin E");
        SerialDebug.print(_enablePin);
        SerialDebug.print(" set to: ");
        SerialDebug.println(SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? "LOW (for active high)" : "HIGH (for active low)");
    }

    void Stepper::setMicrosteps(uint32_t microsteps)
    {
        static const uint32_t validMicrosteps[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
        bool isValid = false;
        for (uint32_t valid : validMicrosteps)
        {
            if (microsteps == valid)
            {
                isValid = true;
                break;
            }
        }
        if (!isValid)
        {
            return;
        }
        SystemConfig::RuntimeConfig::Stepper::microsteps = microsteps;
    }

    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        GPIO_InitStruct.Pin = 1 << (_dirPin & 0x0F);
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = 1 << (_enablePin & 0x0F);
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, 1 << (_enablePin & 0x0F),
                          SystemConfig::RuntimeConfig::Z_Axis::enable_polarity_active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    void Stepper::GPIO_SET_STEP() { HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_SET); }
    void Stepper::GPIO_CLEAR_STEP() { HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_RESET); }
    void Stepper::GPIO_SET_DIRECTION() { HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_SET); }
    void Stepper::GPIO_CLEAR_DIRECTION() { HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET); }

    StepperStatus Stepper::getStatus() const
    {
        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        status.stepsRemaining = _targetPosition - _currentPosition;
        return status;
    }

    void Stepper::incrementCurrentPosition(int32_t increment)
    {
        _currentPosition += increment;
        _targetPosition += increment;
    }

} // namespace STM32Step
