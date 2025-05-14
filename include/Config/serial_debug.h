#pragma once

#include <HardwareSerial.h>

/**
 * @brief Serial debug configuration
 *
 * IMPORTANT: This file intentionally uses a simple external declaration instead of debug macros.
 * The direct hardware access approach was chosen to improve reliability of UART communication
 * on the STM32H743. Previous implementations using debug macros caused timing/initialization
 * issues.
 *
 * DO NOT add debug macro layers (e.g., DEBUG_PRINT) as they can introduce timing complexities
 * in the microcontroller environment. Direct SerialDebug calls are more reliable for
 * time-critical operations.
 */
extern HardwareSerial SerialDebug;
