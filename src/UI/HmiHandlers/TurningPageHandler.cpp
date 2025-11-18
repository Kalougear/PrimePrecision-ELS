#include "UI/HmiHandlers/TurningPageHandler.h"
#include "Config/HmiInputOptions.h"
#include "Config/SystemConfig.h"
#include "LumenProtocol.h"
#include <HardwareSerial.h>
#include "UI/DisplayComm.h"
#include "Motion/TurningMode.h"
#include <stdio.h>
#include <string.h>
#include "Config/Hmi/TurningPageOptions.h"
#include "Config/serial_debug.h"

extern HardwareSerial SerialDebug;

// Define static member variables
TurningMode *TurningPageHandler::_turningMode = nullptr;
DisplayComm *TurningPageHandler::_displayComm = nullptr;
MotionControl *TurningPageHandler::_motionControl = nullptr;

// Static variables for non-blocking "REACHED" flashing
static bool _isFlashingTargetReached = false;
static uint8_t _flashStateCount = 0;
static unsigned long _lastFlashToggleTimeMs = 0;
static bool _flashMessageIsVisible = false;
const unsigned long FLASH_STATE_DURATION_MS = 250;
const uint8_t TOTAL_FLASH_STATES = 6;

const uint16_t STRING_Z_POS_ADDRESS_TURNING_DRO = 135;

static uint32_t lastDroUpdateTimeMs_Handler = 0;
const uint32_t HANDLER_DRO_UPDATE_INTERVAL = 100;
static uint32_t lastRpmUpdateTimeMs_Handler = 0;
const uint32_t HANDLER_RPM_UPDATE_INTERVAL = 200;

void TurningPageHandler::init(TurningMode *turningMode, DisplayComm *displayComm, MotionControl *motionControl)
{
    _turningMode = turningMode;
    _displayComm = displayComm;
    _motionControl = motionControl;
    SerialDebug.println("TurningPageHandler initialized.");
}

