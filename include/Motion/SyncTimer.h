#pragma once

#include <Arduino.h>
#include "stm32h7xx_hal.h"         // For HAL types if used indirectly by HardwareTimer
#include "Hardware/EncoderTimer.h" // Dependency
#include <STM32Step.h>             // Dependency (STM32Step::Stepper)
#include <HardwareTimer.h>         // For HardwareTimer base class or type

/**
 * @class SyncTimer
 * @brief Manages a hardware timer (e.g., TIM6) to periodically synchronize stepper motor position with encoder input.
 *
 * This class sets up a timer to fire an interrupt at a configurable frequency (`sync_frequency`).
 * The Interrupt Service Routine (ISR), `handleInterrupt()`, reads the encoder, calculates
 * the necessary stepper motor adjustment (including fractional step accumulation),
 * and commands the stepper motor directly. This ISR-based processing ensures precise
 * timing for synchronization, independent of main loop load.
 */
class SyncTimer
{
public:
    /**
     * @struct SyncConfig
     * @brief Configuration parameters for the SyncTimer.
     */
    struct SyncConfig
    {
        double steps_per_encoder_tick; ///< Calculated factor: stepper microsteps per raw encoder tick (double precision)
        uint32_t update_freq;          ///< Desired frequency (Hz) for the synchronization ISR to run.
        bool reverse_direction;        ///< If true, reverses the calculated stepper direction.

        // Default constructor
        SyncConfig() : steps_per_encoder_tick(0.0), // Use 0.0 for double literal
                       update_freq(10000),          // Default to 10kHz, matches typical MotionControl::_config.sync_frequency
                       reverse_direction(false)
        {
        }
    };

    /**
     * @brief Constructor for SyncTimer. Initializes member variables.
     */
    SyncTimer();

    /**
     * @brief Destructor for SyncTimer. Ensures timer resources are released.
     */
    ~SyncTimer();

    // --- Basic Operations ---
    /**
     * @brief Initializes the SyncTimer with dependencies and starts the hardware timer.
     * @param encoder Pointer to the EncoderTimer instance.
     * @param stepper Pointer to the STM32Step::Stepper instance.
     * @return True if initialization is successful, false otherwise.
     */
    bool begin(EncoderTimer *encoder, STM32Step::Stepper *stepper);

    /**
     * @brief Stops the timer and de-initializes resources.
     */
    void end();

    /**
     * @brief Enables or disables the SyncTimer ISR processing.
     * When enabled, resets internal state (_isr_lastEncoderCount, _accumulatedFractionalSteps)
     * and resumes the hardware timer. When disabled, pauses the timer.
     * @param enable True to enable, false to disable.
     */
    void enable(bool enable);

    /**
     * @brief Checks if the SyncTimer is currently enabled.
     * @return True if enabled, false otherwise.
     */
    bool isEnabled() const { return _enabled; }

    // --- Configuration ---
    /**
     * @brief Sets the full synchronization configuration.
     * Applies the new configuration and updates the timer frequency if necessary.
     * @param config The SyncConfig struct with new parameters.
     */
    void setConfig(const SyncConfig &config);

    /**
     * @brief Sets the frequency of the synchronization timer ISR.
     * @param freq The desired frequency in Hz.
     */
    void setSyncFrequency(uint32_t freq);

    // --- Status ---
    /**
     * @brief Checks if an error has occurred during SyncTimer operation or initialization.
     * @return True if an error is flagged, false otherwise.
     */
    bool hasError() const { return _error; }

    /**
     * @brief Gets the timestamp of the last ISR execution.
     * @return Timestamp in milliseconds (from HAL_GetTick()).
     */
    uint32_t getLastUpdateTime() const { return _lastUpdateTime; }

    // --- For Testing/Debugging ---
    /**
     * @brief Gets the currently configured timer frequency.
     * @return The timer frequency in Hz.
     */
    uint32_t getTimerFrequency() const { return _timerFrequency; }

    /**
     * @brief Checks if the SyncTimer has been successfully initialized.
     * @return True if initialized, false otherwise.
     */
    bool isInitialized() const { return _initialized; }

protected:
    /**
     * @brief Hardware timer interrupt handler.
     * This is the core of the SyncTimer. It's called by the timer at `_timerFrequency`.
     * It reads the encoder, calculates required stepper motion (with fractional accumulation),
     * and commands the stepper motor via `_stepper->setRelativePosition()`.
     * This method executes in an ISR context and must be efficient.
     */
    void handleInterrupt();

private:
    HardwareTimer *_timer; ///< Pointer to the hardware timer instance (e.g., TIM6).

    // State variables
    volatile bool _enabled;            ///< True if synchronization is active.
    volatile bool _error;              ///< Error flag.
    volatile uint32_t _lastUpdateTime; ///< Timestamp of the last ISR execution.
    bool _initialized;                 ///< True if begin() has been called successfully.

    // Configuration
    SyncConfig _config;       ///< Current synchronization configuration.
    uint32_t _timerFrequency; ///< Current frequency of the sync timer ISR in Hz.

    // References to other components
    EncoderTimer *_encoder;       ///< Pointer to the EncoderTimer instance.
    STM32Step::Stepper *_stepper; ///< Pointer to the Stepper motor instance.

    // Internal state for ISR processing
    double _accumulatedFractionalSteps; ///< Accumulator for fractional steps between ISR calls (double precision).
    int32_t _isr_lastEncoderCount;      ///< Last encoder count read by the ISR, for calculating delta.

    // Private methods
    /**
     * @brief Initializes the hardware timer (e.g., TIM6).
     * @return True if successful, false otherwise.
     */
    bool initTimer();

    /**
     * @brief Calculates timer prescaler and period for a given frequency.
     * @param freq Desired frequency in Hz.
     * @param[out] prescaler Calculated prescaler value.
     * @param[out] period Calculated auto-reload period value.
     */
    void calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period);

    /**
     * @brief Calculates the synchronization ratio (electronic gearing).
     * (Note: With the refactor, this now returns the pre-calculated steps_per_encoder_tick factor)
     * @return The steps_per_encoder_tick factor.
     */
    double calculateSyncRatio() const; // Changed return type to double

    // Static instance for ISR callback
    static SyncTimer *instance; ///< Singleton instance pointer for ISR context.
};
