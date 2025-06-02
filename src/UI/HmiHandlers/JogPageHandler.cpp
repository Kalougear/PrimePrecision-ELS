#include "UI/HmiHandlers/JogPageHandler.h"
#include "Config/Hmi/JogPageOptions.h" // For Jog control HMI addresses and options
#include "Config/SystemConfig.h"       // For SystemConfig::RuntimeConfig and HmiParameters
#include "Motion/MotionControl.h"      // For MotionControl class and JogDirection
#include "LumenProtocol.h"             // For lumen_write_packet, kDataType, etc.
#include <HardwareSerial.h>            // For HardwareSerial type (SerialDebug)
#include <stdio.h>                     // For snprintf
#include <string.h>                    // For strncpy, strncat

extern HardwareSerial SerialDebug; // Declare SerialDebug as extern

namespace JogPageHandler
{
    // Static variables for Jog Control state
    static MotionControl *_motionControlInstance = nullptr;
    static int _currentJogSpeedIndex = 0;
    static float _currentJogSpeedMmPerMin = 0.0f;
    static MotionControl::JogDirection _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE;
    static bool _jogSpeedChanged = false; // Flag to track if jog speed was changed

    // Helper function to update jog speed display on HMI
    void sendJogSpeedDisplay()
    {
        if (!_motionControlInstance)
            return;

        char speedStr[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH];
        float displaySpeed = _currentJogSpeedMmPerMin;

        // Cap display speed by the configured Max Jog Speed for display consistency
        // MotionControl will handle actual capping during beginContinuousJog.
        float maxJogSpeed = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
        if (displaySpeed > maxJogSpeed)
        {
            displaySpeed = maxJogSpeed;
        }

        // Determine units based on system measurement unit
        const char *units = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? " mm/min" : " in/min";
        if (!SystemConfig::RuntimeConfig::System::measurement_unit_is_metric)
        {
            displaySpeed /= 25.4f; // Convert mm/min to in/min for display
        }

        snprintf(speedStr, sizeof(speedStr) - strlen(units), "%.1f", displaySpeed);
        strncat(speedStr, units, sizeof(speedStr) - strlen(speedStr) - 1);

        lumen_packet_t packet;
        packet.address = HmiJogPageOptions::string_display_jog_current_speed_valueAddress;
        packet.type = kString;
        strncpy(packet.data._string, speedStr, SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH - 1);
        packet.data._string[SystemConfig::HmiParameters::MAX_HMI_STRING_LENGTH - 1] = '\0';
        lumen_write_packet(&packet);

        // SerialDebug.print("JogPageHandler: Sent Jog Speed Display (187) -> ");
        // SerialDebug.println(speedStr);
    }

    void init(MotionControl *mcInstance)
    {
        // SerialDebug.println("JogPageHandler initialized.");
        _motionControlInstance = mcInstance;
        _jogSpeedChanged = false; // Initialize the flag

        if (_motionControlInstance)
        {
            // SerialDebug.println("JogPageHandler: MotionControl instance received.");
            // Initialize default jog speed index from SystemConfig
            _currentJogSpeedIndex = SystemConfig::RuntimeConfig::System::default_jog_speed_index;

            if (HmiJogPageOptions::NUM_JOG_SPEEDS > 0)
            {
                // Validate the loaded index
                if (_currentJogSpeedIndex < 0 || _currentJogSpeedIndex >= HmiJogPageOptions::NUM_JOG_SPEEDS)
                {
                    // SerialDebug.print("JogPageHandler: WARNING - Invalid default_jog_speed_index (");
                    // SerialDebug.print(_currentJogSpeedIndex);
                    // SerialDebug.println("), defaulting to 0.");
                    _currentJogSpeedIndex = 0;
                }
                _currentJogSpeedMmPerMin = HmiJogPageOptions::JOG_SPEEDS_MM_PER_MIN[_currentJogSpeedIndex];

                // Cap initial speed by Max Jog Speed
                float maxJogSpeed = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
                if (_currentJogSpeedMmPerMin > maxJogSpeed)
                {
                    _currentJogSpeedMmPerMin = maxJogSpeed;
                    // SerialDebug.print("JogPageHandler: Initial jog speed capped by Max Jog Speed to: ");
                    // SerialDebug.println(_currentJogSpeedMmPerMin);
                }
            }
            else
            {
                _currentJogSpeedMmPerMin = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min; // Fallback to max jog speed
                _currentJogSpeedIndex = -1;                                                               // Indicate no valid index from list
                // SerialDebug.println("JogPageHandler: WARNING - JOG_SPEEDS_MM_PER_MIN is empty, using max_jog_speed_mm_per_min.");
            }
            // SerialDebug.print("JogPageHandler: Initial Jog Speed Index: ");
            // SerialDebug.print(_currentJogSpeedIndex);
            // SerialDebug.print(", Speed: ");
            // SerialDebug.println(_currentJogSpeedMmPerMin);
        }
        else
        {
            // SerialDebug.println("JogPageHandler: WARNING - MotionControl instance is NULL in init.");
        }
    }

