#include <Arduino.h>
#include "stm32h7xx_hal.h"

// Serial for debugging (PA3=RX, PA2=TX)
HardwareSerial SerialDebug(PA3 /* RX */, PA2 /* TX */);

// Timer handle
TIM_HandleTypeDef htim2;

void setup()
{
    // Initialize serial
    SerialDebug.begin(115200);
    delay(100);
    SerialDebug.println("Starting minimal encoder test...");

    // Enable GPIO clock
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure encoder pins (PA0 and PA1)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Enable Timer2 clock
    __HAL_RCC_TIM2_CLK_ENABLE();

    // Configure Timer2 for encoder mode
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 0xF;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 0xF;

    if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
    {
        SerialDebug.println("Failed to initialize encoder timer");
        while (1)
            ;
    }

    // Start encoder
    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
    {
        SerialDebug.println("Failed to start encoder timer");
        while (1)
            ;
    }

    SerialDebug.println("Encoder initialized successfully");
}

void loop()
{
    static uint32_t lastPrint = 0;
    static int32_t lastCount = 0;
    const uint32_t PRINT_INTERVAL = 100; // Print every 100ms

    if (millis() - lastPrint >= PRINT_INTERVAL)
    {
        // Read current count
        int32_t currentCount = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);

        // Calculate delta and direction
        int32_t delta = currentCount - lastCount;
        bool direction = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2);

        // Calculate RPM (assuming 2400 PPR encoder)
        // delta counts per 100ms -> multiply by 10 for counts per second
        // divide by 2400 for revolutions per second
        // multiply by 60 for RPM
        float rpm = ((float)delta * 10.0f * 60.0f) / 2400.0f;

        SerialDebug.print("Count: ");
        SerialDebug.print(currentCount);
        SerialDebug.print(", Delta: ");
        SerialDebug.print(delta);
        SerialDebug.print(", Dir: ");
        SerialDebug.print(direction ? "DOWN" : "UP");
        SerialDebug.print(", RPM: ");
        SerialDebug.println(rpm);

        lastCount = currentCount;
        lastPrint = millis();
    }
}
