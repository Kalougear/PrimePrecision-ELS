#include "UI/HmiHandlers/ThreadingPageHandler.h"
#include "Config/serial_debug.h"
#include "c/LumenProtocol.h"
#include <string.h> // For strncpy, strstr
#include <math.h>   // For fabs

#include "Config/ThreadTable.h"              // Should contain MetricPitches and ImperialPitches
#include "Config/SystemConfig.h"             // For SystemConfig
#include "Config/Hmi/ThreadingPageOptions.h" // Should have 2 categories
#include "Motion/ThreadingMode.h"

// Define static member variables
DisplayComm *ThreadingPageHandler::_displayComm = nullptr;
ThreadingMode *ThreadingPageHandler::_threadingMode = nullptr;
MotionControl *ThreadingPageHandler::_motionControl = nullptr;                                                // Added
uint32_t ThreadingPageHandler::_lastDROUpdateTime = 0;                                                        // For Z-Pos DRO
uint8_t ThreadingPageHandler::_currentCategoryIndex = HmiThreadingPageOptions::DEFAULT_THREAD_CATEGORY_INDEX; // Should be 0
uint8_t ThreadingPageHandler::_currentPitchIndex = 0;
ThreadTable::ThreadData ThreadingPageHandler::_selectedPitchData = {"N/A", 0.0f, true}; // Placeholder name
const ThreadTable::ThreadData *ThreadingPageHandler::_activePitchList = nullptr;
size_t ThreadingPageHandler::_activePitchListSize = 0;

// Auto-stop flasher
ThreadingPageHandler::Flasher ThreadingPageHandler::_autoStopCompletionFlasher(
    HmiThreadingPageOptions::string_set_stop_disp_value_from_stm32Address,
    "REACHED!",
    250, // on time
    150  // off time
);

// Packet definitions
lumen_packet_t bool_auto_stop_enDisPacket;
lumen_packet_t string_set_stop_disp_value_to_stm32Packet;
lumen_packet_t string_set_stop_disp_value_from_stm32Packet;
lumen_packet_t bool_grab_zPacket;

// Define the HMI address for Z position string (same as Turning Tab)
const uint16_t STRING_Z_POS_ADDRESS_THREADING_DRO = 135;
// Define DRO update interval
const uint32_t HANDLER_DRO_UPDATE_INTERVAL_THREADING = 100; // Milliseconds

void ThreadingPageHandler::init(DisplayComm *displayComm, ThreadingMode *threadingMode, MotionControl *motionControl) // Added motionControl
{
    _displayComm = displayComm;
    _lastDROUpdateTime = 0; // Initialize last DRO update time
    _threadingMode = threadingMode;
    _motionControl = motionControl; // Added
    SerialDebug.println("ThreadingPageHandler: Static Initialized.");
    loadPitchesForCurrentCategoryAndSetDefault();
    if (_threadingMode && _activePitchListSize > 0)
    {
        _threadingMode->updatePitchFromHmiSelection();
    }
}

void ThreadingPageHandler::onEnterPage()
{
    // Reset to default category every time the page is entered
    _currentCategoryIndex = HmiThreadingPageOptions::DEFAULT_THREAD_CATEGORY_INDEX;

    updateCategoryDisplay();
    loadPitchesForCurrentCategoryAndSetDefault();

    if (_threadingMode)
    {
        // Reset feed direction to default (Towards Chuck / RH)
        _threadingMode->setFeedDirection(true);
        // Update HMI button visual state to match (true = 1 for RH/Towards Chuck)
        if (_displayComm)
        {
            // User feedback: HMI display for direction button state is address 210
            const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
            _displayComm->updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, true); // Send bool true for RH/Towards Chuck
            SerialDebug.println("ThreadingPageHandler: Sent RH state (true) to HMI dir display (210).");
        }
        _threadingMode->resetAutoStopRuntimeSettings();
        updateAutoStopTargetDisplay();
        _threadingMode->activate();
    }
    SerialDebug.print("ThreadingPageHandler: Entered Page. Current category: ");
    if (_currentCategoryIndex < HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        SerialDebug.println(HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex]);
    }
    else
    {
        SerialDebug.println("Invalid Category Index onEnterPage!");
    }
}

void ThreadingPageHandler::onExitPage()
{
    if (_threadingMode)
    {
        _threadingMode->deactivate();
    }
    SerialDebug.println("ThreadingPageHandler: Exited Page.");
}

