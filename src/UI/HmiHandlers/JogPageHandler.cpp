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
    static bool _isJoggingLeft = false;
    static bool _isJoggingRight = false;
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
        if (speedChanged && _motionControlInstance->isJogActive())
        {
            MotionControl::JogDirection currentDirection = MotionControl::JogDirection::JOG_NONE;
            if (_isJoggingLeft)
            {
                currentDirection = MotionControl::JogDirection::JOG_TOWARDS_CHUCK;
            }
            else if (_isJoggingRight)
            {
                currentDirection = MotionControl::JogDirection::JOG_AWAY_FROM_CHUCK;
            }

            if (currentDirection != MotionControl::JogDirection::JOG_NONE)
            {
                _motionControlInstance->beginContinuousJog(currentDirection, _currentJogSpeedMmPerMin);
            }
        }
    }

    void handlePacket(const lumen_packet_t *packet)
    {
        if (!packet || !_motionControlInstance)
        {
            return;
        }

        bool canJog = SystemConfig::RuntimeConfig::System::jog_system_enabled;

        // Update state based on incoming packets
        if (packet->address == HmiJogPageOptions::bool_jog_leftAddress && packet->type == kBool)
        {
            _isJoggingLeft = packet->data._bool;
        }
        else if (packet->address == HmiJogPageOptions::bool_jog_rightAddress && packet->type == kBool)
        {
            _isJoggingRight = packet->data._bool;
        }
        else if (packet->address == HmiJogPageOptions::int_prev_next_jog_speedAddress && (packet->type == kS32 || packet->type == kBool))
        {
            handleJogSpeedSelection(packet->data._s32);
        }
        else if (packet->address == HmiJogPageOptions::bool_jog_system_enableAddress && packet->type == kBool)
        {
            SystemConfig::RuntimeConfig::System::jog_system_enabled = !packet->data._bool;
            // If jog is disabled while active, the logic below will stop it.
        }

        // Now, determine the action based on the current state
        if (canJog && _isJoggingLeft && !_isJoggingRight)
        {
            _motionControlInstance->beginContinuousJog(MotionControl::JogDirection::JOG_TOWARDS_CHUCK, _currentJogSpeedMmPerMin);
        }
        else if (canJog && _isJoggingRight && !_isJoggingLeft)
        {
            _motionControlInstance->beginContinuousJog(MotionControl::JogDirection::JOG_AWAY_FROM_CHUCK, _currentJogSpeedMmPerMin);
        }
        else
        {
            // Stop if both buttons are pressed, neither is pressed, or jog is disabled
            if (_motionControlInstance->isJogActive())
            {
                _motionControlInstance->endContinuousJog();
            }
        }
    }

    void onExitPage()
    {
        // SAFETY: Stop any active jog when exiting the page
        if (_motionControlInstance && _motionControlInstance->isJogActive())
        {
            _motionControlInstance->endContinuousJog();
        }
        _isJoggingLeft = false;
        _isJoggingRight = false;

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
