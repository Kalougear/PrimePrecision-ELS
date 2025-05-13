#include "Motion/SyncTimer.h"
#include "Config/serial_debug.h"
#include "Hardware/EncoderTimer.h"

// Static instance initialization
SyncTimer *SyncTimer::instance = nullptr;

// Initialize static sync request flag
volatile bool SyncTimer::syncRequested = false;

SyncTimer::SyncTimer() : _timer(nullptr),
                         _enabled(false),
                         _error(false),
                         _lastUpdateTime(0),
                         _initialized(false),
                         _timerFrequency(1000), // Default 1kHz update rate
                         _encoder(nullptr),
                         _stepper(nullptr)
{
}

SyncTimer::~SyncTimer()
{
    end();
    if (instance == this)
    {
        instance = nullptr;
    }
    if (_timer)
    {
        delete _timer;
        _timer = nullptr;
    }
}

bool SyncTimer::begin()
{
    if (_initialized)
    {
        return true;
    }

    instance = this;
    SerialDebug.println("Initializing Sync Timer...");

    if (!initTimer())
    {
        SerialDebug.println("Sync Timer initialization failed");
        return false;
    }

    _initialized = true;
    SerialDebug.println("Sync Timer initialized successfully");
    return true;
}

bool SyncTimer::initTimer()
{
    // Initialize timer on TIM6
    _timer = new HardwareTimer(TIM6);
    if (!_timer)
    {
        SerialDebug.println("Timer allocation failed");
        return false;
    }

    // Calculate initial timer parameters
    uint32_t prescaler, period;
    calculateTimerParameters(_timerFrequency, prescaler, period);

    // Configure timer
    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);

    // Set callback - now just sets a flag
    _timer->attachInterrupt([this]()
                            { this->handleInterrupt(); });

    return true;
}

void SyncTimer::calculateTimerParameters(uint32_t freq, uint32_t &prescaler, uint32_t &period)
{
    uint32_t timerClock = SystemCoreClock;
    uint32_t targetTicks = timerClock / freq;

    prescaler = 1;
    period = targetTicks;

    while (period > 0xFFFF)
    {
        prescaler++;
        period = targetTicks / prescaler;
    }
}

void SyncTimer::end()
{
    if (!_initialized)
    {
        return;
    }

    if (_timer)
    {
        _timer->pause();
        _timer->detachInterrupt();
    }
    _initialized = false;
}

void SyncTimer::enable(bool enable)
{
    if (!_initialized || !_timer)
    {
        return;
    }

    _enabled = enable;
    if (enable)
    {
        _timer->resume();
    }
    else
    {
        _timer->pause();
    }
}

void SyncTimer::setConfig(const SyncConfig &config)
{
    _config = config;
    setSyncFrequency(config.update_freq);
}

void SyncTimer::setSyncFrequency(uint32_t freq)
{
    if (!_initialized || !_timer || freq == 0)
    {
        return;
    }

    uint32_t prescaler, period;
    calculateTimerParameters(freq, prescaler, period);

    _timer->setPrescaleFactor(prescaler);
    _timer->setOverflow(period);

    _timerFrequency = freq;
}

// Minimal interrupt handler - just sets a flag
void SyncTimer::handleInterrupt()
{
    syncRequested = true;
}

// Process sync outside interrupt context
void SyncTimer::processSyncRequest()
{
    if (!syncRequested || !_enabled || !_encoder || !_stepper)
    {
        return;
    }

    static int32_t lastEncoderCount = 0;

    // Get encoder count with interrupts disabled
    __disable_irq();
    int32_t currentCount = _encoder->getCount();
    __enable_irq();

    int32_t encoderDelta = currentCount - lastEncoderCount;

    if (encoderDelta != 0)
    {
        int32_t requiredSteps = calculateRequiredSteps(encoderDelta);
        _stepper->setRelativePosition(requiredSteps);
        lastEncoderCount = currentCount;
    }

    _lastUpdateTime = HAL_GetTick();
    syncRequested = false;
}

float SyncTimer::calculateSyncRatio() const
{
    return _config.thread_pitch / _config.leadscrew_pitch;
}

int32_t SyncTimer::calculateRequiredSteps(int32_t encoderDelta)
{
    float sync_ratio = calculateSyncRatio();
    float steps = encoderDelta * sync_ratio;
    return _config.reverse_direction ? -static_cast<int32_t>(steps) : static_cast<int32_t>(steps);
}