void ThreadingPageHandler::handlePacket(const lumen_packet_t *packet)
{
    if (!packet)
        return;

    // Define HMI addresses for feed direction and motor enable for Threading Tab
    // User feedback: Direction is 129, Motor Enable is 149 for Threading Tab
    const uint16_t HMI_THREADING_FEED_DIRECTION_BUTTON_ADDRESS = 129;
    const uint16_t HMI_THREADING_MOTOR_ENABLE_BUTTON_ADDRESS = 149;

    if (packet->address == HmiThreadingPageOptions::bool_prev_butt_thread_catAddress && packet->type == kBool)
    {
        if (packet->data._bool) // Momentary button, react on true
        {
            selectPreviousCategory();
            SerialDebug.println("ThreadingPageHandler: Previous Category button pressed.");
        }
    }
    else if (packet->address == HmiThreadingPageOptions::bool_next_butt_thread_catAddress && packet->type == kBool)
    {
        if (packet->data._bool) // Momentary button, react on true
        {
            selectNextCategory();
            SerialDebug.println("ThreadingPageHandler: Next Category button pressed.");
        }
    }
    else if (packet->address == HmiThreadingPageOptions::bool_prev_thread_pitchAddress && packet->type == kBool)
    {
        if (packet->data._bool) // Momentary button, react on true
        {
            selectPreviousPitch();
            SerialDebug.println("ThreadingPageHandler: Previous Pitch button pressed.");
        }
    }
    else if (packet->address == HmiThreadingPageOptions::bool_next_thread_pitchAddress && packet->type == kBool)
    {
        if (packet->data._bool) // Momentary button, react on true
        {
            selectNextPitch();
            SerialDebug.println("ThreadingPageHandler: Next Pitch button pressed.");
        }
    }
    // Handle Feed Direction Toggle Button (Address 129 for Threading Tab)
    // Assuming HMI sends its toggle state as kBool (0 or 1)
    else if (packet->address == HMI_THREADING_FEED_DIRECTION_BUTTON_ADDRESS && packet->type == kBool)
    {
        if (_threadingMode)
        {
            // HMI sends true (1) for "Towards Chuck / RH", false (0) for "Away / LH" (consistent with bool)
            bool newDirectionIsTowardsChuck = packet->data._bool;
            _threadingMode->setFeedDirection(newDirectionIsTowardsChuck);
            SerialDebug.print("ThreadingPageHandler: Feed Direction input (129) processed. New state (isTowardsChuck): ");
            SerialDebug.println(newDirectionIsTowardsChuck);

            // Update HMI display for direction button state at address 210
            if (_displayComm)
            {
                const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
                // Send the boolean state directly
                _displayComm->updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, newDirectionIsTowardsChuck);
                SerialDebug.print("ThreadingPageHandler: Sent new dir state to HMI display (210): ");
                SerialDebug.println(newDirectionIsTowardsChuck ? "true" : "false");
            }
        }
    }
    // Handle Motor Enable/Disable Toggle Button (Address 149 for Threading Tab)
    // Assuming HMI sends its toggle state as kBool (0 or 1)
    else if (packet->address == HMI_THREADING_MOTOR_ENABLE_BUTTON_ADDRESS && packet->type == kBool)
    {
        if (_motionControl)
        {
            // HMI sends true (1) for "Enabled", false (0) for "Disabled" (consistent with bool)
            bool newMotorStateIsEnabled = packet->data._bool;
            if (newMotorStateIsEnabled)
            {
                _motionControl->enableMotor();
            }
            else
            {
                _motionControl->disableMotor();
            }
            SerialDebug.print("ThreadingPageHandler: Motor Enable (Addr 149) button pressed. New state (isEnabled): ");
            // SerialDebug.println(newMotorStateIsEnabled); // Original call causing "too many arguments"
            // SerialDebug.println("Motor state changed"); // Temporarily commented out due to persistent "too many arguments" error
        }
    }
    else if (packet->address == HmiThreadingPageOptions::bool_auto_stop_enDisAddress && packet->type == kBool)
    {
        if (_threadingMode)
        {
            _threadingMode->setUiAutoStopEnabled(packet->data._bool);
            updateAutoStopTargetDisplay();
        }
    }
    else if (packet->address == HmiThreadingPageOptions::string_set_stop_disp_value_to_stm32Address && packet->type == kString)
    {
        if (_threadingMode)
        {
            _threadingMode->setUiAutoStopTargetPositionFromString(packet->data._string);
            updateAutoStopTargetDisplay();
        }
    }
    else if (packet->address == HmiThreadingPageOptions::bool_grab_zAddress && packet->type == kBool)
    {
        if (packet->data._bool && _threadingMode) // React on button press (true)
        {
            _threadingMode->grabCurrentZAsUiAutoStopTarget();
            updateAutoStopTargetDisplay();
        }
    }
}

