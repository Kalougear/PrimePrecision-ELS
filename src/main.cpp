// File: src/main.cpp (Implementing Interrupt-Driven Serial RX)

#include "LumenProtocol.h"
#include <Arduino.h>
#include <cmath>
#include "Config/serial_debug.h"
#include "Hardware/SystemClock.h"
#include "Config/SystemConfig.h"
#include <STM32Step.h>
#include "Hardware/EncoderTimer.h"
#include "Motion/MotionControl.h"
#include "Motion/FeedRateManager.h"
#include "Config/HmiInputOptions.h"
#include "UI/DisplayComm.h"
#include "UI/MenuSystem.h"
#include "UI/HmiHandlers/SetupPageHandler.h"
#include "UI/HmiHandlers/TurningPageHandler.h"
#include "UI/HmiHandlers/JogPageHandler.h"
#include "UI/HmiHandlers/ThreadingPageHandler.h"

enum ActiveHmiPage
{
    PAGE_UNKNOWN = 0,
    PAGE_TURNING = 1,
    PAGE_THREADING = 2,
    PAGE_POSITIONING = 3,
    PAGE_SETUP = 4,
    PAGE_JOG = 5
};
ActiveHmiPage currentPage = PAGE_TURNING;
const uint16_t int_tab_selectionAddress = 136;

HardwareSerial SerialDebug(PD9, PD8);
HardwareSerial SerialDisplay(PA10, PA9);
DisplayComm displayComm;
MenuSystem menuSystem;

const uint16_t mmInchSelectorAddress = 124;
const uint16_t directionSelectorAddress = 129;
const uint16_t startSTOPFEEDAddress = 130;
const uint16_t rmpAddress = 131;
const uint16_t actualFEEDRATEAddress = 132;
const uint16_t prevNEXTFEEDRATEVALUEAddress = 133;
const uint16_t actualFEEDRATEDESCRIPTIONAddress = 134;

lumen_packet_t mmInchSelectorPacket = {mmInchSelectorAddress, kS32};
lumen_packet_t startSTOPFEEDPacket = {startSTOPFEEDAddress, kBool};
lumen_packet_t rmpPacket = {rmpAddress, kS32};
lumen_packet_t actualFEEDRATEPacket = {actualFEEDRATEAddress, kString};
lumen_packet_t prevNEXTFEEDRATEVALUEPacket = {prevNEXTFEEDRATEVALUEAddress, kS32};
lumen_packet_t actualFEEDRATEDESCRIPTIONPacket = {actualFEEDRATEDESCRIPTIONAddress, kString};

FeedRateManager feedRateManager;
char feedRateBuffer[40];

void sendMainPageFeedRateDisplay()
{
    feedRateManager.getDisplayString(feedRateBuffer, sizeof(feedRateBuffer));
    memcpy(actualFEEDRATEPacket.data._string, feedRateBuffer, MAX_STRING_SIZE - 1);
    actualFEEDRATEPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEPacket);

    const char *initialCategory = feedRateManager.getCurrentCategory();
    strncpy(actualFEEDRATEDESCRIPTIONPacket.data._string, initialCategory, MAX_STRING_SIZE - 1);
    actualFEEDRATEDESCRIPTIONPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEDESCRIPTIONPacket);
}

EncoderTimer globalEncoderTimerInstance;
MotionControl motionCtrl(MotionControl::MotionPins{STM32Step::PinConfig::StepPin::PIN, STM32Step::PinConfig::DirPin::PIN, STM32Step::PinConfig::EnablePin::PIN});

volatile bool g_exti_pa5_index_pulse_detected = false;
volatile unsigned long g_last_pa5_interrupt_time = 0;
const unsigned long PA5_DEBOUNCE_DELAY_MS = 5;

extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length)
{
    SerialDisplay.write(data, length);
}

void pa5_index_pulse_isr()
{
    unsigned long interrupt_time = millis();
    if (interrupt_time - g_last_pa5_interrupt_time > PA5_DEBOUNCE_DELAY_MS)
    {
        g_exti_pa5_index_pulse_detected = true;
        g_last_pa5_interrupt_time = interrupt_time;
    }
}

namespace STM32Step
{
    extern volatile uint32_t g_isr_call_count;
    extern volatile uint32_t g_stepper_isr_entry_count;
}
uint32_t last_g_isr_call_count = 0;
uint32_t last_g_stepper_isr_entry_count = 0;
uint32_t last_isr_print_time = 0;

const uint16_t HMI_SERIAL_INPUT_BUFFER_SIZE = 256;
static uint8_t hmi_serial_input_buffer[HMI_SERIAL_INPUT_BUFFER_SIZE];
static uint16_t hmi_buffer_write_idx = 0;
static uint16_t hmi_buffer_read_idx = 0;

extern "C" uint16_t lumen_get_byte()
{
    if (hmi_buffer_read_idx < hmi_buffer_write_idx)
    {
        return hmi_serial_input_buffer[hmi_buffer_read_idx++];
    }
    return DATA_NULL;
}

