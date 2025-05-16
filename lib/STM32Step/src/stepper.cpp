#include "stepper.h"
#include "Config/serial_debug.h" // Only if essential debug prints are kept, otherwise remove
#include "Config/SystemConfig.h"
#include "stm32h7xx_hal.h"
#include <algorithm> // For std::abs if used, though not currently

namespace STM32Step
{
    /**
     * @brief Constructs a Stepper object.
     * @param stepPin The GPIO pin number for the STEP signal.
     * @param dirPin The GPIO pin number for the DIRECTION signal.
     * @param enablePin The GPIO pin number for the ENABLE signal.
     */
    Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
        : _stepPin(stepPin),
          _dirPin(dirPin),
          _enablePin(enablePin),
          _currentPosition(0),
          _targetPosition(0),
          _enabled(false),
          _running(false),
          _currentDirection(false), // Assuming default direction is forward (e.g., false = CW, true = CCW)
          _operationMode(OperationMode::IDLE),
          state(0) // Initial state for the ISR state machine
    {
        initPins();
    }

    /**
     * @brief Destructor for the Stepper object. Ensures the stepper is disabled.
     */
    Stepper::~Stepper()
    {
        disable();
    }

    /**
     * @brief Interrupt Service Routine for step generation.
     * This function is called by a timer interrupt (e.g., TIM1 via TimerControl)
     * at a high frequency to generate step pulses.
     * It implements a simple state machine for pulse generation and direction changes.
     */
    void Stepper::ISR(void)
    {
        // Only operate if enabled and running (i.e., a move is in progress)
        if (!_enabled || !_running)
        {
            state = 0;         // Reset state machine
            GPIO_CLEAR_STEP(); // Ensure step pin is low if not actively stepping
            return;
        }

        // Check if the target position has been reached
        int32_t positionDifference = _targetPosition - _currentPosition;
        if (positionDifference == 0)
        {
            _running = false;  // Stop running
            state = 0;         // Reset state machine
            GPIO_CLEAR_STEP(); // Ensure step pin is low
            // Note: TimerControl::stop() is NOT called here to allow TIM1 to continue running.
            // The ISR will just return early if !_running.
            // This prevents issues with rapid start/stop of TIM1.
            return;
        }

        // Determine required direction for this ISR cycle
        bool moveForward = positionDifference > 0;

        // State machine for step pulse generation
        switch (state)
        {
        case 0: // Idle/Check state: Determine if direction change or step pulse is needed
        {
            if (moveForward != _currentDirection)
            {
                // Direction change required
                _currentDirection = moveForward;
                if (moveForward)
                    GPIO_SET_DIRECTION(); // Set direction pin accordingly
                else
                    GPIO_CLEAR_DIRECTION();
                state = 1; // Move to state 1 for direction setup delay
            }
            else
            {
                // Direction is correct, initiate a step pulse
                GPIO_SET_STEP(); // Set step pin HIGH
                state = 2;       // Move to state 2 (step pin is high)
            }
        }
        break;

        case 1:        // Direction Change Delay state: Allows time for direction signal to settle before pulsing.
                       // In this simple model, it's a one-cycle delay.
            state = 0; // Next ISR call, re-evaluate (will now proceed to step if dir is set)
            break;

        case 2:                // Step Pulse High state: Step pin is currently HIGH.
            GPIO_CLEAR_STEP(); // Set step pin LOW to complete the pulse
            if (_currentDirection)
                _currentPosition++; // Increment/decrement software position counter
            else
                _currentPosition--;
            state = 0; // Return to Idle/Check state for next pulse or stop
            break;
            // No default case: Assumes states 0, 1, 2 are the only valid states.
        }
    }

    /**
     * @brief Sets the absolute target position for the stepper motor.
     * If the motor is not already running and the new target is different from the current position,
     * it starts the motor movement by enabling TimerControl.
     * @param position The absolute target position in steps.
     */
    void Stepper::setTargetPosition(int32_t position)
    {
        if (position != _currentPosition) // Only act if there's a change
        {
            _targetPosition = position;
            if (!_running) // If not already running, start the process
            {
                _running = true;
                TimerControl::start(this); // Signal TimerControl to start/resume its timer (TIM1)
            }
        }
    }

    /**
     * @brief Sets a target position relative to the current target position.
     * @param delta The number of steps to move relative to the current target.
     *              Positive for forward, negative for reverse.
     */
    void Stepper::setRelativePosition(int32_t delta)
    {
        if (delta != 0)
        {
            // New target is relative to the current _targetPosition, not _currentPosition.
            // This allows queuing of multiple relative moves.
            setTargetPosition(_targetPosition + delta);
        }
    }

    /**
     * @brief Stops the motor movement immediately.
     * Sets the running flag to false and calls TimerControl::stop() to pause the step ISR timer.
     */
    void Stepper::stop()
    {
        _running = false;
        state = 0;            // Reset ISR state machine
        GPIO_CLEAR_STEP();    // Ensure step pin is low
        TimerControl::stop(); // Pause the TimerControl timer (TIM1)
    }

    /**
     * @brief Performs an emergency stop.
     * Stops motion, disables the motor driver, and signals TimerControl for an emergency stop.
     */
    void Stepper::emergencyStop()
    {
        stop();                               // Stop normal stepping and TimerControl
        disable();                            // Disable the physical motor driver
        TimerControl::emergencyStopRequest(); // Potentially for TimerControl to handle specific e-stop logic
    }