    void onEnterPage()
    {
        // SerialDebug.println("JogPageHandler: onEnterPage - Sending initial HMI values for Jog Page.");
        sendJogSpeedDisplay(); // Send initial jog speed display
        // Future: Send other Jog Page specific initial values (e.g., Max Jog Speed setting, Jog System Enable state)
    }

    void handleJogSpeedSelection(int32_t buttonValue)
    {
        if (!_motionControlInstance || HmiJogPageOptions::NUM_JOG_SPEEDS == 0)
        {
            // SerialDebug.println("JogPageHandler: Cannot handle jog speed selection - MC null or no speeds defined.");
            return;
        }

        bool speedChanged = false;
        if (buttonValue == HmiJogPageOptions::JOG_SPEED_CMD_PREV) // Prev
        {
            _currentJogSpeedIndex--;
            if (_currentJogSpeedIndex < 0)
            {
                _currentJogSpeedIndex = HmiJogPageOptions::NUM_JOG_SPEEDS - 1; // Wrap around
            }
            _currentJogSpeedMmPerMin = HmiJogPageOptions::JOG_SPEEDS_MM_PER_MIN[_currentJogSpeedIndex];
            // Cap selected speed by Max Jog Speed
            float maxJogSpeed = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
            if (_currentJogSpeedMmPerMin > maxJogSpeed)
            {
                _currentJogSpeedMmPerMin = maxJogSpeed;
            }
            // SerialDebug.print("JogPageHandler: Jog speed PREV. New speed (capped): ");
            // SerialDebug.println(_currentJogSpeedMmPerMin);
            sendJogSpeedDisplay();
            speedChanged = true;
            _jogSpeedChanged = true; // Set flag when speed changes
        }
        else if (buttonValue == HmiJogPageOptions::JOG_SPEED_CMD_NEXT) // Next
        {
            _currentJogSpeedIndex++;
            if (_currentJogSpeedIndex >= HmiJogPageOptions::NUM_JOG_SPEEDS)
            {
                _currentJogSpeedIndex = 0; // Wrap around
            }
            _currentJogSpeedMmPerMin = HmiJogPageOptions::JOG_SPEEDS_MM_PER_MIN[_currentJogSpeedIndex];
            // Cap selected speed by Max Jog Speed
            float maxJogSpeed = SystemConfig::RuntimeConfig::Z_Axis::max_jog_speed_mm_per_min;
            if (_currentJogSpeedMmPerMin > maxJogSpeed)
            {
                _currentJogSpeedMmPerMin = maxJogSpeed;
            }
            // SerialDebug.print("JogPageHandler: Jog speed NEXT. New speed (capped): ");
            // SerialDebug.println(_currentJogSpeedMmPerMin);
            sendJogSpeedDisplay();
            speedChanged = true;
            _jogSpeedChanged = true; // Set flag when speed changes
        }
        // If buttonValue is 0 (release), do nothing to the speed.

        // If a jog is currently active and speed changed, update its speed
        if (speedChanged && _motionControlInstance->isJogActive() && _currentJogDirectionCmd != MotionControl::JogDirection::JOG_NONE)
        {
            // SerialDebug.println("JogPageHandler: Jog active, re-initiating with new speed.");
            // MotionControl::beginContinuousJog will internally cap the speed with max_jog_speed_mm_per_min
            _motionControlInstance->beginContinuousJog(_currentJogDirectionCmd, _currentJogSpeedMmPerMin);
        }
    }

