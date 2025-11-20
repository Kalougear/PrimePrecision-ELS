# Changelog

## [Unreleased]

### Changed

- **Refactored Stepper Motor Control to Hardware PWM:**
  - Replaced the software ISR-based step pulse generation with a hardware-driven approach using TIM1 in PWM mode.
  - This eliminates the high-frequency interrupt load on the CPU, resulting in jitter-free step pulses and significantly improved performance.
  - The `STM32Step` library was modified to use the timer's Repetition Counter (RCR) for precise, non-blocking moves (`moveExact`) and continuous frequency control for jogging (`runContinuous`).
  - The `SyncTimer` ISR now calculates the required velocity and step count for each time slice and commands the hardware timer directly, rather than bit-banging GPIOs.

### Fixed

- Serial communication reliability improved by simplifying serial debug configuration
  - Removed debug macro layer from serial_debug.h that was causing potential timing/initialization issues
  - Simplified to direct UART communication using external SerialDebug declaration
  - Previous implementation:
    ```cpp
    #define DEBUG_ENABLE 1
    #define DEBUG_PRINT(x) SerialDebug.print(x)
    #define DEBUG_PRINTLN(x) SerialDebug.println(x)
    ```
  - Current implementation:
    ```cpp
    extern HardwareSerial SerialDebug;
    ```
  - Impact: More reliable serial communication on STM32H743 by removing macro indirection layer
  - Technical details:
    - Eliminates potential timing issues from macro processing
    - Provides direct hardware access to UART
    - Reduces complexity in serial communication path
    - Better suited for time-critical microcontroller operations

### Technical Notes

- When implementing debug systems on STM32 microcontrollers, prefer direct hardware access over macro layers for critical communication paths
- Debug macros, while useful for conditional compilation, can introduce timing complexities in microcontroller environments
- Consider the impact of abstraction layers on hardware communication reliability