void TurningPageHandler::onEnterPage()
{
    SerialDebug.println("TurningPageHandler: onEnterPage called.");
    if (_turningMode)
    {
        _turningMode->setFeedDirection(true);
        _turningMode->activate();
        _turningMode->resetAutoStopRuntimeSettings();

        lumen_packet_t autoStopEnablePacket;
        autoStopEnablePacket.address = HmiTurningPageOptions::bool_auto_stop_enDisAddress;
        autoStopEnablePacket.type = kBool;
        autoStopEnablePacket.data._bool = _turningMode->isUiAutoStopEnabled();
        lumen_write_packet(&autoStopEnablePacket);

        lumen_packet_t autoStopTargetPacket;
        autoStopTargetPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
        autoStopTargetPacket.type = kString;
        String formattedTarget = _turningMode->getFormattedUiAutoStopTarget();
        strncpy(autoStopTargetPacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
        autoStopTargetPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
        lumen_write_packet(&autoStopTargetPacket);

        sendTurningPageFeedDisplays();

        lumen_packet_t motorEnablePacket;
        motorEnablePacket.address = HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE;
        motorEnablePacket.type = kBool;
        motorEnablePacket.data._bool = _turningMode->isMotorEnabled();
        lumen_write_packet(&motorEnablePacket);
    }
}

void TurningPageHandler::onExitPage()
{
    SerialDebug.println("TurningPageHandler: onExitPage called.");
    if (_turningMode)
    {
        _turningMode->deactivate();
    }
}

void TurningPageHandler::handlePacket(const lumen_packet_t *packet)
{
    if (!packet || !_turningMode || !_displayComm || !_motionControl)
    {
        return;
    }

    bool update_feed_displays = false;
    lumen_packet_t responsePacket;

    if (packet->address == HmiInputOptions::ADDR_TURNING_MM_INCH_INPUT_FROM_HMI)
    {
        if (packet->type == kS32 || packet->type == kBool)
        {
            bool set_to_metric = (packet->data._s32 == 0);
            _turningMode->setFeedRateMetric(set_to_metric);
            update_feed_displays = true;
        }
    }
    else if (packet->address == HmiInputOptions::ADDR_TURNING_PREV_NEXT_BUTTON)
    {
        if (packet->type == kS32 || (packet->type == kBool && (packet->data._s32 >= 0 && packet->data._s32 <= 2)))
        {
            if (packet->data._s32 == 1)
            {
                _turningMode->selectPreviousFeedRate();
                update_feed_displays = true;
            }
            else if (packet->data._s32 == 2)
            {
                _turningMode->selectNextFeedRate();
                update_feed_displays = true;
            }
        }
    }
    else if (packet->address == HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE)
    {
        if (packet->type == kBool)
        {
            if (packet->data._bool)
                _turningMode->requestMotorEnable();
            else
                _turningMode->requestMotorDisable();

            responsePacket.address = HmiInputOptions::ADDR_TURNING_MOTOR_ENABLE_TOGGLE;
            responsePacket.type = kBool;
            responsePacket.data._bool = _turningMode->isMotorEnabled();
            lumen_write_packet(&responsePacket);
        }
    }
    else if (packet->address == HmiInputOptions::ADDR_TURNING_FEED_DIRECTION_SELECT)
    {
        if (packet->type == kBool)
        {
            _turningMode->setFeedDirection(packet->data._bool);
            const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
            _displayComm->updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, packet->data._bool);
        }
    }
    else if (packet->address == HmiTurningPageOptions::bool_auto_stop_enDisAddress)
    {
        if (packet->type == kBool)
        {
            _turningMode->setUiAutoStopEnabled(packet->data._bool);
            if (!packet->data._bool && _turningMode->isMotorEnabled() && !_motionControl->isElsActive())
            {
                _motionControl->startMotion();
            }
            String formattedTarget = _turningMode->getFormattedUiAutoStopTarget();
            responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
            responsePacket.type = kString;
            strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
        }
    }
    else if (packet->address == HmiTurningPageOptions::string_set_stop_disp_value_to_stm32Address)
    {
        if (packet->type == kString || packet->type == kBool) // HMI bug workaround
        {
            _turningMode->setUiAutoStopTargetPositionFromString(packet->data._string);
            String formattedTarget = _turningMode->getFormattedUiAutoStopTarget();
            responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
            responsePacket.type = kString;
            strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
        }
    }
    else if (packet->address == HmiTurningPageOptions::bool_grab_zAddress)
    {
        if (packet->type == kBool && packet->data._bool)
        {
            _turningMode->grabCurrentZAsUiAutoStopTarget();
            String formattedTarget = _turningMode->getFormattedUiAutoStopTarget();
            responsePacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
            responsePacket.type = kString;
            strncpy(responsePacket.data._string, formattedTarget.c_str(), MAX_STRING_SIZE - 1);
            responsePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&responsePacket);
        }
    }

    if (update_feed_displays)
    {
        sendTurningPageFeedDisplays();
    }
}

