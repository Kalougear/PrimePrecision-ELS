#include "UI/MenuSystem.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"

// Initialize static instance pointer
MenuSystem *MenuSystem::s_instance = nullptr;

MenuSystem::MenuSystem()
    : _display(nullptr),
      _motionControl(nullptr),
      _turningMode(nullptr),
      _threadingMode(nullptr),
      _currentState(MenuState::MAIN)
{
    s_instance = this;
}

MenuSystem::~MenuSystem()
{
    end();
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}

bool MenuSystem::begin(DisplayComm *display, MotionControl *motion_control)
{
    if (!display || !motion_control)
    {
        SerialDebug.println("Invalid display or motion control reference");
        return false;
    }

    _display = display;
    _motionControl = motion_control;

    // Initialize turning mode
    _turningMode = new TurningMode();
    if (!_turningMode || !_turningMode->begin(_motionControl))
    {
        SerialDebug.println("Failed to initialize turning mode");
        return false;
    }

    // Initialize threading mode
    _threadingMode = new ThreadingMode();
    if (!_threadingMode || !_threadingMode->begin(_motionControl))
    {
        SerialDebug.println("Failed to initialize threading mode");
        return false;
    }

    // Set button handler
    _display->setButtonHandler(staticButtonHandler);

    // Show main menu initially
    showMainMenu();

    return true;
}

void MenuSystem::end()
{
    // Clean up turning mode
    if (_turningMode)
    {
        _turningMode->end();
        delete _turningMode;
        _turningMode = nullptr;
    }

    // Clean up threading mode
    if (_threadingMode)
    {
        _threadingMode->end();
        delete _threadingMode;
        _threadingMode = nullptr;
    }

    _motionControl = nullptr;
    _display = nullptr;
}

void MenuSystem::showMainMenu()
{
    if (!_display)
        return;

    _display->showScreen(ScreenIDs::MAIN_SCREEN);
    _currentState = MenuState::MAIN;

    // Update status display
    _display->showStatus("Ready");
}

void MenuSystem::showTurningMenu()
{
    if (!_display)
        return;

    _display->showScreen(ScreenIDs::TURNING_SCREEN);
    _currentState = MenuState::TURNING;

    // Update turning screen fields
    updateTurningScreen();
}

void MenuSystem::showThreadingMenu()
{
    if (!_display)
        return;

    _display->showScreen(ScreenIDs::THREADING_SCREEN);
    _currentState = MenuState::THREADING;

    // Update threading screen fields
    updateThreadingScreen();
}

void MenuSystem::showSetupMenu()
{
    if (!_display)
        return;

    _display->showScreen(ScreenIDs::SETUP_SCREEN);
    _currentState = MenuState::SETUP;

    // Update setup screen fields
    updateSetupScreen();
}

void MenuSystem::updateStatus()
{
    if (!_display || !_motionControl)
        return;

    // Update RPM display on main screen and also on current functional screen
    int16_t rpm = _motionControl->getStatus().spindle_rpm;
    _display->updateText(TextIDs::RPM_VALUE, rpm);

    // Update specific screen content based on current state
    switch (_currentState)
    {
    case MenuState::TURNING:
        updateTurningScreen();
        break;

    case MenuState::THREADING:
        updateThreadingScreen();
        break;

    case MenuState::SETUP:
        // Setup screen doesn't need regular updates
        break;

    default:
        break;
    }
}

void MenuSystem::handleButtonPress(uint8_t button_id)
{
    if (!_display)
        return;

    SerialDebug.print("Handling button press: ");
    SerialDebug.println(button_id);

    // Handle global navigation buttons
    switch (button_id)
    {
    case ButtonIDs::TURNING_BTN:
        showTurningMenu();
        return;

    case ButtonIDs::THREADING_BTN:
        showThreadingMenu();
        return;

    case ButtonIDs::SETUP_BTN:
        showSetupMenu();
        return;
    }

    // Handle screen-specific buttons
    switch (_currentState)
    {
    case MenuState::TURNING:
        handleTurningButtons(button_id);
        break;

    case MenuState::THREADING:
        handleThreadingButtons(button_id);
        break;

    case MenuState::SETUP:
        handleSetupButtons(button_id);
        break;

    default:
        break;
    }
}

