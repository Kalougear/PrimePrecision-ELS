#pragma once

#include "stm32h7xx_hal.h"
#include <Arduino.h>

// Helper macro to create pin definitions
#define GPIO_PIN(pin_def) (uint16_t)(1 << (digitalPinToPinName(pin_def) & 0x0F))
#define GPIO_PORT(pin_def) ((GPIO_TypeDef *)(GPIOA_BASE + (0x400 * ((digitalPinToPinName(pin_def) >> 4) & 0x0F))))

class GPIOManager
{
public:
    static void initPin(uint8_t pin_def, uint32_t mode, uint32_t pull = GPIO_NOPULL, uint32_t speed = GPIO_SPEED_FREQ_HIGH)
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN(pin_def);
        GPIO_InitStruct.Mode = mode;
        GPIO_InitStruct.Pull = pull;
        GPIO_InitStruct.Speed = speed;

        // Enable GPIO clock based on port
        uint8_t port = (digitalPinToPinName(pin_def) >> 4) & 0x0F;
        switch (port)
        {
        case 0: // Port A
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1: // Port B
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2: // Port C
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3: // Port D
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4: // Port E
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
        }

        HAL_GPIO_Init(GPIO_PORT(pin_def), &GPIO_InitStruct);
    }

    // New method for timer alternate function configuration
    static void initTimerPin(uint8_t pin_def, uint32_t alternate_func)
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN(pin_def);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = alternate_func;

        // Enable GPIO clock based on port
        uint8_t port = (digitalPinToPinName(pin_def) >> 4) & 0x0F;
        switch (port)
        {
        case 0: // Port A
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1: // Port B
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2: // Port C
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3: // Port D
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4: // Port E
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
        }

        HAL_GPIO_Init(GPIO_PORT(pin_def), &GPIO_InitStruct);
    }

    static void writePin(uint8_t pin_def, GPIO_PinState state)
    {
        HAL_GPIO_WritePin(GPIO_PORT(pin_def), GPIO_PIN(pin_def), state);
    }

    static GPIO_PinState readPin(uint8_t pin_def)
    {
        return HAL_GPIO_ReadPin(GPIO_PORT(pin_def), GPIO_PIN(pin_def));
    }

    static void togglePin(uint8_t pin_def)
    {
        HAL_GPIO_TogglePin(GPIO_PORT(pin_def), GPIO_PIN(pin_def));
    }
};
