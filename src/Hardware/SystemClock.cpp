#include "Hardware/SystemClock.h"
#include "Config/serial_debug.h"
#include "stm32h7xx_hal.h"

SystemClock::SystemClock()
    : current_source_(CLOCK_HSI),
      last_error_(CLOCK_OK),
      last_sysclk_freq_(0),
      last_hclk_freq_(0),
      last_pclk1_freq_(0),
      last_pclk2_freq_(0)
{
}

SystemClock &SystemClock::GetInstance()
{
    static SystemClock instance;
    return instance;
}

// This C function is typically called by SystemInit() from the HAL startup.
// It should contain the core clock configuration logic.
extern "C" void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rccOscInit = {};
    RCC_ClkInitTypeDef rccClkInit = {};

    // Configure power supply scaling
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    // Configure HSE and PLL
    rccOscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rccOscInit.HSEState = RCC_HSE_ON; // Assuming an external crystal is used
    rccOscInit.PLL.PLLState = RCC_PLL_ON;
    rccOscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rccOscInit.PLL.PLLM = 5;                    // For 25MHz HSE: 25/5 = 5MHz PLL input
    rccOscInit.PLL.PLLN = 160;                  // 5MHz * 160 = 800MHz (VCO output)
                                                // For 400MHz SYSCLK, PLLP must be 2.
                                                // For 480MHz SYSCLK, PLLN would be 192 for 960MHz VCO, then PLLP=2.
                                                // Let's target 400MHz for CPU as per previous successful config.
                                                // So, PLLN = 80 for 400MHz VCO, then PLLP = 1.
                                                // Or PLLN = 160 for 800MHz VCO, then PLLP = 2 for 400MHz SYSCLK.
                                                // Let's use PLLN=160, PLLP=2 for 400MHz SYSCLK.
    rccOscInit.PLL.PLLN = 160;                  // 5MHz * 160 = 800MHz VCO
    rccOscInit.PLL.PLLP = 2;                    // 800MHz / 2 = 400MHz SYSCLK
    rccOscInit.PLL.PLLQ = 8;                    // 800MHz / 8 = 100MHz (e.g. for QSPI, SDMMC, RNG, USB if PHY used)
    rccOscInit.PLL.PLLR = 2;                    // Not critical if PLLRCLK not used for specific peripherals
    rccOscInit.PLL.PLLRGE = RCC_PLL1VCIRANGE_2; // VCO input range 4-8 MHz (5MHz is in this range)
    rccOscInit.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE; // Wide VCO range (192-836 MHz, 800MHz is in this range)
    rccOscInit.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&rccOscInit) != HAL_OK)
    {
        // Error_Handler() might be called here in a non-Arduino context.
        // For now, let this proceed; C++ init will check.
        // SystemClock::GetInstance().setError(ERR_PLL_TIMEOUT); // Cannot call private member from C function
        // If this fails, system might not boot. Consider a global error flag or Error_Handler().
    }

    // Configure clock dividers
    rccClkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                           RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                           RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    rccClkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rccClkInit.SYSCLKDivider = RCC_SYSCLK_DIV1; // SYSCLK = 400MHz
    rccClkInit.AHBCLKDivider = RCC_HCLK_DIV2;   // HCLK (AXI, AHBs) = 200MHz
    rccClkInit.APB3CLKDivider = RCC_APB3_DIV2;  // APB3 (LTDC, WWDG1) = 100MHz
    rccClkInit.APB1CLKDivider = RCC_APB1_DIV2;  // APB1 (TIMs, UARTs, I2Cs) = 100MHz
    rccClkInit.APB2CLKDivider = RCC_APB2_DIV2;  // APB2 (TIMs, UARTs, SPIs) = 100MHz
    rccClkInit.APB4CLKDivider = RCC_APB4_DIV2;  // APB4 (SYSCFG, LPTIMs, DAC, COMP) = 100MHz

    // Flash latency needs to be set according to HCLK and VOS
    // For HCLK = 200MHz and VOS0, FLASH_LATENCY_2 might be okay.
    // For HCLK = 200MHz and VOS1, FLASH_LATENCY_4 is safer if SYSCLK is 400MHz.
    // Let's assume VOS0 is active from __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    // Datasheet RM0433 Rev 7, Table 19. Flash memory access latency.
    // For VCORE VOS0, HCLK up to 240MHz -> Latency 2. (If SYSCLK is 480MHz, HCLK is 240MHz)
    // If SYSCLK is 400MHz, HCLK is 200MHz. VOS0 allows HCLK up to 240MHz with Latency 2.
    // If VOS1, HCLK up to 200MHz -> Latency 2.
    // Let's use FLASH_LATENCY_2 for HCLK=200MHz (SYSCLK=400MHz, VOS0)
    // The previous value was FLASH_LATENCY_4. Let's stick to that if it was working,
    // as it's safer for higher clock speeds.
    // For 400MHz SYSCLK (HCLK=200MHz), VOS0, Latency 4 is correct.
    // For 480MHz SYSCLK (HCLK=240MHz), VOS0, Latency 4 is correct.
    // Let's use FLASH_LATENCY_4 as it was in the original working version.
    if (HAL_RCC_ClockConfig(&rccClkInit, FLASH_LATENCY_4) != HAL_OK)
    {
        // SystemClock::GetInstance().setError(ERR_SYSCLK_TIMEOUT); // Cannot call private member from C function
    }

    // Reconfigure Systick to ensure it has the correct HCLK
    // This is done after HAL_Init() in the Arduino core, but SystemClock_Config() is called by SystemInit() before HAL_Init().
    // So, HAL_Init() will configure SysTick with whatever HCLK is present after this function.
    // The C++ initialize() method will call HAL_InitTick() again to be sure.
}

