#pragma once

#include <Arduino.h>
#include <HardwareTimer.h> // For TIM_HandleTypeDef if not fully encapsulated
#include "stm32h7xx_hal.h" // For HAL types like TIM_HandleTypeDef, GPIO_InitTypeDef etc.

/**
 * @class EncoderTimer
 * @brief Manages a hardware timer (TIM2) in encoder interface mode to track spindle position and speed.
 *
 * This class configures TIM2 to decode quadrature encoder signals (A/B channels)
 * to provide a continuous count of encoder pulses. It also calculates RPM based on
 * changes in this count over time.
 */
class EncoderTimer
{
public:
    /**
     * @struct Position
     * @brief Holds comprehensive position and speed data from the encoder.
     */
    struct Position
    {
        int32_t count;      ///< Current encoder count (quadrature).
        uint32_t timestamp; ///< Timestamp (ms) when this data was captured.
        int16_t rpm;        ///< Calculated Revolutions Per Minute.
        bool direction;     ///< Current direction of rotation (true if timer is counting down, false if up).
        bool valid;         ///< True if the data in this struct is considered valid.
    };

    /**
     * @brief Constructor for EncoderTimer.
     * Initializes state variables.
     */
    EncoderTimer();

    /**
     * @brief Destructor for EncoderTimer.
     * Ensures the timer is properly de-initialized.
     */
    ~EncoderTimer();

    // --- Basic Operations ---
    /**
     * @brief Initializes the GPIO pins and TIM2 for encoder mode.
     * Must be called before using other methods.
     * @return True if initialization was successful, false otherwise.
     */
    bool begin();

    /**
     * @brief De-initializes the timer and GPIOs.
     */
    void end();

    /**
     * @brief Resets the encoder count and error state.
     * Sets the hardware timer counter to 0.
     */
    void reset();

    // --- Position and Speed Methods ---
    /**
     * @brief Gets the complete current position and speed data.
     * @return Position struct populated with current data.
     */
    Position getPosition() const;

    /**
     * @brief Gets the current raw encoder count.
     * This is a direct read of the hardware timer's counter register.
     * @return Current encoder count (quadrature).
     */
    int32_t getCount() const;

    /**
     * @brief Calculates and returns the current RPM of the spindle.
     * @return Current RPM as a signed 16-bit integer.
     */
    int16_t getRPM() const;

    /**
     * @brief Checks if the encoder timer is initialized and no errors are present.
     * @return True if the encoder is initialized and valid, false otherwise.
     */
    bool isValid() const { return _initialized && !_error; }

    // --- Timer Access Methods (Primarily for Debugging or Advanced Use) ---
    /**
     * @brief Gets the raw value of the TIM2 counter register.
     * @return Raw 32-bit timer counter value.
     */
    uint32_t getRawCounter() const; // Implementation: return __HAL_TIM_GET_COUNTER(&htim2);

    /**
     * @brief Gets the TIM2 update interrupt flag status.
     * @return Value of the TIM_FLAG_UPDATE.
     */
    uint32_t getTimerStatus() const; // Implementation: return __HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE);

    /**
     * @brief Gets the value of the TIM2 Control Register 1 (CR1).
     * @return Raw 32-bit value of CR1.
     */
    uint32_t getTimerCR1() const; // Implementation: return htim2.Instance->CR1;

    /**
     * @brief Provides access to the underlying TIM_HandleTypeDef for TIM2.
     * Useful for direct HAL manipulation or ISR configuration if needed externally.
     * @return Pointer to the TIM_HandleTypeDef for TIM2.
     */
    TIM_HandleTypeDef *getTimerHandle() { return &htim2; }

    // --- Static Callback for Timer Interrupts ---
    /**
     * @brief Static callback function for TIM2 update interrupts (overflow/underflow).
     * This function calls the handleOverflow method of the singleton instance.
     * It must be registered with the NVIC for TIM2_IRQn.
     */
    static void updateCallback();

private:
    // Hardware handle for TIM2
    TIM_HandleTypeDef htim2;

    // State variables
    volatile int32_t _currentCount;    ///< Software-extended count (not currently used for extension, hardware count is 32-bit).
    volatile uint32_t _lastUpdateTime; ///< Timestamp of the last ISR update or significant event.
    volatile bool _error;              ///< Flag indicating an error state.
    bool _initialized;                 ///< True if begin() has been successfully called.

    // Initialization methods
    bool initGPIO();  ///< Initializes GPIO pins for TIM2 encoder channels.
    bool initTimer(); ///< Initializes TIM2 in encoder interface mode.

    // Helper methods
    /**
     * @brief Calculates RPM based on count changes over time.
     * Uses static local variables to track previous count and time for delta calculation.
     * @return Calculated RPM.
     */
    int16_t calculateRPM() const;

    /**
     * @brief Handles timer overflow/underflow interrupts.
     * Called by the static updateCallback.
     * Currently updates _lastUpdateTime. Could be used for software count extension if needed.
     */
    void handleOverflow();

    // Static instance for callbacks (singleton pattern for ISR)
    static EncoderTimer *instance;
};
