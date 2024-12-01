// include/Core/encoder_timer.h
#pragma once

#include <Arduino.h>
#include "Config/encoder_config.h"
#include <HardwareTimer.h>

class EncoderTimer
{
public:
    // Position data structure
    struct Position
    {
        int32_t count;
        uint32_t timestamp;
        int16_t rpm;
        bool direction;
        bool valid;
    };

    // Sync configuration structure
    struct SyncConfig
    {
        float thread_pitch;
        float leadscrew_pitch;
        uint16_t stepper_steps;
        uint16_t microsteps;
        bool reverse_sync;
    };

    // Sync position structure
    struct SyncPosition
    {
        int32_t encoder_count;
        int32_t required_steps;
        uint32_t timestamp;
        bool valid;
    };

    EncoderTimer();
    ~EncoderTimer();

    // Basic operations
    bool begin();
    void end();
    void reset();

    // Position and speed methods
    Position getPosition();
    int32_t getCount();
    int16_t getRPM();
    bool isValid() const { return _initialized && !_error; }

    // Synchronization methods
    void setSyncConfig(const SyncConfig &config);
    void enableSync(bool enable) { _syncEnabled = enable; }
    bool isSyncEnabled() const { return _syncEnabled; }
    SyncPosition getSyncPosition();
    float calculateStepperFrequency(int16_t rpm);

    // Timer access methods
    uint32_t getRawCounter() const
    {
        return __HAL_TIM_GET_COUNTER(&htim2);
    }

    uint32_t getTimerStatus() const
    {
        return __HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE);
    }

    uint32_t getTimerCR1() const
    {
        return htim2.Instance->CR1;
    }

    // Get timer handle for interrupt handler
    TIM_HandleTypeDef *getTimerHandle() { return &htim2; }

    // Static callback for timer interrupts
    static void updateCallback();

private:
    // Hardware handle
    TIM_HandleTypeDef htim2;

    // State variables
    volatile int32_t _currentCount;
    volatile uint32_t _lastUpdateTime;
    volatile bool _error;
    bool _initialized;
    bool _syncEnabled;

    // Sync variables
    SyncConfig _syncConfig;
    volatile int32_t _lastSyncCount;

    // Initialization methods
    bool initGPIO();
    bool initTimer();

    // Helper methods
    int16_t calculateRPM();
    int32_t calculateRequiredSteps(int32_t encoderCount);
    float calculateSyncRatio() const;
    void handleOverflow();

    // Static instance for callbacks
    static EncoderTimer *instance;
};
