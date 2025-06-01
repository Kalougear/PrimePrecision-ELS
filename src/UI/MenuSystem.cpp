#include "UI/MenuSystem.h"
#include "Config/serial_debug.h"
#include "Config/SystemConfig.h"
#include "Config/HmiInputOptions.h" // Ensure this is included

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

    _turningMode = new TurningMode();
    if (!_turningMode || !_turningMode->begin(_motionControl))
    {
        SerialDebug.println("Failed to initialize turning mode");
        delete _turningMode;
        _turningMode = nullptr;
        return false;
    }

    _threadingMode = new ThreadingMode();
    if (!_threadingMode || !_threadingMode->begin(_motionControl))
    {
        SerialDebug.println("Failed to initialize threading mode");
        delete _threadingMode;
        _threadingMode = nullptr;
        delete _turningMode;
        _turningMode = nullptr; // Clean up previous allocation
        return false;
    }

    _display->setPacketHandler(staticPacketHandler);
    showMainMenu(); // Initial screen
    return true;
}

void MenuSystem::end()
{
    if (_turningMode)
    {
        // if (_turningMode->isRunning()) _turningMode->deactivate(); // Or just end()
        _turningMode->end();
        delete _turningMode;
        _turningMode = nullptr;
    }
    if (_threadingMode)
    {
        if (_threadingMode->isRunning())
            _threadingMode->deactivate();
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

    if (_currentState == MenuState::THREADING && _threadingMode)
    {
        _threadingMode->deactivate();
    }
    else if (_currentState == MenuState::TURNING && _turningMode)
    {
        // _turningMode->deactivate(); // Assuming TurningMode gets this
    }
    // Add deactivation for other ELS modes if they exist

    _display->showScreen(ScreenIDs::MAIN_SCREEN);
    _currentState = MenuState::MAIN;
    _display->showStatus("Ready");
}

void MenuSystem::showTurningMenu()
{
    if (!_display)
        return;

    if (_currentState == MenuState::THREADING && _threadingMode)
    {
        _threadingMode->deactivate();
    }
    // Add deactivation for other ELS modes if they exist

    // Set initial visual state of the mm/inch button on the Turning Tab
    // (This HMI specific logic might be better in TurningPageHandler::onEnterPage)
    bool is_system_metric = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;
    lumen_packet_t initialDisplayPacket;
    initialDisplayPacket.address = HmiInputOptions::ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI;
    initialDisplayPacket.type = kBool;
    initialDisplayPacket.data._bool = is_system_metric;
    lumen_write_packet(&initialDisplayPacket);
    SerialDebug.print("showTurningMenu: Sent initial mm/inch state to HMI addr ");
    SerialDebug.println(HmiInputOptions::ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI);

    _display->showScreen(ScreenIDs::TURNING_SCREEN);
    _currentState = MenuState::TURNING;

    // if (_turningMode) _turningMode->activate(); // Assuming TurningMode gets this

    updateTurningScreen();
}

void MenuSystem::showThreadingMenu()
{
    if (!_display)
        return;

    if (_currentState == MenuState::TURNING && _turningMode)
    {
        // _turningMode->deactivate(); // Assuming TurningMode gets this
    }
    // Add deactivation for other ELS modes if they exist

    _display->showScreen(ScreenIDs::THREADING_SCREEN);
    _currentState = MenuState::THREADING;

    if (_threadingMode)
    {
        _threadingMode->activate(); // This calls updatePitchFromHmiSelection and start()
    }
    updateThreadingScreen(); // This might be redundant if activate() updates HMI via handler
}

void MenuSystem::showSetupMenu()
{
    if (!_display)
        return;

    if (_currentState == MenuState::THREADING && _threadingMode)
    {
        _threadingMode->deactivate();
    }
    else if (_currentState == MenuState::TURNING && _turningMode)
    {
        // _turningMode->deactivate(); // Assuming TurningMode gets this
    }
    // Add deactivation for other ELS modes if they exist

    _display->showScreen(ScreenIDs::SETUP_SCREEN);
    _currentState = MenuState::SETUP;
    updateSetupScreen();
}

void MenuSystem::updateStatus()
{
    if (!_display || !_motionControl)
        return;
    int16_t rpm = _motionControl->getStatus().spindle_rpm;
    _display->updateText(TextIDs::RPM_VALUE, rpm);

    switch (_currentState)
    {
    case MenuState::TURNING:
        updateTurningScreen();
        break;
    case MenuState::THREADING:
        updateThreadingScreen();
        break;
    case MenuState::SETUP:
        break; // Setup screen doesn't need regular updates
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
        TurningMode::Position pos;
        pos.start_position = 0.0f;
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
    char feedRateStr[20];
    float feedValue = _turningMode->getFeedRateValue();
    const char *units = _turningMode->getFeedRateIsMetric() ? "mm/rev" : "inch/rev";
    if (!_turningMode->getFeedRateIsMetric() && feedValue < 1.0f)
    {
        snprintf(feedRateStr, sizeof(feedRateStr), "%.4f %s", feedValue, units);
    }
    else
    {
        snprintf(feedRateStr, sizeof(feedRateStr), "%.2f %s", feedValue, units);
    }
    _display->updateText(TextIDs::TURNING_FEEDRATE, feedRateStr);
    float currentPos = _turningMode->getCurrentPosition();
    _display->updateText(TextIDs::TURNING_POSITION, currentPos, 2);
    bool warningActive = _turningMode->getCurrentFeedRateWarning();
    _display->updateText(136, warningActive ? 1 : 0);
}

void MenuSystem::cycleTurningFeedRate(bool increase)
{
    if (!_turningMode)
        return;
    if (increase)
        _turningMode->selectNextFeedRate();
    else
        _turningMode->selectPreviousFeedRate();
    char feedMsg[40];
    float feedValue = _turningMode->getFeedRateValue();
    const char *units = _turningMode->getFeedRateIsMetric() ? "mm/rev" : "inch/rev";
    if (!_turningMode->getFeedRateIsMetric() && feedValue < 1.0f)
    {
        snprintf(feedMsg, sizeof(feedMsg), "Feed: %.4f %s", feedValue, units);
    }
    else
    {
        snprintf(feedMsg, sizeof(feedMsg), "Feed: %.2f %s", feedValue, units);
    }
    _display->showStatus(feedMsg);
}

void MenuSystem::handleThreadingButtons(uint8_t button_id)
{
    if (!_threadingMode)
        return;
    switch (button_id)
    {
    case ButtonIDs::THREADING_START_BTN: // This button might not exist per user feedback
        // _threadingMode->start(); // If it does, this is what it would do.
        _display->showStatus("Threading active (Manual Start)"); // Adjust if no button
        break;
    case ButtonIDs::THREADING_STOP_BTN: // This button might not exist
        // _threadingMode->stop(); // If it does.
        _display->showStatus("Threading stopped (Manual Stop)");
        break;
        // Pitch up/down, multi-start, units are handled by ThreadingPageHandler now
        // case ButtonIDs::THREADING_PITCH_UP: cycleThreadPitch(true); break;
        // case ButtonIDs::THREADING_PITCH_DOWN: cycleThreadPitch(false); break;
        // case ButtonIDs::THREADING_MULTI_BTN: toggleMultiStart(); break;
        // case ButtonIDs::THREADING_UNITS_BTN: toggleThreadUnits(); break;
    }
    updateThreadingScreen();
}

void MenuSystem::updateThreadingScreen()
{
    if (!_display || !_threadingMode)
        return;
    // This function might become simpler if ThreadingPageHandler handles all HMI updates for its page
    // For now, keep basic updates.
    ThreadingMode::ThreadData threadData = _threadingMode->getThreadData();
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
    _display->updateText(TextIDs::THREAD_STARTS, threadData.starts);
    float currentPos = _threadingMode->getCurrentPosition();
    _display->updateText(TextIDs::THREAD_POSITION, currentPos, 2);
    if (threadData.type == ThreadingMode::ThreadType::STANDARD)
    {
        _display->updateText(TextIDs::THREAD_TYPE, "Standard");
    }
    else
    {
        _display->updateText(TextIDs::THREAD_TYPE, "Custom");
    }
}

void MenuSystem::cycleThreadPitch(bool increase) { /* Not implemented - handled by ThreadingPageHandler */ }
void MenuSystem::toggleThreadUnits() { /* Not implemented - handled by ThreadingPageHandler */ }
void MenuSystem::toggleMultiStart() { /* Not implemented - handled by ThreadingPageHandler */ }

void MenuSystem::handleSetupButtons(uint8_t button_id) { updateSetupScreen(); }

void MenuSystem::updateSetupScreen()
{
    if (!_display)
        return;
    float leadscrewPitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    _display->updateText(TextIDs::LEADSCREW_PITCH, leadscrewPitch, 2);
    uint32_t microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    _display->updateText(TextIDs::MICROSTEPS, microsteps);
    float backlash = 0.0f;
    _display->updateText(TextIDs::BACKLASH, backlash, 3);
}

void MenuSystem::staticPacketHandler(const lumen_packet_t *packet)
{
    if (s_instance && packet)
    {
        uint8_t button_id = static_cast<uint8_t>(packet->address);
        SerialDebug.print("MenuSystem received packet. Addr as button_id: ");
        SerialDebug.println(button_id);
        s_instance->handleButtonPress(button_id);
    }
}
