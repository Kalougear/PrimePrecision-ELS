# PrimePrecision ELS - Task List

## Current Sprint / Focus (Derived from `ELS_ROADMAP.md` Current Focus & `PROGRESS_STATUS.md` - REVISED AFTER CODE REVIEW)

- **[x] CRITICAL FIX #1: Resolve `SyncTimer` Dependency Initialization.**
  - [x] Design decision: How `MotionControl` provides its `_encoder` and `_stepper` instances to its `_syncTimer` member (e.g., pass via `SyncTimer::begin()`, dedicated setters, or constructor). (Decision: Pass via `SyncTimer::begin()`)
  - [x] Implement the chosen mechanism in `SyncTimer.h/.cpp` and `MotionControl.cpp`. (Code implemented. Initial testing shows `SyncTimer` now receives dependencies and calls `_stepper->setRelativePosition()`, but no motor movement observed yet - problem likely in `STM32Step`.)
- **[x] CRITICAL FIX #2: Debug and Resolve `STM32Step` Library Issues for Motion.** (Stepper now follows encoder with `MotionControl`)
  - [x] **Sub-Task: Debug `Stepper::ISR()` Execution:** (ISR confirmed to be running; excessive debug prints were removed as they caused timing issues).
  - [x] **Sub-Task: Verify Stepper Enable Logic & Wiring:** (`invert_enable` setting confirmed correct; physical wiring assumed OK as `working_1to1_ratio.cpp` test works).
  - [x] **Sub-Task: Analyze `STM32Step::TimerControl::init()` for `Stepper::ISR()`:** (`TimerControl::init()` confirmed compatible and is now correctly called by `MotionControl::begin()`).
  - [x] **Key Finding & Fix (Initialization Order):** Resolved motion issues by ensuring `EncoderTimer` (TIM2) is initialized _before_ `STM32Step::TimerControl` (TIM1). Implemented by adding a global `EncoderTimer` instance in `main.cpp` and calling its `begin()` method prior to `MotionControl::begin()`.
  - [x] **Key Finding (Debug Overhead):** Minimized serial debug prints in low-level libraries (`STM32Step`, `SyncTimer`) as excessive printing caused timing disruptions and erratic motor behavior.
  - [x] **IMPROVEMENT: `SyncTimer` - ISR-based Processing:** Refactored `SyncTimer` to move core synchronization logic (encoder reading, step calculation with fractional accumulation, stepper commanding via `setRelativePosition`) into its hardware timer ISR (`handleInterrupt`). This provides precise command timing, immune to main loop load, resulting in "ultra smooth" motion. Removed `syncRequested` flag and `processSyncRequest()` method. `MotionControl::update()` no longer calls `processSyncRequest()`.
  - [ ] **Sub-Task: Unify Step Generation & Complete Interface (Longer Term):** (Software bit-banging is now functional. Hardware PWM remains a future consideration.)
    - [ ] Clarify and unify the step generation mechanism in `STM32Step` (software bit-bang via `Stepper::ISR` vs. hardware PWM via the other `TimerControl.cpp` version). Prefer hardware PWM if feasible.
    - [ ] Ensure `Stepper.h` declares all necessary public methods, including speed control (`setSpeed`, `setMaxSpeed`, `getCurrentSpeed`, `getTargetSpeed`) if PWM mode is chosen/restored.
    - [ ] Define/clarify `STM32Step::RuntimeConfig` and ensure it's consistently used. (Currently using global `SystemConfig` primarily; local `STM32Step::RuntimeConfig` from PWM test not integrated).
    - [ ] Reconcile the two apparent versions of `TimerControl.cpp` logic. (`timer_base.cpp` with software stepping is in use; PWM version from test is separate).
- **[x] REFACTOR: `EncoderTimer` - Remove Redundant Sync Logic.**
  - [x] Remove `SyncConfig`, `SyncPosition` structs and related members (`_syncConfig`, `_lastSyncCount`) from `EncoderTimer`.
  - [x] Remove methods `setSyncConfig()`, `getSyncPosition()`, `calculateStepperFrequency()`, `calculateRequiredSteps()`, `calculateSyncRatio()` from `EncoderTimer`.
  - [x] Ensure `EncoderTimer` focuses solely on providing encoder count, RPM, and direction. (`const` correctness applied to `getCount()`, `getRPM()`, etc.).