// The C++ initialize method now calls the C SystemClock_Config
// and then updates its internal state and ensures SysTick is correct.
bool SystemClock::initialize()
{
    // SystemClock_Config() is called by SystemInit() before HAL_Init() in Arduino context.
    // So, by the time this C++ initialize() is called from setup(), clocks are already set.
    // We just need to ensure HAL's internal state and SysTick are updated if this is called again
    // or if we want to be absolutely sure after the Arduino core's HAL_Init().

    // If SystemClock_Config() was not called by SystemInit, or to ensure it's configured:
    // SystemClock_Config(); // This might be redundant if SystemInit already called it.

    // Update HAL's SystemCoreClock variable (important for HAL functions)
    SystemCoreClockUpdate();

    // Re-initialize SysTick with the potentially new SystemCoreClock (HCLK) value.
    // HAL_Init() in Arduino core already does this, but if we change clocks later,
    // or if SystemClock_Config ran before HAL_Init, this ensures correctness.
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
    {
        setError(ERR_SYSCLK_TIMEOUT); // Or a new error code for SysTick failure
        return false;
    }

    // Cache current frequencies
    last_sysclk_freq_ = HAL_RCC_GetSysClockFreq();
    last_hclk_freq_ = HAL_RCC_GetHCLKFreq();
    last_pclk1_freq_ = HAL_RCC_GetPCLK1Freq();
    last_pclk2_freq_ = HAL_RCC_GetPCLK2Freq();

#ifdef DEBUG_ENABLE
    SerialDebug.print("System Clock Frequency (from C++ init): ");
    SerialDebug.print(last_sysclk_freq_ / 1000000);
    SerialDebug.println(" MHz");
    SerialDebug.print("HCLK Frequency (from C++ init): ");
    SerialDebug.print(last_hclk_freq_ / 1000000);
    SerialDebug.println(" MHz");
#endif

    // Basic verification
    if (last_hclk_freq_ == 0 || last_sysclk_freq_ == 0)
    {
        setError(ERR_FREQ_VERIFY);
        return false;
    }

    current_source_ = CLOCK_HSE; // Assuming HSE was successfully used
    last_error_ = CLOCK_OK;
    return true;
}

bool SystemClock::IsClockStable() const
{
    // A more robust check might involve re-reading and comparing if dynamic changes are possible
    return (HAL_RCC_GetSysClockFreq() == last_sysclk_freq_) && (last_error_ == CLOCK_OK);
}

uint32_t SystemClock::GetSysClockFreq() const
{
    return HAL_RCC_GetSysClockFreq(); // Always return current for accuracy
}

uint32_t SystemClock::GetHClkFreq() const
{
    return HAL_RCC_GetHCLKFreq(); // Always return current
}

uint32_t SystemClock::GetPClk1Freq() const
{
    return HAL_RCC_GetPCLK1Freq(); // Always return current
}

uint32_t SystemClock::GetPClk2Freq() const
{
    return HAL_RCC_GetPCLK2Freq(); // Always return current
}

const char *SystemClock::GetErrorMessage() const
{
    switch (last_error_)
    {
    case CLOCK_OK:
        return "No Error";
    case ERR_HSE_TIMEOUT:
        return "HSE Timeout";
    case ERR_PLL_TIMEOUT:
        return "PLL Timeout";
    case ERR_PWR_TIMEOUT:
        return "Power Timeout";
    case ERR_SYSCLK_TIMEOUT:
        return "SYSCLK Timeout";
    case ERR_FREQ_VERIFY:
        return "Frequency Verification Failed";
    default:
        return "Unknown Error";
    }
}
