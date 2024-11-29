// src/Core/encoder_interrupts.cpp
#include "Core/encoder_timer.h"

namespace
{
    EncoderTimer *currentEncoder = nullptr;
}

extern "C" void setCurrentEncoder(EncoderTimer *encoder)
{
    currentEncoder = encoder;
}

// Only keep DMA interrupt handler since it's not handled by HardwareTimer
extern "C" void DMA1_Stream1_IRQHandler(void)
{
    if (!currentEncoder)
        return;
    HAL_DMA_IRQHandler(&currentEncoder->_hdma);
}