void ThreadingPageHandler::updateDRO()
{
    if (!_displayComm || !_threadingMode || !_motionControl)
    {
        // SerialDebug.println("ThreadingPageHandler::updateDRO - Error: Invalid display, threadingMode, or motionControl pointer.");
        return;
    }

    float rawPosition = _threadingMode->getCurrentPosition(); // Uses the mode's own getCurrentPosition
    char positionStr[MAX_STRING_SIZE];
    char unitStr[4] = "";
    float displayPosition = rawPosition;

    // Determine display units based on system settings
    bool isDisplayMetric = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;

    if (isDisplayMetric)
    {
        // Position from getCurrentPosition() is already in the system's display unit if that unit is metric.
        // If getCurrentPosition() returned inches and display is metric, conversion would be needed here.
        // Assuming getCurrentPosition() returns value in the correct unit for direct display or after its own conversion.
        strncpy(unitStr, " mm", sizeof(unitStr) - 1);
    }
    else // Display is Imperial
    {
        // Position from getCurrentPosition() is already in the system's display unit if that unit is imperial.
        // If getCurrentPosition() returned mm and display is imperial, conversion would be needed here.
        strncpy(unitStr, " in", sizeof(unitStr) - 1);
    }
    unitStr[sizeof(unitStr) - 1] = '\0';

    // Format the position string with 3 decimal places
    snprintf(positionStr, sizeof(positionStr) - strlen(unitStr), "%.3f", displayPosition);
    strncat(positionStr, unitStr, sizeof(positionStr) - strlen(positionStr) - 1);

    lumen_packet_t zPosPacket;
    zPosPacket.address = STRING_Z_POS_ADDRESS_THREADING_DRO; // Use defined constant 135
    zPosPacket.type = kString;
    strncpy(zPosPacket.data._string, positionStr, MAX_STRING_SIZE - 1);
    zPosPacket.data._string[MAX_STRING_SIZE - 1] = '\0';

    lumen_write_packet(&zPosPacket);
    // SerialDebug.print("ThreadingPageHandler: Sent Z_POS (135) -> "); SerialDebug.println(positionStr); // Optional
}

void ThreadingPageHandler::update()
{
    uint32_t currentTime = millis();

    // Timed DRO Update
    if (currentTime - _lastDROUpdateTime >= HANDLER_DRO_UPDATE_INTERVAL_THREADING)
    {
        updateDRO();
        _lastDROUpdateTime = currentTime;
    }

    // Handle auto-stop completion and flashing
    checkAndHandleAutoStopCompletionFlash();
    _autoStopCompletionFlasher.update();
}

const ThreadTable::ThreadData &ThreadingPageHandler::getSelectedPitchData()
{
    return _selectedPitchData;
}

void ThreadingPageHandler::selectNextCategory()
{
    _currentCategoryIndex++;
    // NUM_THREAD_CATEGORIES should be 2 now
    if (_currentCategoryIndex >= HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        _currentCategoryIndex = 0;
    }
    updateCategoryDisplay();
    loadPitchesForCurrentCategoryAndSetDefault();
    SerialDebug.print("ThreadingPageHandler: Category changed to: ");
    if (_currentCategoryIndex < HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        SerialDebug.println(HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex]);
    }
    else
    {
        SerialDebug.println("Invalid Category Index after next!");
    }
}

void ThreadingPageHandler::selectPreviousCategory()
{
    if (_currentCategoryIndex == 0)
    {
        // NUM_THREAD_CATEGORIES should be 2 now
        if (HmiThreadingPageOptions::NUM_THREAD_CATEGORIES > 0)
        {
            _currentCategoryIndex = HmiThreadingPageOptions::NUM_THREAD_CATEGORIES - 1; // Should be 1
        }
        else
        {
            _currentCategoryIndex = 0;
        }
    }
    else
    {
        _currentCategoryIndex--;
    }
    updateCategoryDisplay();
    loadPitchesForCurrentCategoryAndSetDefault();
    SerialDebug.print("ThreadingPageHandler: Category changed to: ");
    if (_currentCategoryIndex < HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        SerialDebug.println(HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex]);
    }
    else
    {
        SerialDebug.println("Invalid Category Index after previous!");
    }
}

