// include/Core/encoder_timer.h

#pragma once

#include <Arduino.h>

class EncoderTimer

{

public:
    EncoderTimer(void);

    ~EncoderTimer(void);

    // Basic operations

    bool begin(void);

    void end(void);

    // Encoder operations

    int32_t get_count(void);

    void reset_count(void);

    // Status check

    bool is_initialized(void) { return initialized_; }

private:
    static const uint8_t k_encoder_pin_a = PA0; // TIM2_CH1

    static const uint8_t k_encoder_pin_b = PA1; // TIM2_CH2

    TIM_HandleTypeDef h_tim2_; // Timer handle

    bool initialized_;

    bool init_gpio(void);

    bool init_timer(void);
};