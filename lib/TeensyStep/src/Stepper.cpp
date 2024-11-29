#include <Arduino.h>
#include "Stepper.h"

#pragma push_macro("abs")
#undef abs

namespace TeensyStep
{
    constexpr int32_t Stepper::vMaxMax;
    constexpr uint32_t Stepper::aMax;
    constexpr uint32_t Stepper::vMaxDefault;
    constexpr uint32_t Stepper::vPullInOutDefault;
    constexpr uint32_t Stepper::aDefault;

    Stepper::Stepper(const int _stepPin, const int _dirPin)
        : current(0), stepPin(_stepPin), dirPin(_dirPin)
    {
        setStepPinPolarity(HIGH);
        setInverseRotation(false);
        setAcceleration(aDefault);
        setMaxSpeed(vMaxDefault);
        setPullInSpeed(vPullInOutDefault);

#if defined(STM32H7xx)
        // Initialize pins using GPIOManager
        GPIOManager::initPin(stepPin, GPIO_MODE_OUTPUT_PP);
        GPIOManager::initPin(dirPin, GPIO_MODE_OUTPUT_PP);
#else
        pinMode(stepPin, OUTPUT);
        pinMode(dirPin, OUTPUT);
#endif
    }

    Stepper &Stepper::setStepPinPolarity(int polarity)
    {
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        // Calculate addresses of bitbanded pin-set and pin-clear registers
        uint32_t pinRegAddr = (uint32_t)digital_pin_to_info_PGM[stepPin].reg;
        uint32_t *pinSetReg = (uint32_t *)(pinRegAddr + 4 * 32);
        uint32_t *pinClearReg = (uint32_t *)(pinRegAddr + 8 * 32);

        if (polarity == LOW)
        {
            stepPinActiveReg = pinClearReg;
            stepPinInactiveReg = pinSetReg;
        }
        else
        {
            stepPinActiveReg = pinSetReg;
            stepPinInactiveReg = pinClearReg;
        }
#else
        this->polarity = polarity;
#endif
        clearStepPin(); // set step pin to inactive state
        return *this;
    }

    Stepper &Stepper::setInverseRotation(bool reverse)
    {
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        // Calculate addresses of bitbanded pin-set and pin-clear registers
        uint32_t pinRegAddr = (uint32_t)digital_pin_to_info_PGM[dirPin].reg;
        uint32_t *pinSetReg = (uint32_t *)(pinRegAddr + 4 * 32);
        uint32_t *pinClearReg = (uint32_t *)(pinRegAddr + 8 * 32);

        if (reverse)
        {
            dirPinCwReg = pinClearReg;
            dirPinCcwReg = pinSetReg;
        }
        else
        {
            dirPinCwReg = pinSetReg;
            dirPinCcwReg = pinClearReg;
        }
#else
        this->reverse = reverse;
#endif
        return *this;
    }

    Stepper &Stepper::setAcceleration(uint32_t a) // steps/s^2
    {
        this->a = std::min(aMax, a);
        return *this;
    }

    Stepper &Stepper::setMaxSpeed(int32_t speed)
    {
        setDir(speed >= 0 ? 1 : -1);
        vMax = std::min(vMaxMax, std::max(-vMaxMax, speed));
        return *this;
    }

    Stepper &Stepper::setPullInSpeed(int32_t speed)
    {
        vPullIn = vPullOut = std::abs(speed);
        return *this;
    }

    Stepper &Stepper::setPullInOutSpeed(int32_t pullInSpeed, int32_t pullOutSpeed)
    {
        vPullIn = std::abs(pullInSpeed);
        vPullOut = std::abs(pullOutSpeed);
        return *this;
    }

    void Stepper::setTargetAbs(int32_t target)
    {
        setTargetRel(target - current);
    }

    void Stepper::setTargetRel(int32_t delta)
    {
        setDir(delta < 0 ? -1 : 1);
        target = current + delta;
        A = std::abs(delta);
    }
}

#pragma pop_macro("abs")