void ThreadingPageHandler::updateCategoryDisplay()
{
    if (_currentCategoryIndex >= HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        SerialDebug.print("ThreadingPageHandler::updateCategoryDisplay - Error: _currentCategoryIndex (");
        SerialDebug.print(_currentCategoryIndex);
        SerialDebug.println(") is out of bounds!");
        return;
    }
    lumen_packet_t packet_to_send;
    packet_to_send.address = HmiThreadingPageOptions::string_thread_catAddress;
    packet_to_send.type = kString;

    strncpy(packet_to_send.data._string,
            HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex],
            MAX_STRING_SIZE - 1);
    packet_to_send.data._string[MAX_STRING_SIZE - 1] = '\0';

    lumen_write_packet(&packet_to_send);

    SerialDebug.print("ThreadingPageHandler: Sent category to HMI: ");
    SerialDebug.println(HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex]);
}

// Renamed from findPitchIndexByName to avoid conflict if old version is still linked somewhere
static int findPitchByValueAndNameHelper(const ThreadTable::ThreadData *list, size_t count, float targetPitch, const char *targetNameHint, bool isMetricTarget)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (list[i].metric != isMetricTarget)
            continue;

        if (fabs(list[i].pitch - targetPitch) < 0.001f)
        {
            if (targetNameHint == nullptr || (list[i].name && targetNameHint && strstr(list[i].name, targetNameHint) != nullptr))
            {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

void ThreadingPageHandler::loadPitchesForCurrentCategoryAndSetDefault()
{
    int default_pitch_idx_within_active_list = 0;
    bool system_is_metric = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;

    // HmiThreadingPageOptions::THREAD_CATEGORIES indices (should be 2 categories now):
    // 0: "Metric (mm)"
    // 1: "Imperial (TPI)"

    switch (_currentCategoryIndex)
    {
    case 0: // Metric Pitches (mm)
        _activePitchList = ThreadTable::MetricPitches::Threads;
        _activePitchListSize = ThreadTable::MetricPitches::COUNT;
        if (system_is_metric)
        {
            int idx = findPitchByValueAndNameHelper(_activePitchList, _activePitchListSize, 1.0f, "1.0 mm", true);
            if (idx != -1)
                default_pitch_idx_within_active_list = idx;
            else
            {
                idx = findPitchByValueAndNameHelper(_activePitchList, _activePitchListSize, 1.25f, "1.25 mm", true);
                if (idx != -1)
                    default_pitch_idx_within_active_list = idx;
            }
        }
        break;
    case 1: // Imperial Pitches (TPI)
        _activePitchList = ThreadTable::ImperialPitches::Threads;
        _activePitchListSize = ThreadTable::ImperialPitches::COUNT;
        if (!system_is_metric)
        {
            int idx = findPitchByValueAndNameHelper(_activePitchList, _activePitchListSize, 20.0f, "20 TPI", false);
            if (idx != -1)
                default_pitch_idx_within_active_list = idx;
            else
            {
                idx = findPitchByValueAndNameHelper(_activePitchList, _activePitchListSize, 16.0f, "16 TPI", false);
                if (idx != -1)
                    default_pitch_idx_within_active_list = idx;
            }
        }
        break;
    default:
        SerialDebug.print("ThreadingPageHandler::loadPitches... - Error: Invalid _currentCategoryIndex: ");
        SerialDebug.println(_currentCategoryIndex);
        // Default to Metric Pitches as a safe fallback
        _activePitchList = ThreadTable::MetricPitches::Threads;
        _activePitchListSize = ThreadTable::MetricPitches::COUNT;
        if (system_is_metric)
        {
            int idx = findPitchByValueAndNameHelper(_activePitchList, _activePitchListSize, 1.0f, "1.0 mm", true);
            if (idx != -1)
                default_pitch_idx_within_active_list = idx;
        }
        break;
    }

    _currentPitchIndex = static_cast<uint8_t>(default_pitch_idx_within_active_list);

    if (_activePitchListSize == 0)
    {
        _currentPitchIndex = 0;
        _selectedPitchData = {"Error: Empty List", 0.0f, true};
        SerialDebug.println("ThreadingPageHandler: Active pitch list is empty!");
    }
    else if (_currentPitchIndex >= _activePitchListSize)
    {
        _currentPitchIndex = 0;
        SerialDebug.println("ThreadingPageHandler: Default pitch index out of bounds for active list, defaulting to 0.");
        if (_activePitchListSize > 0)
        {
            _selectedPitchData = _activePitchList[_currentPitchIndex];
        }
        else
        {
            _selectedPitchData = {"Error: List Empty", 0.0f, true};
        }
    }
    else
    {
        _selectedPitchData = _activePitchList[_currentPitchIndex];
    }

    updatePitchDisplay();

    if (_threadingMode)
    {
        _threadingMode->updatePitchFromHmiSelection();
    }

    SerialDebug.print("ThreadingPageHandler: Loaded pitches for category '");
    if (_currentCategoryIndex < HmiThreadingPageOptions::NUM_THREAD_CATEGORIES)
    {
        SerialDebug.print(HmiThreadingPageOptions::THREAD_CATEGORIES[_currentCategoryIndex]);
    }
    else
    {
        SerialDebug.print("Invalid Category Index!");
    }
    SerialDebug.print("'. Active list size: ");
    SerialDebug.print(_activePitchListSize);
    SerialDebug.print(". Current pitch index (in active list): ");
    SerialDebug.print(_currentPitchIndex);
    SerialDebug.print(" -> Pitch: '");
    if (_activePitchListSize > 0 && _currentPitchIndex < _activePitchListSize && _selectedPitchData.name != nullptr)
    {
        SerialDebug.print(_selectedPitchData.name);
    }
    else
    {
        SerialDebug.print("N/A");
    }
    SerialDebug.print("', Value: ");
    SerialDebug.print(_selectedPitchData.pitch);
    SerialDebug.println(_selectedPitchData.metric ? " mm" : " TPI");
}

void ThreadingPageHandler::selectNextPitch()
{
    if (_activePitchListSize == 0)
    {
        SerialDebug.println("selectNextPitch: No active pitches in list, returning.");
        return;
    }

    SerialDebug.print("selectNextPitch: Before: _currentCategoryIndex=");
    SerialDebug.print(_currentCategoryIndex);
    SerialDebug.print(", _currentPitchIndex (in active list)=");
    SerialDebug.print(_currentPitchIndex);
    SerialDebug.print(", active_list_size=");
    SerialDebug.println(_activePitchListSize);

    uint8_t initialPitchIndex = _currentPitchIndex;
    float initialPitchValue = _selectedPitchData.pitch;

    do
    {
        _currentPitchIndex++;
        if (_currentPitchIndex >= _activePitchListSize)
        {
            _currentPitchIndex = 0;
        }
        if (_currentPitchIndex == initialPitchIndex && _activePitchListSize > 1)
        {
            break;
        }
    } while (fabs(_activePitchList[_currentPitchIndex].pitch - initialPitchValue) < 0.001f && _activePitchListSize > 1);

    if (_currentPitchIndex < _activePitchListSize)
    {
        _selectedPitchData = _activePitchList[_currentPitchIndex];
    }
    else
    {
        SerialDebug.println("selectNextPitch: Error - _currentPitchIndex out of bounds after loop!");
        _currentPitchIndex = 0;
        if (_activePitchListSize > 0)
            _selectedPitchData = _activePitchList[_currentPitchIndex];
    }

    if (_threadingMode)
    {
        _threadingMode->updatePitchFromHmiSelection();
    }

    updatePitchDisplay();
    SerialDebug.print("ThreadingPageHandler: Pitch changed to index (in active list): ");
    SerialDebug.println(_currentPitchIndex);
}

void ThreadingPageHandler::selectPreviousPitch()
{
    if (_activePitchListSize == 0)
    {
        SerialDebug.println("selectPreviousPitch: No active pitches in list, returning.");
        return;
    }
    SerialDebug.print("selectPreviousPitch: Before: _currentCategoryIndex=");
    SerialDebug.print(_currentCategoryIndex);
    SerialDebug.print(", _currentPitchIndex (in active list)=");
    SerialDebug.print(_currentPitchIndex);
    SerialDebug.print(", active_list_size=");
    SerialDebug.println(_activePitchListSize);

    uint8_t initialPitchIndex = _currentPitchIndex;
    float initialPitchValue = _selectedPitchData.pitch;

    do
    {
        if (_currentPitchIndex == 0)
        {
            if (_activePitchListSize > 0)
            {
                _currentPitchIndex = _activePitchListSize - 1;
            }
            else
            {
                _currentPitchIndex = 0;
            }
        }
        else
        {
            _currentPitchIndex--;
        }
        if (_currentPitchIndex == initialPitchIndex && _activePitchListSize > 1)
        {
            break;
        }
    } while (fabs(_activePitchList[_currentPitchIndex].pitch - initialPitchValue) < 0.001f && _activePitchListSize > 1);

    if (_currentPitchIndex < _activePitchListSize)
    {
        _selectedPitchData = _activePitchList[_currentPitchIndex];
    }
    else
    {
        SerialDebug.println("selectPreviousPitch: Error - _currentPitchIndex out of bounds after loop!");
        _currentPitchIndex = 0;
        if (_activePitchListSize > 0)
            _selectedPitchData = _activePitchList[_currentPitchIndex];
    }

    if (_threadingMode)
    {
        _threadingMode->updatePitchFromHmiSelection();
    }

    updatePitchDisplay();
    SerialDebug.print("ThreadingPageHandler: Pitch changed to index (in active list): ");
    SerialDebug.println(_currentPitchIndex);
}

void ThreadingPageHandler::updatePitchDisplay()
{
    char pitch_string_buffer[MAX_STRING_SIZE];

    if (_activePitchListSize == 0 || _currentPitchIndex >= _activePitchListSize)
    {
        strncpy(pitch_string_buffer, "---", MAX_STRING_SIZE - 1);
        pitch_string_buffer[MAX_STRING_SIZE - 1] = '\0';
    }
    else
    {
        if (_selectedPitchData.name != nullptr)
        {
            if (_selectedPitchData.metric)
            {
                snprintf(pitch_string_buffer, MAX_STRING_SIZE, "%.2f mm", _selectedPitchData.pitch);
            }
            else
            {
                snprintf(pitch_string_buffer, MAX_STRING_SIZE, "%.0f TPI", _selectedPitchData.pitch);
            }
        }
        else
        {
            strncpy(pitch_string_buffer, "ERR", MAX_STRING_SIZE - 1);
            pitch_string_buffer[MAX_STRING_SIZE - 1] = '\0';
        }
    }
    pitch_string_buffer[MAX_STRING_SIZE - 1] = '\0';

    lumen_packet_t packet_to_send;
    packet_to_send.address = HmiThreadingPageOptions::string_thread_pitchAddress;
    packet_to_send.type = kString;
    strncpy(packet_to_send.data._string, pitch_string_buffer, MAX_STRING_SIZE - 1);
    packet_to_send.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet_to_send);

    SerialDebug.print("ThreadingPageHandler: Sent pitch to HMI: ");
    SerialDebug.println(pitch_string_buffer);
}

void ThreadingPageHandler::updateAutoStopTargetDisplay()
{
    if (!_displayComm || !_threadingMode)
    {
        return;
    }
    String formattedTarget = _threadingMode->getFormattedUiAutoStopTarget();
    _displayComm->updateText(HmiThreadingPageOptions::string_set_stop_disp_value_from_stm32Address, formattedTarget.c_str());
}

void ThreadingPageHandler::checkAndHandleAutoStopCompletionFlash()
{
    if (_threadingMode && _threadingMode->isAutoStopCompletionPendingHmiSignal())
    {
        _threadingMode->clearAutoStopCompletionHmiSignal();
        _autoStopCompletionFlasher.start();
    }
}

void ThreadingPageHandler::Flasher::update()
{
    if (!active)
    {
        return;
    }

    uint32_t elapsed = millis() - startTime;
    uint32_t cycleTime = onTime + offTime;
    uint8_t currentCycle = elapsed / cycleTime;

    if (currentCycle >= 3) // Flash 3 times
    {
        stop();
        // After flashing, restore the original display
        ThreadingPageHandler::updateAutoStopTargetDisplay();
        return;
    }

    bool isOn = (elapsed % cycleTime) < onTime;
    if (isOn)
    {
        _displayComm->updateText(address, message);
    }
    else
    {
        // Show blank during off-time
        _displayComm->updateText(address, "");
    }
}