void TurningPageHandler::update()
{
    if (!_turningMode || !_displayComm || !_motionControl)
        return;

    _turningMode->update();

    if (!_isFlashingTargetReached && _turningMode->isAutoStopCompletionPendingHmiSignal())
    {
        flashCompleteMessage();
        _turningMode->clearAutoStopCompletionHmiSignal();
    }

    if (_isFlashingTargetReached)
    {
        if (millis() - _lastFlashToggleTimeMs >= FLASH_STATE_DURATION_MS)
        {
            _lastFlashToggleTimeMs = millis();
            _flashMessageIsVisible = !_flashMessageIsVisible;
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
                strncpy(togglePacket.data._string, "        ", MAX_STRING_SIZE - 1);
            }
            togglePacket.data._string[MAX_STRING_SIZE - 1] = '\0';
            lumen_write_packet(&togglePacket);

            if (_flashStateCount >= TOTAL_FLASH_STATES)
            {
                _isFlashingTargetReached = false;
                _flashStateCount = 0;
                String clearedTargetDisplay = "--- ";
                clearedTargetDisplay += (SystemConfig::RuntimeConfig::System::measurement_unit_is_metric ? "mm" : "in");

                lumen_packet_t finalPacket;
                finalPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
                finalPacket.type = kString;
                strncpy(finalPacket.data._string, clearedTargetDisplay.c_str(), MAX_STRING_SIZE - 1);
                finalPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
                lumen_write_packet(&finalPacket);
            }
        }
    }

    uint32_t currentTime = millis();
    if (currentTime - lastDroUpdateTimeMs_Handler >= HANDLER_DRO_UPDATE_INTERVAL)
    {
        updateDRO();
        lastDroUpdateTimeMs_Handler = currentTime;
    }
    // RPM update is now handled globally in main.cpp
}

void TurningPageHandler::sendTurningPageFeedDisplays()
{
    if (!_turningMode)
        return;

    lumen_packet_t packet;
    bool is_metric = _turningMode->getFeedRateIsMetric();
    packet.address = HmiInputOptions::ADDR_TURNING_MM_INCH_DISPLAY_TO_HMI;
    packet.type = kBool;
    packet.data._bool = !is_metric;
    lumen_write_packet(&packet);

    char feedRateStr[MAX_STRING_SIZE];
    _turningMode->getFeedRateManager().getDisplayString(feedRateStr, sizeof(feedRateStr));
    packet.address = HmiInputOptions::ADDR_TURNING_FEED_RATE_VALUE_DISPLAY;
    packet.type = kString;
    strncpy(packet.data._string, feedRateStr, MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);

    const char *category = _turningMode->getFeedRateCategory();
    packet.address = HmiInputOptions::ADDR_TURNING_FEED_RATE_DESC_DISPLAY;
    packet.type = kString;
    strncpy(packet.data._string, category ? category : "", MAX_STRING_SIZE - 1);
    packet.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&packet);
}

void TurningPageHandler::updateDRO()
{
    if (!_displayComm || !_turningMode)
        return;

    float rawPosition = _turningMode->getCurrentPosition();
    char positionStr[MAX_STRING_SIZE];
    char unitStr[4] = "";
    float displayPosition = rawPosition;

    bool isDisplayMetric = SystemConfig::RuntimeConfig::System::measurement_unit_is_metric;
    if (isDisplayMetric)
    {
        strncpy(unitStr, " mm", sizeof(unitStr) - 1);
    }
    else
    {
        displayPosition = rawPosition / 25.4f;
        strncpy(unitStr, " in", sizeof(unitStr) - 1);
    }
    unitStr[sizeof(unitStr) - 1] = '\0';

    snprintf(positionStr, sizeof(positionStr) - strlen(unitStr), "%.3f", displayPosition);
    strncat(positionStr, unitStr, sizeof(positionStr) - strlen(positionStr) - 1);

    lumen_packet_t zPosPacket;
    zPosPacket.address = STRING_Z_POS_ADDRESS_TURNING_DRO;
    zPosPacket.type = kString;
    strncpy(zPosPacket.data._string, positionStr, MAX_STRING_SIZE - 1);
    zPosPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&zPosPacket);
}

void TurningPageHandler::flashCompleteMessage()
{
    _isFlashingTargetReached = true;
    _flashStateCount = 0;
    _lastFlashToggleTimeMs = millis();
    _flashMessageIsVisible = true;

    lumen_packet_t flashPacket;
    flashPacket.address = HmiTurningPageOptions::string_set_stop_disp_value_from_stm32Address;
    flashPacket.type = kString;
    strncpy(flashPacket.data._string, "REACHED!", MAX_STRING_SIZE - 1);
    flashPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&flashPacket);
}