- **[x] IMPROVEMENT: `EncoderTimer` - Respect `invert_direction` for RPM Display.**
  - [x] Modified `EncoderTimer::calculateRPM()` to use `SystemConfig::RuntimeConfig::Encoder::invert_direction` flag, allowing correct RPM sign display.
- **[x] IMPROVEMENT: `SyncTimer` - Fractional Step Accumulation.** (Discovered and resolved during debugging, now part of ISR processing)
  - [x] Logic to accumulate floating-point `requiredSteps` is integrated into `SyncTimer::handleInterrupt()`.
  - [x] Added `_accumulatedFractionalSteps` and `_isr_lastEncoderCount` members to `SyncTimer`.
  - [x] This ensures smooth motor response even with small encoder deltas (e.g., slow encoder turning).
- **[ ] REFACTOR: `TurningMode` to use `FeedRateManager` as Single Source of Truth.** (Now unblocked)
  - [ ] Modifying `TurningMode::setFeedRate()` to interact with `_feedRateManager` (e.g., to select a feed rate by value or index).
  - [ ] Modifying `TurningMode::configureFeedRate()` to use `_feedRateManager.getCurrentRatio()` for precise `MotionControl` setup.
  - [ ] Removing or reconciling `TurningMode`'s internal `FeedRate` enum with `FeedRateManager`'s tables.
  - [ ] Ensuring `TurningMode` correctly updates/uses `_feedRateManager`'s `isMetric` state.
- **[ ] UI: Enhance `MenuSystem` for `TurningMode` Feed Rate Control.** (Depends on `TurningMode` refactor)
  - [ ] Modify `MenuSystem::cycleTurningFeedRate()` to call methods on the refactored `_turningMode` that correctly interact with `_feedRateManager`'s full tables (metric & imperial).
  - [ ] Implement UI logic in `MenuSystem::handleTurningButtons()` for "MM/Inch" unit selection for turning feed rates, ensuring it calls a method in `TurningMode` that updates `_feedRateManager.setMetric()`. (Logic for this button not yet present in MenuSystem.cpp)
  - [ ] Ensure `MenuSystem::updateTurningScreen()` displays feed rate information sourced reliably from the refactored `TurningMode`.
- **[x] Motion Control: Synchronization Core Logic.** (Depends on `SyncTimer` fix, `EncoderTimer` refactor, and `STM32Step` fixes for full functionality)
  - [x] `SyncTimer` calculates required steps based on `thread_pitch / leadscrew_pitch` ratio. (Core calculation logic present)
  - [x] `MotionControl` configures `SyncTimer` with these pitches. (Mechanism present)
  - [ ] Integrate `FeedRateManager`'s `numerator`/`denominator` (via refactored `TurningMode/ThreadingMode` into `MotionControl::Config::thread_pitch`) into motion control logic for precise movement. (This part is pending refactors)
- **[x] Motion Control: Threading Calculations Core Logic.** (Depends on `SyncTimer` fix and `STM32Step` fixes for full functionality)
  - [x] `ThreadingMode` calculates effective pitch (lead) from `ThreadData` (value, units, starts). (Core calculation logic present)
  - [x] `ThreadingMode` configures `MotionControl` with this effective pitch. (Mechanism present)
- **[ ] UI: Develop simple UI navigation on the Proculus display (General).**
  - [x] `MenuSystem` handles screen switching (`showMainMenu`, `showTurningMenu`, etc. via `DisplayComm`). (Core logic present)
  - [x] `MenuSystem` handles button presses and delegates to mode-specific handlers. (Core logic present)
  - [ ] Enhance feed rate selection UI (general, beyond turning mode specifics above):
    - [ ] Implement category-based feed rate navigation (from `PROGRESS_STATUS.md` & `turning_tab_progress.md`) if not covered by `TurningMode` cycling.
    - [ ] Add visual indicators for current feed rate category.
- **[ ] Bugfix: Resolve compilation errors in feed rate system (from `PROGRESS_STATUS.md`).** (Re-evaluate after refactors; no obvious compilation errors seen in reviewed core logic, may refer to older states or specific `main.cpp` test setups)
  - [ ] Fix variable type conflicts / redeclarations.
  - [ ] Ensure clean integration with Proculus display.
