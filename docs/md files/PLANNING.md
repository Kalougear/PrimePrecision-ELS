# PrimePrecision ELS - Project Planning

## 1. Project Vision & Goals

The PrimePrecision Electronic Leadscrew (ELS) project aims to develop a robust, precise, and user-friendly control system for a lathe, enhancing its capabilities for threading, turning, and other machining operations.

**Overall Goals:**

- Replace manual gear changes with an electronic system for higher precision and flexibility.
- Provide intuitive user interface for selecting modes, feed rates, and thread pitches.
- Ensure reliable and safe operation.
- Support a range of common metric and imperial threads and feeds.
- Allow for future expansion with advanced features.

## 2. Architecture Overview

### 2.1. Hardware Components

- **Microcontroller:** STM32H743VIT6 series (specific model to be confirmed, e.g., STM32F103, STM32F4xx).
- **Main Spindle Encoder:** Quadrature encoder to read spindle speed and position.
- **Leadscrew Stepper Motor:** Drives the lathe carriage.
- **Stepper Motor Driver:** Interface between MCU and stepper motor (HBS57).
- **User Interface Display:** Proculus HMI (or similar) for displaying information and receiving user input.
- **Power Supply:** Appropriate power for MCU, display, and motor driver.

### 2.2. Software Components & Modules

The software is designed to be interrupt-driven and modular. Key modules include:

- **System Configuration (`src/Config`, `include/Config`):**
  - `SystemConfig.h/.cpp`: Hardware pinouts, serial port settings, core parameters.
  - `serial_debug.h`: Debugging utilities.
- **Hardware Abstraction (`src/Hardware`, `include/Hardware`):**
  - `SystemClock.h/.cpp`: MCU clock setup.
  - `EncoderTimer.h/.cpp`: Manages encoder input and RPM calculation.
  - `StepperTimer.h/.cpp`: Generates step pulses for the motor.
- **Motion Control (`src/Motion`, `include/Motion`):**
  - `MotionControl.h/.cpp`: Core logic for coordinating encoder input with stepper output.
  - `Positioning.h/.cpp`: Tracks carriage position.
  - `SyncTimer.h/.cpp`: Manages synchronization between encoder and stepper.
  - `FeedRateManager.h/.cpp`: Manages selection and calculation of feed rates. (Consolidated from `FeedTable` and `FeedTableFactory` concepts).
  - `TurningMode.h/.cpp`: Logic for manual turning operations.
  - `ThreadingMode.h/.cpp`: Logic for threading operations.
- **User Interface (`src/UI`, `include/UI`):**
  - `DisplayComm.h/.cpp`: Handles communication with the HMI (e.g., using Lumen Protocol).
  - `MenuSystem.h/.cpp`: Manages UI navigation, parameter display, and input.
- **Libraries (`lib/`):**
  - `Lumen_Protocol`: For HMI communication.
  - `STM32Step`: Stepper motor control library.
- **Main Application (`src/main.cpp`):** Initialization, main loop, interrupt handlers.

## 3. Tech Stack

- **Primary Language:** C++ (Embedded C++, likely C++11 or a version compatible with the chosen STM32 toolchain).
- **Development Environment:** PlatformIO with VSCode.
- **Microcontroller Family:** STM32 (STMH743VIT6).
- **Communication Protocol (HMI):** Lumen Protocol.
- **Key Libraries:** STM32Step, HAL/LL libraries for STM32.

## 4. Key Features (Derived from ELS_ROADMAP.md)

### Phase 1: Core Hardware Setup

- System Clock Configuration
- Debug & Display Serial Communication
- Encoder Timer (Quadrature, Direction, Filtering)
- Stepper Timer (Pulse Gen, Direction, E-Stop, Microstepping)

### Phase 2: Motion Control

- Position Tracking (Overflow, Limits)
- Synchronization System (Ratios, Thread Pitch, Feed Rate)
- Motion Modes:
  - Manual Mode
  - Threading Mode (Metric, Imperial, Multi-start)
  - Feed Mode (Constant, Reverse)

### Phase 3: User Interface

- Display Communication (Protocol, Command/Response)
- Menu System (Navigation, Parameter Editing, Mode Selection, Status)
- Configuration Storage (Flash Save/Load, Defaults)

### Phase 4: Testing & Refinement

- Validation Tests (Encoder, Step Timing, Thread Cutting, Feed Accuracy)
- Optimization (Timing, Speed, Memory)

### Phase 5: Advanced Features (Optional)

- Constant Surface Speed
- Taper Threading
- Automatic Tool Retraction
- Multiple Presets
- Emergency Recovery
- Backlash Compensation