void setup()
{
    SerialDebug.begin(115200);
    HAL_Delay(3000);

    SerialDisplay.begin(115200, SERIAL_8N1);

    if (!SystemConfig::ConfigManager::initialize())
    {
        while (1)
            ;
    }

    if (!SystemClock::GetInstance().initialize())
    {
        while (1)
            ;
    }

    if (!globalEncoderTimerInstance.begin())
    {
        while (1)
            ;
    }

    pinMode(PA5, INPUT);
    if (digitalPinToInterrupt(PA5) != NOT_AN_INTERRUPT)
    {
        attachInterrupt(digitalPinToInterrupt(PA5), pa5_index_pulse_isr, FALLING);
    }
    else
    {
        while (1)
            ;
    }

    if (!displayComm.begin(&SerialDisplay))
    {
        while (1)
            ;
    }

    if (!motionCtrl.begin(&globalEncoderTimerInstance))
    {
        while (1)
            ;
    }

    // Enable the stepper motor driver
    motionCtrl.getStepperInstance()->enable();

    MotionControl::Config cfg;
    cfg.thread_pitch = 0.5f;
    cfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    cfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    cfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    cfg.reverse_direction = false;
    cfg.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;
    motionCtrl.setConfig(cfg);
    motionCtrl.setMode(MotionControl::Mode::TURNING);

    if (!menuSystem.begin(&displayComm, &motionCtrl))
    {
        while (1)
            ;
    }

    SetupPageHandler::init();
    TurningPageHandler::init(menuSystem.getTurningMode(), &displayComm, &motionCtrl);
    JogPageHandler::init(&motionCtrl);
    ThreadingPageHandler::init(&displayComm, menuSystem.getThreadingMode(), &motionCtrl);

    displayComm.showScreen(currentPage);

    rmpPacket.data._s32 = 0;
    lumen_write_packet(&rmpPacket);

    feedRateManager.setMetric(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric);
    sendMainPageFeedRateDisplay();

    motionCtrl.startMotion();

    sendMainPageFeedRateDisplay();

    // Force HMI to Page 1 (Turning) on startup
    // Only initialize the active page to ensure consistent state
    if (currentPage == PAGE_TURNING)
    {
        TurningPageHandler::onEnterPage();
    }
}

void loop()
{
    static uint32_t lastRpmHmiUpdateTime = 0;
    const uint32_t RPM_HMI_UPDATE_INTERVAL = 250;

    uint32_t currentTime = millis();

    if (g_exti_pa5_index_pulse_detected)
    {
        g_exti_pa5_index_pulse_detected = false;
    }

    if (currentTime - lastRpmHmiUpdateTime >= RPM_HMI_UPDATE_INTERVAL)
    {
        MotionControl::Status mcStatus = motionCtrl.getStatus();
        int32_t currentRpmBeforeAbs = mcStatus.spindle_rpm;
        rmpPacket.data._s32 = abs(currentRpmBeforeAbs);
        lumen_write_packet(&rmpPacket);
        lastRpmHmiUpdateTime = currentTime;
    }

    if (currentPage == PAGE_TURNING)
    {
        TurningPageHandler::update();
    }
    else if (currentPage == PAGE_THREADING)
    {
        ThreadingPageHandler::update();
    }

    while (SerialDisplay.available() > 0 && hmi_buffer_write_idx < HMI_SERIAL_INPUT_BUFFER_SIZE)
    {
        uint8_t byte_received = SerialDisplay.read();
        hmi_serial_input_buffer[hmi_buffer_write_idx++] = byte_received;
    }

    if (hmi_buffer_write_idx > 0)
    {
        lumen_available();
    }

    lumen_packet_t *packet = nullptr;
    while ((packet = lumen_get_first_packet()) != NULL)
    {
        if (packet->address == int_tab_selectionAddress)
        {
            ActiveHmiPage newPage = (ActiveHmiPage)packet->data._s32;
            if (newPage != currentPage)
            {
                if (currentPage == PAGE_JOG)
                {
                    JogPageHandler::onExitPage();
                    HAL_Delay(10);
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onExitPage();
                }
                else if (currentPage == PAGE_THREADING)
                {
                    ThreadingPageHandler::onExitPage();
                }

                currentPage = newPage;

                if (currentPage == PAGE_SETUP)
                {
                    SetupPageHandler::onEnterPage();
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onEnterPage();
                    const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
                    displayComm.updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, true);
                }
                else if (currentPage == PAGE_JOG)
                {
                    JogPageHandler::onEnterPage();
                }
                else if (currentPage == PAGE_THREADING)
                {
                    ThreadingPageHandler::onEnterPage();
                }
            }
        }
        else
        {
            switch (currentPage)
            {
            case PAGE_SETUP:
                SetupPageHandler::handlePacket(packet);
                break;
            case PAGE_TURNING:
                if (packet->address == 192 && packet->type == kBool && packet->data._bool)
                {
                    if (menuSystem.getTurningMode())
                    {
                        menuSystem.getTurningMode()->setZeroPosition();
                    }
                }
                else
                {
                    TurningPageHandler::handlePacket(packet);
                }
                break;
            case PAGE_JOG:
                JogPageHandler::handlePacket(packet);
                break;
            case PAGE_THREADING:
                if (packet->address == 192 && packet->type == kBool && packet->data._bool)
                {
                    if (menuSystem.getThreadingMode())
                    {
                        menuSystem.getThreadingMode()->setZeroPosition();
                    }
                }
                else
                {
                    ThreadingPageHandler::handlePacket(packet);
                }
                break;
            default:
                break;
            }
        }
    }

    if (hmi_buffer_read_idx >= hmi_buffer_write_idx)
    {
        hmi_buffer_read_idx = 0;
        hmi_buffer_write_idx = 0;
    }
    else if (hmi_buffer_read_idx > 0)
    {
        uint16_t remaining_bytes = hmi_buffer_write_idx - hmi_buffer_read_idx;
        memmove(hmi_serial_input_buffer, &hmi_serial_input_buffer[hmi_buffer_read_idx], remaining_bytes);
        hmi_buffer_write_idx = remaining_bytes;
        hmi_buffer_read_idx = 0;
    }
}
