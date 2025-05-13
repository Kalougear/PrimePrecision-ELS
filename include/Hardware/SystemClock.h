// system_clock.h
#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include "stm32h7xx_hal.h"

// Error codes for clock configuration
enum ClockError
{
    CLOCK_OK = 0,
    ERR_HSE_TIMEOUT,
    ERR_PLL_TIMEOUT,
    ERR_PWR_TIMEOUT,
    ERR_SYSCLK_TIMEOUT,
    ERR_FREQ_VERIFY
};

// Clock source types
enum ClockSource
{
    CLOCK_HSI = 0,
    CLOCK_HSE
};

class SystemClock
{
public:
    // Get singleton instance
    static SystemClock &GetInstance();

    // Initialize system clock with HSE and PLL
    bool initialize(); // This was missing

    // Clock status checks
    bool IsClockStable() const;

    // Get current frequencies
    uint32_t GetSysClockFreq() const;
    uint32_t GetHClkFreq() const;
    uint32_t GetPClk1Freq() const;
    uint32_t GetPClk2Freq() const;

    // Get last error message
    const char *GetErrorMessage() const;

private:
    // Private constructor for singleton
    SystemClock();

    // Prevent copying
    SystemClock(const SystemClock &) = delete;
    SystemClock &operator=(const SystemClock &) = delete;

    // Set error code
    void setError(ClockError error) { last_error_ = error; }

    ClockSource current_source_;
    ClockError last_error_;

    // Cache for clock frequencies
    uint32_t last_sysclk_freq_;
    uint32_t last_hclk_freq_;
    uint32_t last_pclk1_freq_;
    uint32_t last_pclk2_freq_;
};

// External C function for HAL
extern "C" void SystemClock_Config(void);

#endif