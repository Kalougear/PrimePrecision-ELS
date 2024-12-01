#pragma once

#include <Arduino.h>
#include "stm32h7xx_hal.h"
#include "Core/encoder_timer.h"
#include <STM32Step.h>
#include <HardwareTimer.h>

class SyncTimer
{
public:
    // Configuration structure
    struct SyncConfig
    {
        float thread_pitch;     // Desired thread pitch
        float leadscrew_pitch;  // Leadscrew pitch
        uint32_t update_freq;   // Sync update frequency (Hz)
        bool reverse_direction; // Reverse sync direction
    };

    SyncTimer();
    ~SyncTimer();

    // Basic operations
    bool begin();
    void end();
    void enable(bool enable);
    bool isEnabled() const { return _enabled; }

    // Configuration
    void setConfig(const SyncConfig &config);
    void setSyncFrequency(uint32_t freq);

    // Status
    bool hasError() const { return _error; }
    uint32_t getLastUpdateTime() const { return _lastUpdateTime; }

    // New method to process sync requests outside interrupt context
    void processSyncRequest();

    // For testing/debugging
    uint32_t getTimerFrequency() const { return _timerFrequency; }
    bool isInitialized() const { return _initialized; }

protected:
    // Timer interrupt handler - minimal processing
    void handleInterrupt();

private:
    // Hardware timer
    HardwareTimer *_timer;

    // State variables
    volatile bool _enabled;
    volatile bool _error;
    volatile uint32_t _lastUpdateTime;
    bool _initialized;

    // Configuration
    SyncConfig _config;
    uint32_t _timerFrequency;

    // References to other components
    EncoderTimer *_encoder;
    STM32Step::Stepper *_stepper;

    // Private methods
    bool initTimer();
    void calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period);
    float calculateSyncRatio() const;
    int32_t calculateRequiredSteps(int32_t encoderDelta);

    // Static instance for callback
    static SyncTimer *instance;

    // New sync request flag for interrupt handling
    static volatile bool syncRequested;
};