void MenuSystem::handleTurningButtons(uint8_t button_id)
{
    if (!_turningMode)
        return;

    switch (button_id)
    {
    case ButtonIDs::TURNING_START_BTN:
        _turningMode->start();
        _display->showStatus("Turning active");
        break;

    case ButtonIDs::TURNING_STOP_BTN:
        _turningMode->stop();
        _display->showStatus("Turning stopped");
        break;

    case ButtonIDs::TURNING_FEEDRATE_UP:
        cycleTurningFeedRate(true);
        break;

    case ButtonIDs::TURNING_FEEDRATE_DOWN:
        cycleTurningFeedRate(false);
        break;

    case ButtonIDs::TURNING_AUTOMODE_BTN:
        // Toggle between manual and semi-auto modes
        if (_turningMode->getMode() == TurningMode::Mode::MANUAL)
        {
            _turningMode->setMode(TurningMode::Mode::SEMI_AUTO);
            _display->showStatus("Semi-auto mode");
        }
        else
        {
            _turningMode->setMode(TurningMode::Mode::MANUAL);
            _display->showStatus("Manual mode");
        }
        break;

    case ButtonIDs::TURNING_SET_END_BTN:
        // Set current position as end position
        TurningMode::Position pos;
        pos.start_position = 0.0f; // Start position is always 0
        pos.end_position = _turningMode->getCurrentPosition();
        pos.valid = true;
        _turningMode->setPositions(pos);
        _display->showStatus("End position set");
        break;
    }

    updateTurningScreen();
}

void MenuSystem::updateTurningScreen()
{
    if (!_display || !_turningMode)
        return;

    // Update feed rate display
    _display->updateText(TextIDs::TURNING_FEEDRATE, _turningMode->getFeedRateValue(), 2);

    // Update position display
    float currentPos = _turningMode->getCurrentPosition();
    _display->updateText(TextIDs::TURNING_POSITION, currentPos, 2);

    // Update feed rate warning indicator
    bool warningActive = _turningMode->getCurrentFeedRateWarning();
    _display->updateText(136, warningActive ? 1 : 0); // HMI Address 136 for warning (0=off, 1=on)
}

void MenuSystem::cycleTurningFeedRate(bool increase)
{
    if (!_turningMode)
        return;

    // Get current feed rate
    TurningMode::FeedRate currentRate = _turningMode->getFeedRate();
    TurningMode::FeedRate newRate = currentRate;

    // Cycle through feed rates
    if (increase)
    {
        switch (currentRate)
        {
        case TurningMode::FeedRate::F0_01:
            newRate = TurningMode::FeedRate::F0_02;
            break;
        case TurningMode::FeedRate::F0_02:
            newRate = TurningMode::FeedRate::F0_05;
            break;
        case TurningMode::FeedRate::F0_05:
            newRate = TurningMode::FeedRate::F0_10;
            break;
        case TurningMode::FeedRate::F0_10:
            newRate = TurningMode::FeedRate::F0_20;
            break;
        case TurningMode::FeedRate::F0_20:
            newRate = TurningMode::FeedRate::F0_50;
            break;
        case TurningMode::FeedRate::F0_50:
            newRate = TurningMode::FeedRate::F1_00;
            break;
        case TurningMode::FeedRate::F1_00:
            newRate = TurningMode::FeedRate::F1_00;
            break; // Maximum
        }
    }
    else
    {
        switch (currentRate)
        {
        case TurningMode::FeedRate::F0_01:
            newRate = TurningMode::FeedRate::F0_01;
            break; // Minimum
        case TurningMode::FeedRate::F0_02:
            newRate = TurningMode::FeedRate::F0_01;
            break;
        case TurningMode::FeedRate::F0_05:
            newRate = TurningMode::FeedRate::F0_02;
            break;
        case TurningMode::FeedRate::F0_10:
            newRate = TurningMode::FeedRate::F0_05;
            break;
        case TurningMode::FeedRate::F0_20:
            newRate = TurningMode::FeedRate::F0_10;
            break;
        case TurningMode::FeedRate::F0_50:
            newRate = TurningMode::FeedRate::F0_20;
            break;
        case TurningMode::FeedRate::F1_00:
            newRate = TurningMode::FeedRate::F0_50;
            break;
        }
    }

    // Set new feed rate
    if (newRate != currentRate)
    {
        _turningMode->setFeedRate(newRate);

        char feedMsg[32];
        snprintf(feedMsg, sizeof(feedMsg), "Feed: %.2f mm/rev", _turningMode->getFeedRateValue());
        _display->showStatus(feedMsg);
    }
}

