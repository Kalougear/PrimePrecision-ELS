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

extern "C" void SystemClock_Config(void)
{
    SystemClock::GetInstance().initialize(); // Changed from Initialize to initialize
}

bool SystemClock::initialize()
{
    // Initialize system clock to 400 MHz using HSE as source

    RCC_OscInitTypeDef rccOscInit = {};
    RCC_ClkInitTypeDef rccClkInit = {};

    // Configure power supply scaling
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    // Note: Still using VOS0 for maximum performance
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    // Wait for voltage scaling to complete
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    // Configure HSE and PLL
    rccOscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rccOscInit.HSEState = RCC_HSE_ON;
    rccOscInit.PLL.PLLState = RCC_PLL_ON;
    rccOscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    // Modified PLL configuration for 400MHz operation
    rccOscInit.PLL.PLLM = 5;                    // M div 5 (25MHz / 5 = 5MHz PLL input)
    rccOscInit.PLL.PLLN = 80;                   // N mul 80 (5MHz * 80 = 400MHz)
    rccOscInit.PLL.PLLP = 1;                    // P div 1 (400MHz for CPU)
    rccOscInit.PLL.PLLQ = 8;                    // Q div 8 (50MHz for USB, adjust if needed)
    rccOscInit.PLL.PLLR = 8;                    // R unused
    rccOscInit.PLL.PLLRGE = RCC_PLL1VCIRANGE_2; // 4-8 MHz VCO input
    rccOscInit.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    rccOscInit.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&rccOscInit) != HAL_OK)
    {
        setError(ERR_PLL_TIMEOUT);
        return false;
    }

    // Configure clock dividers
    rccClkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                           RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                           RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;

    rccClkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rccClkInit.SYSCLKDivider = RCC_SYSCLK_DIV1; // 400MHz CPU clock
    rccClkInit.AHBCLKDivider = RCC_HCLK_DIV2;   // 200MHz AHB clock
    rccClkInit.APB3CLKDivider = RCC_APB3_DIV2;  // 100MHz APB3 clock
    rccClkInit.APB1CLKDivider = RCC_APB1_DIV2;  // 100MHz APB1 clock
    rccClkInit.APB2CLKDivider = RCC_APB2_DIV2;  // 100MHz APB2 clock
    rccClkInit.APB4CLKDivider = RCC_APB4_DIV2;  // 100MHz APB4 clock

    // Using FLASH_LATENCY_4 for 400MHz operation
    if (HAL_RCC_ClockConfig(&rccClkInit, FLASH_LATENCY_4) != HAL_OK)
    {
        setError(ERR_SYSCLK_TIMEOUT);
        return false;
    }

    // Cache current frequencies
    last_sysclk_freq_ = HAL_RCC_GetSysClockFreq(); // Should read ~400MHz
    last_hclk_freq_ = HAL_RCC_GetHCLKFreq();       // Should read ~200MHz
    last_pclk1_freq_ = HAL_RCC_GetPCLK1Freq();     // Should read ~100MHz
    last_pclk2_freq_ = HAL_RCC_GetPCLK2Freq();     // Should read ~100MHz

#ifdef DEBUG_ENABLE
    SerialDebug.print("System Clock Frequency: ");
    SerialDebug.print(last_sysclk_freq_ / 1000000);
    SerialDebug.println(" MHz");
#endif

    return true;
}

bool SystemClock::IsClockStable() const
{
    return HAL_RCC_GetSysClockFreq() == last_sysclk_freq_;
}

uint32_t SystemClock::GetSysClockFreq() const
{
    return HAL_RCC_GetSysClockFreq();
}

uint32_t SystemClock::GetHClkFreq() const
{
    return HAL_RCC_GetHCLKFreq();
}

uint32_t SystemClock::GetPClk1Freq() const
{
    return HAL_RCC_GetPCLK1Freq();
}

uint32_t SystemClock::GetPClk2Freq() const
{
    return HAL_RCC_GetPCLK2Freq();
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