### Specific Feature: Turning Tab (Manual Mode)

- Metric Feed Rates (0.02 mm/rev to 0.40 mm/rev, categorized)
- Imperial Feed Rates (0.0005 in/rev to 0.0100 in/rev, categorized)
- Ratio-based calculations for precision.
- UI: Current feed rate/unit, category, warnings, unit toggle.
- Navigation: Category-based, Prev/Next.
- Safety: Visual warnings, cautious rate confirmation, max feed limits.

## 5. Development Principles & Conventions

### 5.1. General Principles

- **Interrupt-Driven Design:** Avoid RTOS for simplicity and real-time performance unless absolutely necessary.
- **Reliability & Precision First:** Prioritize these over adding numerous features quickly.
- **Thorough Component Testing:** Test each component individually before integration.
- **Modularity:** Keep code organized into logical, reusable modules.
- **Clear Naming Conventions:** Use consistent and descriptive names for variables, functions, classes, and files. (To be further defined if specific project conventions exist).
- **Code Comments:** Comment non-obvious code and complex logic. Use `// Reason:` for explaining the 'why'.
- **Version Control:** Use Git for version control (assumed, as `.gitignore` exists).

### 5.2. AI Coding Assistant Workflow (Based on "Full Process for Coding with AI Coding Assistants")

- **This `PLANNING.MD` file:** Serves as the high-level vision, architecture, constraints, tech stack, and development principles. It should be referenced at the start of new development conversations.
- **`TASK.MD` file:** Tracks current tasks, backlog, sub-tasks, and items discovered during development. It will be updated as tasks are completed or new ones are identified.
- **Golden Rules Adaptation for Embedded C++:**
  - **File Size:** Aim to keep source files manageable. While the 500-line rule is a guideline, consider logical cohesion for C++ classes/modules (header/source pairs).
  - **One Task Per Interaction:** Focus requests to the AI assistant on a single, clear task.
  - **Test Early, Test Often:**
    - Unit tests for pure logic modules (e.g., `FeedRateManager` calculations) where feasible.
    - Integration tests on target hardware for hardware-dependent modules.
    - Use `serial_debug` extensively for runtime diagnostics.
  - **Specificity:** Provide clear context, expected behavior, and relevant code snippets when requesting changes or new features.
  - **Documentation:** Update `README.MD` (if one is created for the main project), this `PLANNING.MD`, and `TASK.MD` as development progresses. Add Doxygen-style comments to headers for functions and classes.
  - **Environment Variables/Secrets:** Not directly applicable in the same way as web projects, but sensitive configurations (e.g., calibration values) should be handled carefully, perhaps via a separate config file or EEPROM/Flash storage, not hardcoded if they are user-adjustable.

## 6. Core Data Structures & Modules (Examples)

- **`FeedRateManager`:**

  ```cpp
  // (Conceptual, based on docs/turning_tab_progress.md)
  // Actual implementation in src/Motion/FeedRateManager.h/.cpp
  struct FeedRate {
      double value;             // Display value
      int32_t numerator;
      int32_t denominator;
      const char* category;
      bool isMetric;
      // Potentially other fields like display string
  };

  class FeedRateManager {
  public:
      FeedRateManager();
      const FeedRate* getCurrentFeedRate() const;
      void selectNextFeedRate();
      void selectPreviousFeedRate();
      void setUnit(bool isMetric);
      // ... other methods
  private:
      // Internal storage for metric and imperial feed rate tables
      // Current selection indices
  };
  ```

- **`MotionControl`:** Central class orchestrating encoder input, calculations, and stepper output based on selected mode (Turning, Threading).
- **`MenuSystem`:** Manages UI states, receives input from HMI via `DisplayComm`, and updates HMI. Interacts with `MotionControl` and other modules to reflect and change system state.

## 7. Project Structure

(Based on current file listing)

- `.gitignore`, `CHANGELOG.md`, `platformio.ini`: Project-level files.
- `docs/`: Documentation, diagrams, datasheets.
  - `ELS_ROADMAP.md`, `PROGRESS_STATUS.md`, `turning_tab_progress.md`: Existing planning/progress docs (to be archived or integrated further).
- `include/`: Header files for the project's source code.
  - `Config/`, `Hardware/`, `Motion/`, `UI/`
- `lib/`: External libraries.
  - `Lumen_Protocol/`, `STM32Step/`
- `src/`: Source code files.
  - `Config/`, `Hardware/`, `Motion/`, `UI/`, `main.cpp`
- `test/`: Test files.

This document should be considered a living document and updated as the project evolves.
