# PrimePrecision ELS Development Roadmap

## Phase 1: Core Hardware Setup ⚙️
- [x] Configure System Clock
- [x] Configure Debug Serial (PA2/PA3)
- [x] Configure Display Serial (PA9/PA10)
- [ ] Set Up Encoder Timer
  - [x] Basic Timer Configuration
  - [x] Quadrature Decoding
  - [ ] Direction Detection Improvement
  - [ ] Noise Filtering
- [ ] Set Up Stepper Timer
  - [x] Basic Pulse Generation
  - [x] Direction Control
  - [ ] Emergency Stop Function
  - [ ] Microstepping Control

## Phase 2: Motion Control 🔄
- [ ] Position Tracking
  - [x] Basic Position Reading
  - [ ] Overflow/Underflow Handling
  - [ ] Position Limits
- [ ] Synchronization System
  - [x] Basic 1:1 Ratio
  - [ ] Configurable Gear Ratios
  - [ ] Thread Pitch Calculations
  - [ ] Feed Rate Control
- [ ] Motion Modes
  - [ ] Manual Mode
  - [ ] Threading Mode
    - [ ] Metric Threading
    - [ ] Imperial Threading
    - [ ] Multi-start Threads
  - [ ] Feed Mode
    - [ ] Constant Feed Rates
    - [ ] Reverse Feed

## Phase 3: User Interface 🖥️
- [ ] Basic Display Communication
  - [x] Serial Protocol Setup
  - [ ] Command Processing
  - [ ] Response Handling
- [ ] Menu System
  - [ ] Main Menu Design
  - [ ] Parameter Editing
  - [ ] Mode Selection Interface
  - [ ] Status Display
- [ ] Configuration Storage
  - [ ] Save Settings to Flash
  - [ ] Load Settings on Boot
  - [ ] Default Parameter Handling

## Phase 4: Testing & Refinement 🧪
- [ ] Validation Tests
  - [ ] Encoder Accuracy Test
  - [ ] Step Timing Verification
  - [ ] Thread Cutting Test Piece
  - [ ] Feed Accuracy Measurement
- [ ] Optimization
  - [ ] Timing Refinement
  - [ ] Speed Improvements
  - [ ] Memory Usage Optimization

## Phase 5: Advanced Features (Optional) 🌟
- [ ] Constant Surface Speed
- [ ] Taper Threading
- [ ] Automatic Tool Retraction
- [ ] Multiple Presets
- [ ] Emergency Recovery
- [ ] Backlash Compensation

---

## Current Focus 🎯
- Complete encoder/stepper synchronization with configurable ratios
- Implement and test basic threading calculations
- Develop simple UI navigation on the Proculus display

## Development Notes 📝
- Keep the design interrupt-driven rather than RTOS-based
- Focus on reliability and precision before adding features
- Test each component thoroughly before integration