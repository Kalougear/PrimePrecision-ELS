# STM32H743 Pin Mapping for PrimePrecision ELS

## Overview

This document provides a comprehensive mapping of all pins used in the PrimePrecision Electronic Lead Screw project.

## Quick Reference Table

| Pin  | Function  | Direction | Description                         |
| ---- | --------- | --------- | ----------------------------------- |
| PA2  | USART2_TX | Output    | Debug Serial Output                 |
| PA3  | USART2_RX | Input     | Debug Serial Input                  |
| PA9  | USART1_TX | Output    | Display Serial Output               |
| PA10 | USART1_RX | Input     | Display Serial Input                |
| PE7  | STEP_EN   | Output    | Stepper Motor Enable                |
| PE8  | STEP_DIR  | Output    | Stepper Motor Direction             |
| PE9  | STEP_PUL  | Output    | Stepper Motor Step Pulse (TIM1_CH1) |
| PA0  | ENC_A     | Input     | Encoder Channel A (TIM2_CH1)        |
| PA1  | ENC_B     | Input     | Encoder Channel B (TIM2_CH2)        |
| PA5  | ENC_IDX   | Input     | Encoder Index Signal                |

## Detailed Pin Descriptions

### Serial Communication

#### Debug Interface (USART2)

- **PA2 (TX)**: Debug serial output

  - Mode: Alternate Function (AF7)
  - Speed: High
  - Used for development debugging and status messages

- **PA3 (RX)**: Debug serial input
  - Mode: Alternate Function (AF7)
  - Pull-Up: Enabled
  - Used for receiving debug commands

#### Display Interface (USART1)

- **PA9 (TX)**: Display serial output

  - Mode: Alternate Function (AF7)
  - Speed: High
  - Communicates with Nextion display

- **PA10 (RX)**: Display serial input
  - Mode: Alternate Function (AF7)
  - Pull-Up: Enabled
  - Receives commands from display

### Stepper Motor Control

#### Motor Driver Interface

- **PE7 (STEP_EN)**: Stepper motor enable

  - Mode: Output Push-Pull
  - Speed: High
  - Active Low: Motor enabled when pin is LOW
  - Default State: HIGH (disabled)

- **PE8 (STEP_DIR)**: Direction control

  - Mode: Output Push-Pull
  - Speed: High
  - Logic: HIGH = CW, LOW = CCW
  - Used for controlling rotation direction

- **PE9 (STEP_PUL)**: Step pulse
  - Mode: Alternate Function (TIM1_CH1)
  - Speed: Very High
  - Frequency Range: 100 Hz - 20 kHz
  - Each pulse moves the motor one step

### Encoder Interface

#### Quadrature Encoder Input

- **PA0 (ENC_A)**: Encoder channel A

  - Mode: Alternate Function (TIM2_CH1)
  - Pull-Up: Enabled
  - Filter: Digital noise filter enabled
  - Used for position and direction sensing

- **PA1 (ENC_B)**: Encoder channel B

  - Mode: Alternate Function (TIM2_CH2)
  - Pull-Up: Enabled
  - Filter: Digital noise filter enabled
  - Phase shifted 90° from Channel A

- **PA5 (ENC_IDX)**: Encoder index
  - Mode: Input
  - Pull-Up: Enabled
  - Interrupt: EXTI line enabled
  - Triggers once per revolution

## Timer Configuration

### TIM1 (Stepper Control)

- Channel 1 (PE9) configured for PWM output
- Base frequency: Calculated from SystemClock
- Used for precise step pulse generation

### TIM2 (Encoder Interface)

- Channels 1/2 (PA0/PA1) configured for encoder mode
- 32-bit counter for extended position range
- Quadrature decoding with 4x resolution

## Notes

1. All unused pins are configured as analog inputs to minimize power consumption
2. Critical timing pins (stepper and encoder) use direct timer hardware
3. Serial communication pins operate at 115200 baud
4. All output pins are configured for maximum slew rate where needed
5. Inputs have noise filtering enabled where appropriate

## Hardware Considerations

- Use appropriate pull-up/down resistors for encoder inputs if not provided by encoder
- Stepper driver must be compatible with 3.3V logic levels
- Consider using opto-isolation for stepper signals in noisy environments
- Ensure proper power sequencing with enable signal