- **[ ] Feed Rate Display: Complete functionality (from `PROGRESS_STATUS.md`).** (Re-evaluate after refactors)
  - [ ] Verify unit switching (Metric/Imperial) is fully functional through `MenuSystem -> TurningMode -> FeedRateManager`. (Unit switching for Turning Mode in MenuSystem is pending)
  - [ ] Implement prev/next button logic fully through `MenuSystem -> TurningMode -> FeedRateManager`. (Currently uses TurningMode's old enum)
  - [ ] Add error handling and edge case management for feed rate display.
- **[ ] Turning Tab (Manual Mode): Z-Axis Control Implementation (from `turning_tab_progress.md`)**
  - [x] `TurningMode::getCurrentPosition()` exists for DRO. (HMI display part is UI work)
  - [ ] Implement Z-Axis Position Display (DRO) on HMI (ensure HMI address `11` / `TextIDs::TURNING_POSITION` is correctly displaying the value from `TurningMode::getCurrentPosition()`).
  - [ ] Implement "Towards Chuck" / "Away from Chuck" manual jog buttons (requires HMI button definitions, `ButtonIDs`, and corresponding logic in `MotionControl`, `TurningMode`, and `MenuSystem`. Consider if `StepperTimer` class is used here).
  - [ ] Implement "Zero Carriage" button for Z-axis (requires HMI button definition, `ButtonID`, and corresponding logic in `MotionControl`, `TurningMode`, and `MenuSystem`).
- **[ ] Turning Tab (Manual Mode): Cautious Feed Warning (from `turning_tab_progress.md`)**
  - [x] `TurningMode::getCurrentFeedRateWarning()` exists and calls `_feedRateManager.getCurrentWarning()`. (Effectiveness depends on `TurningMode` refactor for sync with `_feedRateManager` state).
  - [ ] HMI visual implementation for "cautious" feed rate warning (linked to HMI address `136` via `DisplayComm` and `TextIDs`).
- **[ ] Safety Features: Implement (from `PROGRESS_STATUS.md` & `turning_tab_progress.md`).**
  - [ ] Add warnings for aggressive feed rates (visual part, ensuring it reflects `FeedRateManager`'s state via refactored `TurningMode`).
  - [ ] Implement maximum feed rate limits (STM32 logic).

## Backlog - Phase 1: Core Hardware Setup (from `ELS_ROADMAP.md`)

- **[x] Encoder Timer: Direction Detection Improvement.** (`EncoderTimer::getPosition()` provides direction via `__HAL_TIM_IS_TIM_COUNTING_DOWN`).
- **[ ] Encoder Timer: Noise Filtering.** (`EncoderTimer` HAL init uses filter value `0xF`, `SystemConfig` allows runtime filter level).
- **[ ] Stepper Timer: Clarify role of `StepperTimer.h/.cpp` wrapper class.** (Is it needed for manual jogging if `MotionControl` handles ELS modes? If so, ensure no conflicts with `MotionControl`'s stepper instance).
- **[x] Stepper Timer: Emergency Stop Function.** (`MotionControl::emergencyStop()` and `StepperTimer::emergencyStop()` exist, calling `_stepper->emergencyStop()`).
- **[x] Stepper Timer: Microstepping Control.** (`MotionControl` and `StepperTimer` can set microsteps on `STM32Step::Stepper` instance, which uses `SystemConfig`).

## Backlog - Phase 2: Motion Control (from `ELS_ROADMAP.md`)

- **[ ] Position Tracking: Overflow/Underflow Handling.** (`working_1to1_ratio.cpp` test has an example, main code needs robust implementation if 32-bit counter isn't sufficient for all cases).
- **[x] Position Tracking: Position Limits.** (`Positioning.cpp` provides `hasReachedEndPosition`, used by `TurningMode` and `ThreadingMode`).
- **[x] Synchronization System: Configurable Gear Ratios (partially covered by current focus).** (Core logic in `SyncTimer` and `MotionControl::setConfig` exists).
- **[x] Synchronization System: Thread Pitch Calculations (partially covered by current focus).** (Core logic in `ThreadingMode` and `MotionControl::setConfig` exists).
- **[x] Synchronization System: Feed Rate Control (partially covered by current focus).** (Core logic in `TurningMode` (needs refactor for `FeedRateManager`) and `MotionControl::setConfig` exists).
- **[ ] Motion Modes: Manual Mode (largely covered by Turning Tab, but ensure all aspects from roadmap are addressed).** (`TurningMode` with `Mode::MANUAL` exists).
- **[x] Motion Modes: Threading Mode** (Core `ThreadingMode` class logic exists)
  - [x] Metric Threading. (`ThreadingMode` handles metric pitch).
  - [x] Imperial Threading. (`ThreadingMode` converts TPI to metric pitch).
  - [x] Multi-start Threads. (`ThreadingMode` adjusts effective pitch for starts).
- **[ ] Motion Modes: Feed Mode (distinct from Manual Turning feeds if applicable)** (`MotionControl::Mode::FEEDING` exists, maps to `STM32Step::OperationMode::TURNING`. Logic might be similar to Turning mode).
  - [ ] Constant Feed Rates.
  - [ ] Reverse Feed.

## Backlog - Phase 3: User Interface (from `ELS_ROADMAP.md`)

- **[x] Basic Display Communication: Command Processing (refine if needed).** (`DisplayComm::processIncoming` handles single-byte button IDs).
- **[x] Basic Display Communication: Response Handling (refine if needed).** (Not much "response" from HMI beyond button presses in current `DisplayComm`).
- **[ ] Menu System: Main Menu Design.** (`MenuSystem::showMainMenu` exists, HMI design itself is external).
- **[ ] Menu System: Parameter Editing (general, beyond feed rates).**
- **[ ] Menu System: Mode Selection Interface (between Manual, Threading, Feed).**
- **[ ] Menu System: Status Display (general system status).**
- **[ ] Configuration Storage:**
  - [ ] Save Settings to Flash.
  - [ ] Load Settings on Boot.
  - [ ] Default Parameter Handling.
- **[ ] Turning Tab: Semi-Auto mode design (from `turning_tab_progress.md`).**
- **[ ] Turning Tab: Job mode planning (from `turning_tab_progress.md`).**

## Backlog - Phase 4: Testing & Refinement (from `ELS_ROADMAP.md` & others)

- **[ ] Validation Tests:**
  - [ ] Encoder Accuracy Test.
  - [ ] Step Timing Verification.
  - [ ] Thread Cutting Test Piece.
  - [ ] Feed Accuracy Measurement.
- **[ ] Optimization:**
  - [ ] Timing Refinement.
  - [ ] Speed Improvements.
  - [ ] Memory Usage Optimization.
- **[ ] Testing: Unit testing for `FeedRateManager` (from `PROGRESS_STATUS.md`).**
- **[ ] Testing: Integration testing with display (from `PROGRESS_STATUS.md`).**
- **[ ] Testing: Verify behavior across different scenarios (from `PROGRESS_STATUS.md`).**
- **[ ] Testing: Comprehensive testing framework for UI interactions and motion control accuracy (from `turning_tab_progress.md`).**
- **[ ] Documentation: Detailed documentation of final UI states, HMI variable addresses, and communication flow (from `turning_tab_progress.md`).**
  - _Operational Note: HMI elements at addresses 132, 134, and 136 (if display-only) should have "auto-upload on change" disabled in HMI editor to prevent serial spam._

## Backlog - Phase 5: Advanced Features (Optional - from `ELS_ROADMAP.md`)

- [ ] Constant Surface Speed
- [ ] Taper Threading
- [ ] Automatic Tool Retraction
- [ ] Multiple Presets
- [ ] Emergency Recovery
- [ ] Backlash Compensation

## Known Issues (from `PROGRESS_STATUS.md`)

- **[ ] Potential display communication inconsistencies.** (Monitor and address as specific instances arise).
  - This might be related to the "auto-upload on change" note for HMI elements.

## Completed (For Reference - derived from `ELS_ROADMAP.md`, `turning_tab_progress.md`, `PROGRESS_STATUS.md`)

- **Core Hardware Setup:**
  - [x] Configure System Clock
  - [x] Configure Debug Serial (PA2/PA3)
  - [x] Configure Display Serial (PA9/PA10)
  - [x] Encoder Timer: Basic Timer Configuration
  - [x] Encoder Timer: Quadrature Decoding
  - [x] Stepper Timer: Basic Pulse Generation
  - [x] Stepper Timer: Direction Control
- **Motion Control:**
  - [x] Position Tracking: Basic Position Reading
  - [x] Synchronization System: Basic 1:1 Ratio
- **User Interface:**
  - [x] Basic Display Communication: Serial Protocol Setup
- **Turning Tab / Feed Rate System (Manual Mode):**
  - [x] Feed rate categorization.
  - [x] Basic data structure design for feed rates (evolved into `FeedRateManager`).
  - [x] HMI Control Communication Debugging & Resolution (Manual Mode) - common ground, HMI "auto send" enabled for buttons.
  - [x] Feed table implementation: `metricFeedRates[]` and `imperialFeedRates[]` populated in `FeedRateManager.cpp`.
  - [x] `FeedRateManager` class implemented (managing selection, cycling, unit switching, default rates). (Code review confirms internal logic is sound).
  - [x] HMI Display Logic for Feed Rate (Manual Mode) - value/unit string (addr `132`), category string (addr `134`). (This refers to `main.cpp` Lumen test. `FeedRateManager` can provide this data; UI path via `MenuSystem/TurningMode` using Nextion-like protocol needs refactor to fully utilize `FeedRateManager`).
  - [x] `MAX_STRING_SIZE` in Lumen Protocol increased to 40 (Relevant for `main.cpp` Lumen test).
  - [x] Backend logic for "cautious" feed rate warning signal (via HMI address `136`) implemented (HMI visual pending). (This refers to `main.cpp` Lumen test using global `FeedRateManager`. `TurningMode`'s use of its `_feedRateManager.getCurrentWarning()` depends on its refactor for synchronization).
- **Hardware Setup Progress:**
  - [x] Display Communication: Basic serial communication established (Verified for both Lumen test in `main.cpp` and Nextion-like in `DisplayComm`).
  - [x] Display Communication: RPM display working correctly (Verified for `main.cpp` Lumen test; `MenuSystem` also updates RPM via `DisplayComm`).
  - [x] Encoder System: Basic position reading (Verified via `EncoderTimer.cpp`).
  - [x] Encoder System: RPM calculation (Verified via `EncoderTimer.cpp`).
  - [x] Encoder System: Feed rate tracking (initial - this likely refers to `main.cpp`'s direct use of `FeedRateManager` with encoder for display, not full motion sync).
- **Testing Status (Partial):**
  - [x] RPM display: Functioning (as per `main.cpp` and `MenuSystem` capabilities).

---

**Code Review Notes (Summary - May 15, 2025):**

- **`main.cpp` (in `src/`):** Identified as a testbed for HMI (Lumen Protocol) and direct `FeedRateManager` interaction. Not the primary application structure using `MenuSystem`. The final application `main.cpp` will need to instantiate and run `MenuSystem`.
- **`DisplayComm.cpp`:** Uses a Nextion-like protocol. This is the HMI communication layer for `MenuSystem`.
- **`STM32Step` Library (in `lib/`):**
  - **CRITICAL INTERNAL INCONSISTENCIES:** Contains conflicting step generation logic (software bit-bang in `Stepper::ISR` vs. hardware PWM in `TimerControl.cpp` from `working_test_timer_without dma.cpp`).
  - **INCOMPLETE INTERFACE:** `Stepper.h` is missing speed control method declarations (`setSpeed`, etc.) that are used by `TimerControl.cpp` (PWM version) and test files.
  - **UNCLEAR CONFIG:** `STM32Step::RuntimeConfig` used in tests is not clearly defined/scoped.
  - This library needs significant review and unification before it can be reliably used by `MotionControl`.
- **`SyncTimer.cpp`:** CRITICAL - Missing mechanism to receive `EncoderTimer*` and `STM32Step::Stepper*` references from `MotionControl`. All sync motion via `MotionControl` is blocked until this is fixed.
- **`EncoderTimer.cpp`:** Contains redundant sync logic (config, ratio/step calculation) that conflicts with `SyncTimer`'s role. Should be refactored. `getCount()`/`getRPM()` should be `const`.
- **`TurningMode.cpp`:** CRITICAL - Does not correctly use its internal `_feedRateManager`. Relies on its own limited enum for feed rates. Needs major refactor.
- **`MenuSystem.cpp`:** Interacts with `TurningMode` using its limited enum. Needs updates post-`TurningMode` refactor. MM/Inch unit switching for Turning Mode feeds is not implemented.
- **`FeedRateManager.cpp`, `MotionControl.cpp` (core logic), `ThreadingMode.cpp`, `Positioning.cpp`, `SystemConfig.cpp`:** These modules appear internally mostly sound for their intended roles, but their overall effectiveness and integration depend heavily on the critical issues in `SyncTimer`, `STM32Step`, `EncoderTimer`, and `TurningMode` being resolved.
- **`StepperTimer.cpp` (wrapper class):** Its role alongside `MotionControl`'s direct `STM32Step::Stepper` usage needs clarification, especially if manual jogging is a required feature. Potential for conflicting control over stepper pins if not managed carefully.

This `TASK.MD` should be updated regularly. When a task is started, consider moving it to "Current Sprint /Focus". Mark tasks with `[x]` when completed and move them to a "Recently Completed" or archive section periodically. Add new tasks as they are discovered.
