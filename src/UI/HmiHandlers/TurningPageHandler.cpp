#include <stdint.h> // Explicitly include for uint16_t
#include "UI/HmiHandlers/TurningPageHandler.h"
#include "Config/HmiInputOptions.h" // For ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI etc.
#include "Config/SystemConfig.h"    // For SystemConfig::RuntimeConfig
// #include "Config/Hmi/TurningPageOptions.h" // This might be empty or not needed if all options are general
#include "LumenProtocol.h"      // For lumen_write_packet, kBool, kString, lumen_packet_t, MAX_STRING_SIZE
#include <HardwareSerial.h>     // For HardwareSerial type
#include "UI/DisplayComm.h"     // For DisplayComm class
#include "Motion/TurningMode.h" // For TurningMode class
// #include "Motion/MotionControl.h"       // Not directly needed by TurningPageHandler if jog is separate
#include <stdio.h>                         // For snprintf
#include <string.h>                        // For strncpy, strncat
#include "Config/Hmi/TurningPageOptions.h" // For auto-stop HMI addresses

extern HardwareSerial SerialDebug; // Declare SerialDebug as extern

namespace TurningPageHandler
{
    // Static variables for non-blocking "REACHED" flashing
    static bool _isFlashingTargetReached = false;
    static uint8_t _flashStateCount = 0; // Counts ON/OFF states
    static unsigned long _lastFlashToggleTimeMs = 0;
    static bool _flashMessageIsVisible = false;
    const unsigned long FLASH_STATE_DURATION_MS = 250; // Duration for one state (ON or OFF)
    const uint8_t TOTAL_FLASH_STATES = 6;              // 3 ON states, 3 OFF states for 3 visible flashes

    // Define the HMI address for Z position string as requested by user (specific to Turning Page DRO)
    const uint16_t STRING_Z_POS_ADDRESS_TURNING_DRO = 135; // Renamed to avoid conflict if JogPage also uses 135 for something else

    // Static variables for timing DRO and RPM updates within the handler
    static uint32_t lastDroUpdateTimeMs_Handler = 0;
    const uint32_t HANDLER_DRO_UPDATE_INTERVAL = 100; // Milliseconds for responsive DRO
    static uint32_t lastRpmUpdateTimeMs_Handler = 0;
    const uint32_t HANDLER_RPM_UPDATE_INTERVAL = 200; // Milliseconds for RPM, matching DRO

    // Helper function to update all feed rate related displays on the Turning Tab
    void sendTurningPageFeedDisplays(TurningMode *turningMode)
    {
        if (!turningMode)
        {
            SerialDebug.println("TurningPageHandler::sendTurningPageFeedDisplays - Error: Invalid turningMode pointer.");
            return;
        }

        lumen_packet_t packet;

        // 1. Update MM/Inch Display (Address 191)
        bool is_metric = turningMode->getFeedRateIsMetric();
        packet.address = HmiInputOptions::ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI; // 191
        packet.type = kBool;
        packet.data._bool = !is_metric; // HMI 191: false = "mm/rev" (Metric), true = "inch/rev" (Imperial)
        lumen_write_packet(&packet);
        SerialDebug.print("TPH: Sent MM/Inch disp (191) -> ");
        SerialDebug.println(packet.data._bool ? "INCH" : "MM");

        // 2. Update Feed Rate Value Display (Address 132)
        char feedRateStr[MAX_STRING_SIZE];
        turningMode->getFeedRateManager().getDisplayString(feedRateStr, sizeof(feedRateStr));

        packet.address = HmiInputOptions::ADDR_TURNING_FEED_RATE_VALUE_DISPLAY; // 132
        packet.type = kString;
        strncpy(packet.data._string, feedRateStr, MAX_STRING_SIZE - 1);
        packet.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&packet);
        SerialDebug.print("TPH: Sent Feed Value disp (132) -> ");
        SerialDebug.println(feedRateStr);

