#pragma once

#include <Arduino.h>
#include "stm32h7xx_hal.h"
#include "Hardware/EncoderTimer.h"
#include <STM32Step.h>
#include <HardwareTimer.h>

/**
 * @class SyncTimer
 * @brief Manages a hardware timer (e.g., TIM6) to periodically calculate and command stepper motor motion.
 *
 * This class sets up a timer to fire an interrupt at a configurable frequency.
 * The ISR, `handleInterrupt()`, reads the encoder, calculates the required stepper motor
 * velocity (frequency) and number of steps for the next time interval, and commands the
 * hardware-driven stepper controller.
 */
class SyncTimer
{
public:
    struct SyncConfig
    {
        uint32_t steps_per_encoder_tick_scaled;
        uint32_t scaling_factor;
        uint32_t update_freq;
        bool reverse_direction;

        SyncConfig() : steps_per_encoder_tick_scaled(0),
                       scaling_factor(1),
                       update_freq(10000),
                       reverse_direction(false)
        {
        }
    };

    SyncTimer();
    ~SyncTimer();

    bool begin(EncoderTimer *encoder, STM32Step::Stepper *stepper);
    void end();
    void enable(bool enable);
    bool isEnabled() const { return _enabled; }

    void setConfig(const SyncConfig &config);
    void setSyncFrequency(uint32_t freq);

    bool hasError() const { return _error; }
    uint32_t getLastUpdateTime() const { return _lastUpdateTime; }
    uint32_t getTimerFrequency() const { return _timerFrequency; }
    bool isInitialized() const { return _initialized; }

    // Debugging
    volatile uint32_t _debug_interrupt_count;
    volatile int32_t _debug_last_steps;
    volatile uint32_t _debug_isr_spindle_pos;
    volatile uint32_t _debug_isr_previous_pos;
    void printDebugInfo();

protected:
    /**
     * @brief Hardware timer interrupt handler.
     * This is the core of the SyncTimer. It's called by the timer at `_timerFrequency`.
     * It reads the encoder delta, calculates the number of steps to move in this time slice,
     * determines the required frequency to achieve that move, and commands the stepper
     * using the `moveExact` hardware-accelerated method.
     */
    void handleInterrupt();

private:
    HardwareTimer *_timer;

    volatile bool _enabled;
    volatile bool _error;
    volatile uint32_t _lastUpdateTime;
    bool _initialized;

    SyncConfig _config;
    uint32_t _timerFrequency;

    EncoderTimer *_encoder;
    STM32Step::Stepper *_stepper;

    int64_t _desiredSteps_scaled_accumulated;
    int32_t _isr_lastEncoderCount;
    uint32_t _previousSpindlePosition;

    bool initTimer();
    void calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period);

    static SyncTimer *instance;
};