    void handlePacket(const lumen_packet_t *packet)
    {
        if (!packet || !_motionControlInstance)
        {
            // SerialDebug.println("JogPageHandler::handlePacket - Error: Invalid packet or MotionControl instance pointer.");
            return;
        }

        // SerialDebug.print("JogPageHandler: RX Addr ");
        // SerialDebug.print(packet->address);

        // Check master jog system enable flag first for any jog related commands
        bool canJog = SystemConfig::RuntimeConfig::System::jog_system_enabled;
        if (packet->address == HmiJogPageOptions::bool_jog_leftAddress || packet->address == HmiJogPageOptions::bool_jog_rightAddress)
        {
            if (!canJog)
            {
                // SerialDebug.println(" -> Jog system is disabled. Ignoring jog command.");
                // Ensure any active jog is stopped if system gets disabled during a jog press
                if (_currentJogDirectionCmd != MotionControl::JogDirection::JOG_NONE)
                {
                    _motionControlInstance->endContinuousJog();
                    _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE;
                }
                return; // Do not process further if jog system is disabled
            }
            // RPM check (temporarily disabled as per original code)
            // SerialDebug.println(" -> RPM check for jog temporarily disabled for debugging.");
            // MotionControl::Status mcStatus = _motionControlInstance->getStatus();
            // if (mcStatus.spindle_rpm != 0) {
            //     // SerialDebug.print(" -> Spindle RPM is ");
            //     // SerialDebug.print(mcStatus.spindle_rpm);
            //     // SerialDebug.println(". Jogging inhibited.");
            //     if (_currentJogDirectionCmd != MotionControl::JogDirection::JOG_NONE) {
            //         _motionControlInstance->endContinuousJog();
            //         _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE;
            //     }
            //     return;
            // }
        }

        if (packet->address == HmiJogPageOptions::bool_jog_leftAddress) // Address 185
        {
            if (packet->type == kBool)
            {
                bool jogLeftPressed = packet->data._bool;
                SerialDebug.print("JogPageHandler: RX Addr 185 (Jog Left), Value: ");
                SerialDebug.println(jogLeftPressed ? "TRUE (Pressed)" : "FALSE (Released)");

                if (jogLeftPressed)
                {
                    _currentJogDirectionCmd = MotionControl::JogDirection::JOG_TOWARDS_CHUCK;
                    _motionControlInstance->beginContinuousJog(_currentJogDirectionCmd, _currentJogSpeedMmPerMin);
                }
                else // Jog Left Released
                {
                    if (_currentJogDirectionCmd == MotionControl::JogDirection::JOG_TOWARDS_CHUCK)
                    {
                        _motionControlInstance->endContinuousJog();
                        _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE;
                    }
                }
            }
            else
            {
                // SerialDebug.print(" -> WRONG TYPE for Jog Left (expected Bool, got ");
                // SerialDebug.print(packet->type);
                // SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiJogPageOptions::bool_jog_rightAddress) // Address 186
        {
            if (packet->type == kBool)
            {
                bool jogRightPressed = packet->data._bool;
                SerialDebug.print("JogPageHandler: RX Addr 186 (Jog Right), Value: ");
                SerialDebug.println(jogRightPressed ? "TRUE (Pressed)" : "FALSE (Released)");

                if (jogRightPressed)
                {
                    _currentJogDirectionCmd = MotionControl::JogDirection::JOG_AWAY_FROM_CHUCK;
                    _motionControlInstance->beginContinuousJog(_currentJogDirectionCmd, _currentJogSpeedMmPerMin);
                }
                else // Jog Right Released
                {
                    if (_currentJogDirectionCmd == MotionControl::JogDirection::JOG_AWAY_FROM_CHUCK)
                    {
                        _motionControlInstance->endContinuousJog();
                        _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE;
                    }
                }
            }
            else
            {
                // SerialDebug.print(" -> WRONG TYPE for Jog Right (expected Bool, got ");
                // SerialDebug.print(packet->type);
                // SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiJogPageOptions::int_prev_next_jog_speedAddress) // Address 194
        {
            if (packet->type == kS32 || packet->type == kBool)
            { // HMI sends kS32
                int32_t speedCmdValue = packet->data._s32;
                // SerialDebug.print(" -> Jog Speed P/N (194) value: ");
                // SerialDebug.println(speedCmdValue);
                handleJogSpeedSelection(speedCmdValue);
            }
            else
            {
                // SerialDebug.print(" -> WRONG TYPE for Jog Speed P/N (expected S32/Bool, got ");
                // SerialDebug.print(packet->type);
                // SerialDebug.println(")");
            }
        }
        // Future: Handle Max Jog Speed input from HMI if added
        else if (packet->address == HmiJogPageOptions::bool_jog_system_enableAddress) // Address 195
        {
            if (packet->type == kBool)
            {
                bool hmiSignalState = packet->data._bool; // True if HMI button indicates "enable", false for "disable"
                // If HMI signal is inverted relative to system logic:
                SystemConfig::RuntimeConfig::System::jog_system_enabled = !hmiSignalState;

                // SerialDebug.print(" -> Jog System Enable HMI Signal (195): ");
                // SerialDebug.print(hmiSignalState ? "ON (requesting enable)" : "OFF (requesting disable)");
                // SerialDebug.print(" -> System Jog Actual State: ");
                // SerialDebug.println(SystemConfig::RuntimeConfig::System::jog_system_enabled ? "ENABLED" : "DISABLED");

                // If jog system is NOW disabled (SystemConfig... == false) while a jog is active, stop the jog.
                if (!SystemConfig::RuntimeConfig::System::jog_system_enabled && _motionControlInstance && _motionControlInstance->isJogActive())
                {
                    // SerialDebug.println("JogPageHandler: Jog system disabled while jog active. Stopping jog.");
                    _motionControlInstance->endContinuousJog();
                    _currentJogDirectionCmd = MotionControl::JogDirection::JOG_NONE; // Reset direction
                }
                // Optionally, send the state back to HMI if there's a display element for it
            }
            else
            {
                // SerialDebug.print(" -> WRONG TYPE for Jog System Enable (expected Bool, got ");
                // SerialDebug.print(packet->type);
                // SerialDebug.println(")");
            }
        }
        else
        {
            // SerialDebug.println(" -> Not handled by JogPageHandler logic.");
        }
    }

    void onExitPage()
    {
        if (_currentJogSpeedIndex >= 0 && _currentJogSpeedIndex < HmiJogPageOptions::NUM_JOG_SPEEDS)
        {
            // Update the RAM value with the last selected jog speed index
            uint8_t new_ram_index = static_cast<uint8_t>(_currentJogSpeedIndex);
            if (SystemConfig::RuntimeConfig::System::default_jog_speed_index != new_ram_index)
            {
                // This condition might be redundant if _jogSpeedChanged already covers it,
                // but ensures RAM is updated if _jogSpeedChanged was somehow missed.
                // However, _jogSpeedChanged should be the primary trigger for marking dirty.
            }
            SystemConfig::RuntimeConfig::System::default_jog_speed_index = new_ram_index;
            // SerialDebug.print("JogPageHandler: onExitPage - Updated RAM default_jog_speed_index to: ");
            // SerialDebug.println(SystemConfig::RuntimeConfig::System::default_jog_speed_index);

            // If the jog speed was changed during this session, mark it as dirty for the next "Save All"
            if (_jogSpeedChanged)
            {
                SystemConfig::RuntimeConfigDirtyFlags::System::default_jog_speed_index = true;
                // SerialDebug.println("JogPageHandler: onExitPage - Marked default_jog_speed_index as dirty.");
                _jogSpeedChanged = false; // Reset flag for the next Jog Page session
            }
            else
            {
                // SerialDebug.println("JogPageHandler: onExitPage - Jog speed not changed this session, dirty flag not set.");
            }
        }
        else
        {
            // SerialDebug.println("JogPageHandler: onExitPage - Not updating RAM/dirty flag for jog speed index (current index invalid or list empty).");
        }
        // EEPROM save is now handled by "Save All Parameters" button via dirty flags.
    }

} // namespace JogPageHandler