        // 3. Update Feed Rate Description/Category Display (Address 134)
        const char *category = turningMode->getFeedRateCategory();
        packet.address = HmiInputOptions::ADDR_TURNING_FEED_RATE_DESC_DISPLAY; // 134
        packet.type = kString;
        strncpy(packet.data._string, category ? category : "", MAX_STRING_SIZE - 1);
        packet.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&packet);
        SerialDebug.print("TPH: Sent Feed Desc disp (134) -> ");
        SerialDebug.println(category ? category : "N/A");

        SerialDebug.flush();
    }

    // init function now only takes TurningMode, as MotionControl was for jog.
    void init(TurningMode *turningModeInstance)
    {
        SerialDebug.println("TurningPageHandler initialized.");
        if (turningModeInstance)
        {
            SerialDebug.println("TurningPageHandler: TurningMode instance received.");
        }
        else
        {
            SerialDebug.println("TurningPageHandler: WARNING - TurningMode instance is NULL in init.");
        }
    }

    void onEnterPage(TurningMode *turningModeInstance)
    {
        SerialDebug.println("TurningPageHandler: onEnterPage called.");
        if (turningModeInstance)
        {
            // Reset feed direction to default (Towards Chuck / RH)
            turningModeInstance->setFeedDirection(true);
            SerialDebug.println("TPH: onEnterPage - Set feed direction to Towards Chuck (true).");

            turningModeInstance->activate(); // Activate TurningMode
            SerialDebug.println("TPH: onEnterPage - Called turningModeInstance->activate().");

            // Reset auto-stop state in TurningMode and update HMI (still relevant after activate)
            turningModeInstance->resetAutoStopRuntimeSettings();

            lumen_packet_t autoStopEnablePacket;
            autoStopEnablePacket.address = HmiTurningPageOptions::bool_auto_stop_enDisAddress;
            autoStopEnablePacket.type = kBool;
            autoStopEnablePacket.data._bool = turningModeInstance->isUiAutoStopEnabled();
            lumen_write_packet(&autoStopEnablePacket);
            SerialDebug.print("TPH: Sent initial Auto-Stop Enable state (145) -> ");
            SerialDebug.println(autoStopEnablePacket.data._bool);

            lumen_packet_t autoStopTargetPacket;
            autoStopTargetPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
            autoStopTargetPacket.type = kString;
            String formattedTarget = turningModeInstance->getFormattedUiAutoStopTarget();
            strncpy(autoStopTargetPacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
            autoStopTargetPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&autoStopTargetPacket);
            SerialDebug.print("TPH: Sent initial Auto-Stop Target (197) -> ");
            SerialDebug.println(formattedTarget);

            sendTurningPageFeedDisplays(turningModeInstance);

            lumen_packet_t motorEnablePacket;
            motorEnablePacket.address = HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE; // 149
            motorEnablePacket.type = kBool;
            motorEnablePacket.data._bool = turningModeInstance->isMotorEnabled();
            lumen_write_packet(&motorEnablePacket);
            SerialDebug.print("TPH: Sent initial Motor Enable state (149) -> ");
            SerialDebug.println(motorEnablePacket.data._bool);
        }
        else
        {
            SerialDebug.println("TurningPageHandler::onEnterPage - Error: turningModeInstance is NULL.");
        }
    }

    void onExitPage(TurningMode *turningModeInstance)
    {
        SerialDebug.println("TurningPageHandler: onExitPage called.");
        if (turningModeInstance)
        {
            turningModeInstance->deactivate();
            SerialDebug.println("TPH: onExitPage - Called turningModeInstance->deactivate().");
        }
        else
        {
            SerialDebug.println("TurningPageHandler::onExitPage - Error: turningModeInstance is NULL.");
        }
    }

    void handlePacket(const lumen_packet_t *packet, TurningMode *turningMode, DisplayComm *display, MotionControl *motionControlInstance)
    {
        if (!packet || !turningMode || !display || !motionControlInstance) // Added motionControlInstance check
        {
            SerialDebug.println("TurningPageHandler::handlePacket - Error: Invalid packet, turningMode, display, or motionControlInstance pointer.");
            return;
        }

        SerialDebug.print("TurningPageHandler: RX Addr ");
        SerialDebug.print(packet->address);

        bool update_feed_displays = false;
        lumen_packet_t responsePacket; // For sending data back to HMI

        if (packet->address == HmiInputOptions::ADDR_TURNING_MM_INCH_INPUT_FROM_HMI) // Address 124
        {
            if (packet->type == kS32 || packet->type == kBool)
            {
                bool set_to_metric = (packet->data._s32 == 0);
                SerialDebug.print(" -> Turning MM/Inch to: ");
                SerialDebug.println(set_to_metric ? "Metric (0)" : "Imperial (non-0)");
                turningMode->setFeedRateMetric(set_to_metric);
                update_feed_displays = true;
            }
            else
            {
                SerialDebug.print(" -> WRONG TYPE for MM/Inch (expected S32/Bool, got ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiInputOptions::ADDR_TURNING_PREV_NEXT_BUTTON) // Address 133
        {
            if (packet->type == kS32 || (packet->type == kBool && (packet->data._s32 == 0 || packet->data._s32 == 1 || packet->data._s32 == 2)))
            {
                int32_t buttonValue = packet->data._s32;
                SerialDebug.print(" -> Prev/Next Feed (133) value: ");
                SerialDebug.println(buttonValue);
                if (buttonValue == 1)
                {
                    turningMode->selectPreviousFeedRate();
                    update_feed_displays = true;
                }
                else if (buttonValue == 2)
                {
                    turningMode->selectNextFeedRate();
                    update_feed_displays = true;
                }
                else if (buttonValue == 0)
                {
                    SerialDebug.println(" -> Prev/Next button released.");
                }
                else
                {
                    SerialDebug.println(" -> Unknown S32 value for Prev/Next button.");
                }
            }
            else
            {
                SerialDebug.print(" -> UNEXPECTED TYPE or VALUE for Prev/Next. Type: ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE) // Address 149
        {
            if (packet->type == kBool)
            {
                bool requested_state_is_enable = packet->data._bool;
                SerialDebug.print(" -> Motor Enable Toggle (149) request: ");
                SerialDebug.println(requested_state_is_enable ? "ENABLE" : "DISABLE");
                if (requested_state_is_enable)
                    turningMode->requestMotorEnable();
                else
                    turningMode->requestMotorDisable();

                lumen_packet_t responsePacket;
                responsePacket.address = HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE;
                responsePacket.type = kBool;
                responsePacket.data._bool = turningMode->isMotorEnabled();
                lumen_write_packet(&responsePacket);
                SerialDebug.print("    Sent Motor Enable state (149) back to HMI -> ");
                SerialDebug.println(responsePacket.data._bool);
            }
            else
            {
                SerialDebug.print(" -> WRONG TYPE for Motor Enable (expected Bool, got ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiInputOptions::ADDR_TURNING_FEED_DIRECTION_SELECT) // Address 129
        {
            if (packet->type == kBool)
            {
                bool setTowardsChuck = packet->data._bool;
                SerialDebug.print(" -> Feed Direction Select (129) request: ");
                SerialDebug.println(setTowardsChuck ? "TOWARDS CHUCK (true)" : "AWAY FROM CHUCK (false)");
                turningMode->setFeedDirection(setTowardsChuck);

                // Update HMI display for direction button state at address 210
                if (display)
                {
                    const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
                    display->updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, setTowardsChuck); // Send bool directly
                    SerialDebug.print("    TPH: Sent new dir state (bool) to HMI display (210): ");
                    SerialDebug.println(setTowardsChuck ? "true" : "false");
                }
            }
            else
            {
                SerialDebug.print(" -> WRONG TYPE for Feed Direction (expected kBool, got ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        // --- Auto-Stop Feature HMI Handling ---
        else if (packet->address == HmiTurningPageOptions::bool_auto_stop_enDisAddress) // Address 145
        {
            if (packet->type == kBool)
            {
                bool isEnabled = packet->data._bool;
                SerialDebug.print(" -> Auto-Stop Enable Toggle (145) request: ");
                SerialDebug.println(isEnabled ? "ENABLE" : "DISABLE");
                turningMode->setUiAutoStopEnabled(isEnabled); // This clears target in MC if isEnabled is false

                if (!isEnabled) // If auto-stop was just disabled
                {
                    // If motor is on but ELS is not active (e.g., after an auto-stop completion), restart ELS.
                    if (turningMode->isMotorEnabled() && !motionControlInstance->isElsActive())
                    {
                        SerialDebug.println("TPH: Auto-stop disabled by HMI, motor ON but ELS not active. Restarting ELS.");
                        motionControlInstance->startMotion();
                    }
                }

                // Update the target display string (will be "---" if disabled or just completed)
                String formattedTarget = turningMode->getFormattedUiAutoStopTarget();
                responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address; // 197
                responsePacket.type = kString;
                strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
                responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                lumen_write_packet(&responsePacket);
                SerialDebug.print("    Sent Auto-Stop Target (197) to HMI -> ");
                SerialDebug.println(formattedTarget);
            }
            else
            {
                SerialDebug.print(" -> WRONG TYPE for Auto-Stop Enable (expected Bool, got ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        else if (packet->address == HmiTurningPageOptions::string_set_stop_disp_value_to_stm32Address) // Address 148
        {
            if (packet->type == kString)
            {
                SerialDebug.print(" -> Auto-Stop Target Value from HMI (148): ");
                SerialDebug.println(packet->data._string);
                turningMode->setUiAutoStopTargetPositionFromString(packet->data._string);

                // Send formatted value back to HMI display
                String formattedTarget = turningMode->getFormattedUiAutoStopTarget();
                responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address; // 197
                responsePacket.type = kString;
                strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
                responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                lumen_write_packet(&responsePacket);
                SerialDebug.print("    Sent Auto-Stop Target (197) to HMI -> ");
                SerialDebug.println(formattedTarget);
            }
            else // packet->type is not kString
            {
                // WORKAROUND for HMI bug: HMI sends Type:0 (kBool) but payload is actually a string.
                // This was observed for Auto-Stop Target Value (Addr 148).
                // The HMI *should* send Type:10 (kString).
                if (packet->type == kBool) // If type is 0 (kBool)
                {
                    SerialDebug.print(" -> Auto-Stop Target (148): Received kBool (Type 0) - APPLYING WORKAROUND assuming string payload: ");
                    SerialDebug.println(packet->data._string); // Attempt to print as string
                    turningMode->setUiAutoStopTargetPositionFromString(packet->data._string);

                    // Send formatted value back to HMI display
                    String formattedTarget = turningMode->getFormattedUiAutoStopTarget();
                    responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address; // 197
                    responsePacket.type = kString;                                                                // We always send back a proper string
                    strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
                    responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                    lumen_write_packet(&responsePacket);
                    SerialDebug.print("    Sent Auto-Stop Target (197) to HMI (WORKAROUND APPLIED) -> ");
                    SerialDebug.println(formattedTarget);
                }
                else // Original error for other unexpected types
                {
                    SerialDebug.print(" -> WRONG TYPE for Auto-Stop Target Value (expected String or kBool for workaround, got ");
                    SerialDebug.print(packet->type);
                    SerialDebug.println(")");
                }

                // Log raw packet data for debugging (still useful)
                SerialDebug.print("    Raw data for address 148: ");
                for (size_t i = 0; i < sizeof(packet->data); ++i)
                {
                    SerialDebug.print(reinterpret_cast<const uint8_t *>(&packet->data)[i], HEX);
                    SerialDebug.print(" ");
                }
                SerialDebug.println();
            }
        }
        else if (packet->address == HmiTurningPageOptions::bool_grab_zAddress) // Address 198
        {
            if (packet->type == kBool && packet->data._bool) // Only act on true (button press)
            {
                SerialDebug.println(" -> Auto-Stop Grab Current Z (198) requested.");
                turningMode->grabCurrentZAsUiAutoStopTarget();

                // Send formatted value back to HMI display
                String formattedTarget = turningMode->getFormattedUiAutoStopTarget();
                responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address; // 197
                responsePacket.type = kString;
                strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
                responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                lumen_write_packet(&responsePacket);
                SerialDebug.print("    Sent Auto-Stop Target (197) to HMI -> ");
                SerialDebug.println(formattedTarget);
            }
            else if (packet->type != kBool)
            {
                SerialDebug.print(" -> WRONG TYPE for Grab Z (expected Bool, got ");
                SerialDebug.print(packet->type);
                SerialDebug.println(")");
            }
        }
        // Jog control addresses (185, 194) are handled by JogPageHandler now.
        else
        {
            SerialDebug.println(" -> Not handled by TurningPageHandler logic.");
        }

        if (update_feed_displays)
        {
            sendTurningPageFeedDisplays(turningMode);
        }
    }

    void updateDRO(DisplayComm *display, TurningMode *turningMode)
    {
        if (!display || !turningMode)
        {
            SerialDebug.println("TurningPageHandler::updateDRO - Error: Invalid display or turningMode pointer.");
            return;
        }

        float rawPosition = turningMode->getCurrentPosition();
        char positionStr[MAX_STRING_SIZE];
        char unitStr[4] = "";
        float displayPosition = rawPosition;

        bool isLeadscrewMetric = SystemConfig::RuntimeConfig::Z_Axis::leadscrew_standard_is_metric;
        bool isDisplayMetric = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;

        if (isLeadscrewMetric && !isDisplayMetric)
        {
            displayPosition = rawPosition / 25.4f;
            strncpy(unitStr, " in", sizeof(unitStr) - 1);
        }
        else if (!isLeadscrewMetric && isDisplayMetric)
        {
            displayPosition = rawPosition * 25.4f;
            strncpy(unitStr, " mm", sizeof(unitStr) - 1);
        }
        else if (isDisplayMetric)
        {
            strncpy(unitStr, " mm", sizeof(unitStr) - 1);
        }
        else
        {
            strncpy(unitStr, " in", sizeof(unitStr) - 1);
        }
        unitStr[sizeof(unitStr) - 1] = '\0';

        snprintf(positionStr, sizeof(positionStr) - strlen(unitStr), "%.3f", displayPosition);
        strncat(positionStr, unitStr, sizeof(positionStr) - strlen(positionStr) - 1);

        lumen_packet_t zPosPacket;
        zPosPacket.address = STRING_Z_POS_ADDRESS_TURNING_DRO; // Use renamed constant
        zPosPacket.type = kString;
        strncpy(zPosPacket.data._string, positionStr, MAX_STRING_SIZE - 1);
        zPosPacket.data._string[MAX_STRING_SIZE - 1] = '\0';

        lumen_write_packet(&zPosPacket);
    }

    // void flashCompleteMessage(DisplayComm *display, TurningMode *turningMode) // Now redundant
    // {
    //     if (!display || !turningMode)
    //         return;

    //     lumen_packet_t packet;
    //     packet.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address; // 197
    //     packet.type = kString;

    //     const char *completeMsg = "COMPLETE";
    //     const char *blankMsg = "        "; // Spaces to clear, ensure same length or HMI handles it

    //     for (int i = 0; i < 3; ++i)
    //     {
    //         strncpy(packet.data._string, completeMsg, MAX_STRING_SIZE - 1);
    //         packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    //         lumen_write_packet(&packet);
    //         HAL_Delay(200); // Adjust delay as needed for flash rate

    //         strncpy(packet.data._string, blankMsg, MAX_STRING_SIZE - 1);
    //         packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    //         lumen_write_packet(&packet);
    //         HAL_Delay(200);
    //     }
    //     // After flashing, restore the target display or "---"
    //     String finalDisplay = turningMode->getFormattedUiAutoStopTarget();
    //     strncpy(packet.data._string, finalDisplay.c_str(), MAX_STRING_SIZE - 1);
    //     packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    //     lumen_write_packet(&packet);
    //     SerialDebug.println("TPH: Flashed 'COMPLETE' and restored target display.");
    // }

    void update(TurningMode *turningMode, DisplayComm *display, MotionControl *motionControl) // Added motionControl
    {
        if (!turningMode || !display || !motionControl) // Added motionControl check
            return;

        // First, update the core TurningMode logic.
        // This will call turningMode->checkAndHandleAutoStopCompletion() internally
        // and set _autoStopCompletionPendingHmiSignal if target is reached.
        turningMode->update();

        // Handle non-blocking flashing sequence for "REACHED"
        if (!_isFlashingTargetReached && turningMode->isAutoStopCompletionPendingHmiSignal())
        {
            _isFlashingTargetReached = true; // Start flashing
            _flashStateCount = 0;
            _lastFlashToggleTimeMs = millis();
            _flashMessageIsVisible = true; // Start with message visible
            // Use lumen_write_packet directly
            lumen_packet_t flashPacket;
            flashPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
            flashPacket.type = kString;
            strncpy(flashPacket.data._string, "REACHED!", MAX_STRING_SIZE - 1);
            flashPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&flashPacket);
            turningMode->clearAutoStopCompletionHmiSignal(); // Consume the signal
            SerialDebug.println("TPH: Starting 'REACHED!' flash sequence.");
        }

        if (_isFlashingTargetReached)
        {
            if (millis() - _lastFlashToggleTimeMs >= FLASH_STATE_DURATION_MS)
            {
                _lastFlashToggleTimeMs = millis();
                _flashMessageIsVisible = !_flashMessageIsVisible; // Toggle visibility
                _flashStateCount++;

                lumen_packet_t togglePacket;
                togglePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
                togglePacket.type = kString;
                if (_flashMessageIsVisible)
                {
                    strncpy(togglePacket.data._string, "REACHED!", MAX_STRING_SIZE - 1);
                }
                else
                {
                    // Send spaces to clear.
                    strncpy(togglePacket.data._string, "        ", MAX_STRING_SIZE - 1);
                }
                togglePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                lumen_write_packet(&togglePacket);
                SerialDebug.print("TPH: Flash state ");
                SerialDebug.print(_flashStateCount);
                SerialDebug.print(", Visible: ");
                SerialDebug.println(_flashMessageIsVisible);

                if (_flashStateCount >= TOTAL_FLASH_STATES)
                {
                    _isFlashingTargetReached = false; // Stop flashing
                    _flashStateCount = 0;
                    // After flashing, display "---" for the target
                    String clearedTargetDisplay = "--- ";
                    clearedTargetDisplay += (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? "mm" : "in");
                    // Use lumen_write_packet directly
                    lumen_packet_t finalPacket;
                    finalPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
                    finalPacket.type = kString;
                    strncpy(finalPacket.data._string, clearedTargetDisplay.c_str(), MAX_STRING_SIZE - 1);
                    finalPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                    lumen_write_packet(&finalPacket);
                    SerialDebug.println("TPH: 'REACHED!' flash sequence complete. Target display cleared.");
                }
            }
        }

        // Any other periodic updates for Turning Page can go here
        uint32_t currentTime = millis();

        // Timed DRO Update
        if (currentTime - lastDroUpdateTimeMs_Handler >= HANDLER_DRO_UPDATE_INTERVAL)
        {
            updateDRO(display, turningMode);
            lastDroUpdateTimeMs_Handler = currentTime;
        }

        // Timed RPM Update
        if (currentTime - lastRpmUpdateTimeMs_Handler >= HANDLER_RPM_UPDATE_INTERVAL)
        {
            lumen_packet_t rpmPacket;
            rpmPacket.address = 131; // RPM HMI Address
            rpmPacket.type = kS32;
            rpmPacket.data._s32 = static_cast<int32_t>(abs(motionControl->getStatus().spindle_rpm)); // Use abs for positive display
            lumen_write_packet(&rpmPacket);
            lastRpmUpdateTimeMs_Handler = currentTime;
            SerialDebug.print("TPH: Sent RPM (131) -> ");
            SerialDebug.println(rpmPacket.data._s32); // Optional debug
        }
    }

} // namespace TurningPageHandler