    /**
     * @brief Enables the stepper motor driver.
     * Sets the enable pin according to the `invert_enable` configuration.
     * Resets running state.
     */
    void Stepper::enable()
    {
        if (_enabled)
            return;

        // Set enable pin based on SystemConfig's invert_enable flag
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          SystemConfig::RuntimeConfig::Stepper::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
        _enabled = true;
        _running = false; // Not running yet, just enabled
        state = 0;        // Reset ISR state
    }

    /**
     * @brief Disables the stepper motor driver.
     * Stops any motion first, then sets the enable pin to disable the driver.
     */
    void Stepper::disable()
    {
        if (!_enabled)
            return;

        stop(); // Ensure motor is stopped before disabling
        // Typically, to disable, the enable pin is set to its inactive state.
        // Assuming active-low enable, inactive is HIGH.
        // If active-high, inactive is LOW. This logic depends on the driver's ENA pin spec.
        // The HBS57 default is active-LOW, so to disable, set ENA HIGH.
        HAL_GPIO_WritePin(GPIOE, 1 << _enablePin,
                          (SystemConfig::RuntimeConfig::Stepper::invert_enable ? GPIO_PIN_RESET : GPIO_PIN_SET));
        _enabled = false;
    }

    /**
     * @brief Sets the microstepping value in the system configuration.
     * Note: This only updates the software configuration. Physical microstepping
     * must be set on the stepper driver itself (e.g., via DIP switches).
     * @param microsteps The desired microstepping value (e.g., 1, 2, 4, 8, ... 256).
     */
    void Stepper::setMicrosteps(uint32_t microsteps)
    {
        // Validate microstep values against a list of common valid ones
        static const uint32_t validMicrosteps[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
        bool isValid = false;
        for (uint32_t valid : validMicrosteps)
        {
            if (microsteps == valid)
            {
                isValid = true;
                break;
            }
        }

        if (!isValid)
        {
            // Optionally log an error or handle invalid microstep value
            return;
        }

        SystemConfig::RuntimeConfig::Stepper::microsteps = microsteps;
    }

    /**
     * @brief Initializes the GPIO pins for step, direction, and enable signals.
     * Configures pins as outputs and sets their initial states.
     */
    void Stepper::initPins()
    {
        __HAL_RCC_GPIOE_CLK_ENABLE(); // Ensure GPIO Port E clock is enabled

        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // Configure step pin (e.g., PE9)
        GPIO_InitStruct.Pin = 1 << (_stepPin & 0x0F); // Use lower 4 bits for pin number on the port
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // Push-pull output
        GPIO_InitStruct.Pull = GPIO_NOPULL;           // No pull-up or pull-down
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for fast pulses
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        // Configure direction pin (e.g., PE8)
        GPIO_InitStruct.Pin = 1 << (_dirPin & 0x0F);
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct); // Same settings as step pin

        // Configure enable pin (e.g., PE7)
        GPIO_InitStruct.Pin = 1 << (_enablePin & 0x0F);
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct); // Same settings as step pin

        // Set initial pin states
        GPIO_CLEAR_STEP();                                               // Step pin LOW
        HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET); // Direction pin LOW (default direction)
        // Enable pin state depends on whether it's active-low or active-high
        HAL_GPIO_WritePin(GPIOE, 1 << (_enablePin & 0x0F),
                          SystemConfig::RuntimeConfig::Stepper::invert_enable ? GPIO_PIN_SET : GPIO_PIN_RESET); // Initial state for enable (usually enabled if active-low)
    }

    /** @brief Sets the STEP pin HIGH. */
    void Stepper::GPIO_SET_STEP()
    {
        HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_SET);
    }

    /** @brief Sets the STEP pin LOW. */
    void Stepper::GPIO_CLEAR_STEP()
    {
        HAL_GPIO_WritePin(GPIOE, 1 << (_stepPin & 0x0F), GPIO_PIN_RESET);
    }

    /** @brief Sets the DIRECTION pin HIGH (e.g., for one direction). */
    void Stepper::GPIO_SET_DIRECTION()
    {
        HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_SET);
    }

    /** @brief Sets the DIRECTION pin LOW (e.g., for the other direction). */
    void Stepper::GPIO_CLEAR_DIRECTION()
    {
        HAL_GPIO_WritePin(GPIOE, 1 << (_dirPin & 0x0F), GPIO_PIN_RESET);
    }

    /**
     * @brief Gets the current status of the stepper motor.
     * @return StepperStatus struct containing current state information.
     */
    StepperStatus Stepper::getStatus() const
    {
        StepperStatus status;
        status.enabled = _enabled;
        status.running = _running;
        status.currentPosition = _currentPosition;
        status.targetPosition = _targetPosition;
        status.stepsRemaining = _targetPosition - _currentPosition; // More direct than abs() if sign matters
        // status.stepsRemaining = std::abs(_targetPosition - _currentPosition); // If only magnitude is needed
        return status;
    }

    /**
     * @brief Manually increments the current and target software position of the stepper.
     * Useful for re-synchronizing software position with actual position after an event like encoder overflow.
     * @param increment The amount to increment the positions by.
     */
    void Stepper::incrementCurrentPosition(int32_t increment)
    {
        // This function is intended to adjust the internal reference point.
        // For example, after an encoder overflow, the absolute encoder count wraps,
        // but the stepper's perceived absolute position should continue linearly.
        // So, we adjust both current and target to maintain the same relative distance.
        _currentPosition += increment;
        _targetPosition += increment;
    }

} // namespace STM32Step