void MenuSystem::handleThreadingButtons(uint8_t button_id)
{
    if (!_threadingMode)
        return;

    switch (button_id)
    {
    case ButtonIDs::THREADING_START_BTN:
        _threadingMode->start();
        _display->showStatus("Threading active");
        break;

    case ButtonIDs::THREADING_STOP_BTN:
        _threadingMode->stop();
        _display->showStatus("Threading stopped");
        break;

    case ButtonIDs::THREADING_PITCH_UP:
        cycleThreadPitch(true);
        break;

    case ButtonIDs::THREADING_PITCH_DOWN:
        cycleThreadPitch(false);
        break;

    case ButtonIDs::THREADING_MULTI_BTN:
        toggleMultiStart();
        break;

    case ButtonIDs::THREADING_UNITS_BTN:
        toggleThreadUnits();
        break;
    }

    updateThreadingScreen();
}

void MenuSystem::updateThreadingScreen()
{
    if (!_display || !_threadingMode)
        return;

    // Get thread data
    ThreadingMode::ThreadData threadData = _threadingMode->getThreadData();

    // Update pitch display with proper units
    if (threadData.units == ThreadingMode::Units::METRIC)
    {
        char pitchText[16];
        snprintf(pitchText, sizeof(pitchText), "%.2f mm", threadData.pitch);
        _display->updateText(TextIDs::THREAD_PITCH, pitchText);
    }
    else
    {
        char pitchText[16];
        snprintf(pitchText, sizeof(pitchText), "%.1f TPI", threadData.pitch);
        _display->updateText(TextIDs::THREAD_PITCH, pitchText);
    }

    // Update starts display
    _display->updateText(TextIDs::THREAD_STARTS, threadData.starts);

    // Update position display
    float currentPos = _threadingMode->getCurrentPosition();
    _display->updateText(TextIDs::THREAD_POSITION, currentPos, 2);

    // Update thread type display
    if (threadData.type == ThreadingMode::ThreadType::STANDARD)
    {
        _display->updateText(TextIDs::THREAD_TYPE, "Standard");
    }
    else
    {
        _display->updateText(TextIDs::THREAD_TYPE, "Custom");
    }
}

void MenuSystem::cycleThreadPitch(bool increase)
{
    // Not implemented - would need thread table data
}

void MenuSystem::toggleThreadUnits()
{
    if (!_threadingMode)
        return;

    ThreadingMode::ThreadData threadData = _threadingMode->getThreadData();

    // Toggle between metric and imperial
    if (threadData.units == ThreadingMode::Units::METRIC)
    {
        threadData.units = ThreadingMode::Units::IMPERIAL;
        // Convert pitch from mm to TPI
        if (threadData.pitch > 0)
        {
            threadData.pitch = 25.4f / threadData.pitch;
        }
        _display->showStatus("Imperial threads (TPI)");
    }
    else
    {
        threadData.units = ThreadingMode::Units::METRIC;
        // Convert pitch from TPI to mm
        if (threadData.pitch > 0)
        {
            threadData.pitch = 25.4f / threadData.pitch;
        }
        _display->showStatus("Metric threads (mm)");
    }

    _threadingMode->setThreadData(threadData);
}

void MenuSystem::toggleMultiStart()
{
    if (!_threadingMode)
        return;

    ThreadingMode::ThreadData threadData = _threadingMode->getThreadData();

    // Cycle through 1, 2, 3, and 4 starts
    threadData.starts = (threadData.starts % 4) + 1;

    char startMsg[32];
    snprintf(startMsg, sizeof(startMsg), "%d-start thread", threadData.starts);
    _display->showStatus(startMsg);

    _threadingMode->setThreadData(threadData);
}

void MenuSystem::handleSetupButtons(uint8_t button_id)
{
    // Setup screen buttons not implemented yet
    updateSetupScreen();
}

void MenuSystem::updateSetupScreen()
{
    if (!_display)
        return;

    // Update leadscrew pitch display
    float leadscrewPitch = SystemConfig::RuntimeConfig::Motion::leadscrew_pitch;
    _display->updateText(TextIDs::LEADSCREW_PITCH, leadscrewPitch, 2);

    // Update microsteps display
    uint32_t microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    _display->updateText(TextIDs::MICROSTEPS, microsteps);

    // Update backlash display
    float backlash = 0.0f; // Not implemented yet
    _display->updateText(TextIDs::BACKLASH, backlash, 3);
}

// Static button handler
void MenuSystem::staticButtonHandler(uint8_t button_id)
{
    if (s_instance)
    {
        s_instance->handleButtonPress(button_id);
    }
}
