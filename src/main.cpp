// File: src/main.cpp

#include "LumenProtocol.h"
#include <Arduino.h>
#include <cmath> // For abs()
#include "Config/serial_debug.h"
#include "Hardware/SystemClock.h"
#include "Config/SystemConfig.h"
#include <STM32Step.h>
#include "Hardware/EncoderTimer.h"
#include "Motion/MotionControl.h"
#include "Motion/FeedRateManager.h"
#include "Config/HmiInputOptions.h" // For HMI addresses and page enums
#include "UI/DisplayComm.h"         // For DisplayComm class
#include "UI/MenuSystem.h"          // For MenuSystem class
#include "UI/HmiHandlers/SetupPageHandler.h"
#include "UI/HmiHandlers/TurningPageHandler.h"
#include "UI/HmiHandlers/JogPageHandler.h"
#include "UI/HmiHandlers/ThreadingPageHandler.h"
// #include "UI/HmiHandlers/PositioningPageHandler.h" // If/when created

// Pin definitions
static const uint8_t ACTUAL_STEP_PIN = STM32Step::PinConfig::StepPin::PIN;
static const uint8_t ACTUAL_DIR_PIN = STM32Step::PinConfig::DirPin::PIN;
static const uint8_t ACTUAL_ENABLE_PIN = STM32Step::PinConfig::EnablePin::PIN;

// Serial ports
HardwareSerial SerialDebug(PD9, PD8);    // USART3
HardwareSerial SerialDisplay(PA10, PA9); // USART1 for HMI

// Global UI and System Objects
DisplayComm displayComm;
MenuSystem menuSystem;
FeedRateManager feedRateManager;
EncoderTimer globalEncoderTimerInstance;
MotionControl motionCtrl(MotionControl::MotionPins{ACTUAL_STEP_PIN, ACTUAL_DIR_PIN, ACTUAL_ENABLE_PIN});

// HMI Page Management
enum ActiveHmiPage
{
    PAGE_UNKNOWN = 0,
    PAGE_TURNING = 1,
    PAGE_THREADING = 2,
    PAGE_POSITIONING = 3,
    PAGE_SETUP = 4,
    PAGE_JOG = 5
};
ActiveHmiPage currentPage = PAGE_TURNING;      // Default to Turning page
const uint16_t int_tab_selectionAddress = 136; // HMI address for page change notifications (defined locally as not in HmiInputOptions.h)

// Global HMI Packets (using constants from HmiInputOptions.h where applicable)
const uint16_t mmInchSelectorAddress = 124;    // Consider moving to HmiInputOptions if it's a general control
const uint16_t directionSelectorAddress = 129; // Consider moving
const uint16_t startSTOPFEEDAddress = 130;     // Consider moving
const uint16_t rmpAddress = 131;               // Consider moving

lumen_packet_t mmInchSelectorPacket = {mmInchSelectorAddress, kS32};
lumen_packet_t startSTOPFEEDPacket = {startSTOPFEEDAddress, kBool};
lumen_packet_t rmpPacket = {rmpAddress, kS32};
lumen_packet_t actualFEEDRATEPacket = {HmiInputOptions::ADDR_TURNING_FEED_RATE_VALUE_DISPLAY, kString};
lumen_packet_t prevNEXTFEEDRATEVALUEPacket = {HmiInputOptions::ADDR_TURNING_PREV_NEXT_BUTTON, kS32};
lumen_packet_t actualFEEDRATEDESCRIPTIONPacket = {HmiInputOptions::ADDR_TURNING_FEED_RATE_DESC_DISPLAY, kString};

char feedRateBuffer[MAX_STRING_SIZE]; // Buffer for feed rate strings

// Helper function to send main page feed rate display
void sendMainPageFeedRateDisplay()
{
    feedRateManager.getDisplayString(feedRateBuffer, sizeof(feedRateBuffer)); // Populates buffer with value and unit
    memcpy(actualFEEDRATEPacket.data._string, feedRateBuffer, MAX_STRING_SIZE);
    actualFEEDRATEPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEPacket);

    const char *currentCategory = feedRateManager.getCurrentCategory();
    strncpy(actualFEEDRATEDESCRIPTIONPacket.data._string, currentCategory, MAX_STRING_SIZE - 1);
    actualFEEDRATEDESCRIPTIONPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEDESCRIPTIONPacket);
}

// Index Pulse Handling
volatile bool g_exti_pa5_index_pulse_detected = false;
volatile unsigned long g_last_pa5_interrupt_time = 0;
const unsigned long PA5_DEBOUNCE_DELAY_MS = 5;

void pa5_index_pulse_isr()
{
    unsigned long interrupt_time = millis();
    if (interrupt_time - g_last_pa5_interrupt_time > PA5_DEBOUNCE_DELAY_MS)
    {
        g_exti_pa5_index_pulse_detected = true;
        g_last_pa5_interrupt_time = interrupt_time;
    }
}

// HMI Serial Input Buffer & Lumen Integration
const uint16_t HMI_SERIAL_INPUT_BUFFER_SIZE = 256;
static uint8_t hmi_serial_input_buffer[HMI_SERIAL_INPUT_BUFFER_SIZE];
static uint16_t hmi_buffer_write_idx = 0;
static uint16_t hmi_buffer_read_idx = 0;

extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length)
{
    SerialDisplay.write(data, length);
}

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
    HAL_Delay(5000); // Wait 5 seconds for HMI to initialize and stabilize.

    SerialDisplay.begin(115200, SERIAL_8N1);

    rmpPacket.address = rmpAddress;
    rmpPacket.type = kS32;
    rmpPacket.data._s32 = 0;

    if (!SystemConfig::ConfigManager::initialize())
    {
        SerialDebug.println("CRITICAL ERROR: System Configuration initialization failed!");
        while (1)
            ;
    }

    if (!SystemClock::GetInstance().initialize())
    {
        SerialDebug.println("CRITICAL ERROR: SystemClock initialization failed!");
        while (1)
            ;
    }

    // --- Enable DWT Cycle Counter for precise delays ---
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable access to trace and debug registers
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable the cycle counter
    // SystemCoreClockUpdate(); // Call if SystemCoreClock might not be initialized/updated yet. Usually done by HAL_Init.

    if (!globalEncoderTimerInstance.begin())
    {
        SerialDebug.println("CRITICAL ERROR: globalEncoderTimerInstance.begin() failed!");
        while (1)
            ;
    }
    // DWT enabling lines are already present above this block. Removing duplicates.

    pinMode(PA5, INPUT);
    if (digitalPinToInterrupt(PA5) != NOT_AN_INTERRUPT)
    {
        attachInterrupt(digitalPinToInterrupt(PA5), pa5_index_pulse_isr, FALLING);
        SerialDebug.println("DEBUG: Attached EXTI for PA5 Index Pulse.");
    }
    else
    {
        SerialDebug.println("CRITICAL ERROR: PA5 cannot be used as an interrupt pin!");
        while (1)
            ;
    }

    if (!displayComm.begin(&SerialDisplay))
    {
        SerialDebug.println("CRITICAL ERROR: DisplayComm begin failed!");
        while (1)
            ;
    }

    if (!motionCtrl.begin())
    {
        SerialDebug.println("CRITICAL ERROR: MotionControl begin failed!");
        while (1)
            ;
    }

    // Default MotionControl config (can be overridden by specific modes/HMI)
    MotionControl::Config cfg;
    cfg.thread_pitch = 1.0f; // Default to 1mm pitch for initial setup
    cfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    cfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    cfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    cfg.reverse_direction = false;
    cfg.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;
    motionCtrl.setConfig(cfg);
    motionCtrl.setMode(MotionControl::Mode::TURNING); // Default mode

    if (!menuSystem.begin(&displayComm, &motionCtrl))
    {
        SerialDebug.println("CRITICAL ERROR: MenuSystem begin failed!");
        while (1)
            ;
    }

    SetupPageHandler::init();
    TurningPageHandler::init(menuSystem.getTurningMode());
    JogPageHandler::init(&motionCtrl);
    ThreadingPageHandler::init(&displayComm, menuSystem.getThreadingMode(), &motionCtrl);

    displayComm.showScreen(currentPage);
    SerialDebug.print("Initial HMI screen explicitly set to page ID: ");
    SerialDebug.println(static_cast<int>(currentPage));

    lumen_write_packet(&rmpPacket); // Send initial RPM (0)

    feedRateManager.setMetric(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric);
    sendMainPageFeedRateDisplay();

    SerialDebug.println("STM32 Core Setup complete. HMI data being sent proactively.");
    SerialDebug.print("Sending initial data for default page: ");
    SerialDebug.println(currentPage);
    sendMainPageFeedRateDisplay();

    SerialDebug.println("Proactively sending initial data for Setup Page...");
    SetupPageHandler::onEnterPage();
    SerialDebug.println("Proactively sending initial data for Turning Page...");
    TurningPageHandler::onEnterPage(menuSystem.getTurningMode());
    SerialDebug.println("Proactively sending initial data for Jog Page...");
    JogPageHandler::onEnterPage();

    if (currentPage == PAGE_SETUP)
    {
        SerialDebug.println("Default page is Setup. Data sent by SetupPageHandler::onEnterPage().");
    }
    else if (currentPage == PAGE_TURNING)
    {
        SerialDebug.println("Default page is Turning. Data sent by TurningPageHandler::onEnterPage().");
    }
    else if (currentPage == PAGE_JOG)
    {
        SerialDebug.println("Default page is Jog. Data sent by JogPageHandler::onEnterPage().");
    }

    SerialDebug.println("Initial HMI data sent. Entering main loop.");

    // --- ELS Test Setup Example ---
    SerialDebug.println("--- CONFIGURING FOR ELS TEST (e.g., Turning Mode, 1mm pitch) ---");
    MotionControl::Config els_cfg;
    els_cfg.thread_pitch = 1.0f;                                                     // Example: 1mm pitch
    els_cfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch; // Use configured leadscrew pitch
    els_cfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    els_cfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    els_cfg.reverse_direction = false; // Example: Forward direction
    els_cfg.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;
    motionCtrl.setConfig(els_cfg);
    motionCtrl.setMode(MotionControl::Mode::TURNING); // Or THREADING
    if (motionCtrl.getStepperInstance())
        motionCtrl.getStepperInstance()->enable(); // Ensure stepper is enabled
    motionCtrl.startMotion();                      // Start ELS
    SerialDebug.println("ELS Mode configured and started. Rotate encoder to test.");
    // --- End ELS Test Setup Example ---
}

void loop()
{
    static uint32_t lastRpmHmiUpdateTime = 0;
    static uint32_t lastEncoderDebugTime = 0;
    const uint32_t RPM_HMI_UPDATE_INTERVAL = 250;
    // const uint32_t DRO_UPDATE_INTERVAL = 200; // DRO updates handled by page handlers

    uint32_t currentTime = millis();

    if (g_exti_pa5_index_pulse_detected)
    {
        SerialDebug.println("MainLoop: Index Pulse Detected (EXTI PA5)!");
        g_exti_pa5_index_pulse_detected = false;
    }

    if (currentTime - lastEncoderDebugTime > 250)
    {
        int32_t rawCount = globalEncoderTimerInstance.getCount();
        int16_t rpm = globalEncoderTimerInstance.getRPM();
        bool dir = __HAL_TIM_IS_TIM_COUNTING_DOWN(globalEncoderTimerInstance.getTimerHandle());
        SerialDebug.print("Encoder Raw Cnt: ");
        SerialDebug.print(rawCount);
        SerialDebug.print(", RPM: ");
        SerialDebug.print(rpm);
        SerialDebug.print(", Dir (Down?): ");
        SerialDebug.println(dir ? "Y" : "N");
        lastEncoderDebugTime = currentTime;
    }

    if (currentPage != PAGE_TURNING && (currentTime - lastRpmHmiUpdateTime >= RPM_HMI_UPDATE_INTERVAL))
    {
        MotionControl::Status mcStatus = motionCtrl.getStatus();
        int32_t currentRpmBeforeAbs = mcStatus.spindle_rpm;
        rmpPacket.data._s32 = abs(currentRpmBeforeAbs);
        lumen_write_packet(&rmpPacket);
        lastRpmHmiUpdateTime = currentTime;
    }

    if (currentPage == PAGE_TURNING)
    {
        TurningPageHandler::update(menuSystem.getTurningMode(), menuSystem.getDisplayComm(), &motionCtrl);
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
        bool packet_handled_by_page_handler = false;

        if (packet->address == int_tab_selectionAddress)
        {
            ActiveHmiPage newPage = (ActiveHmiPage)packet->data._s32;
            SerialDebug.print("MainLoop: Received Page Change Addr 136. HMI sent newPage ID: ");
            SerialDebug.println(static_cast<int>(newPage));
            SerialDebug.print("MainLoop: Current internal page ID before change: ");
            SerialDebug.println(static_cast<int>(currentPage));

            if (newPage != currentPage)
            {
                SerialDebug.println("MainLoop: newPage ID is different from currentPage ID. Processing page change.");
                if (currentPage == PAGE_JOG)
                {
                    JogPageHandler::onExitPage();
                    HAL_Delay(10);
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onExitPage(menuSystem.getTurningMode());
                    SerialDebug.println("MainLoop: Called TurningPageHandler::onExitPage().");
                }
                else if (currentPage == PAGE_THREADING)
                {
                    ThreadingPageHandler::onExitPage();
                    SerialDebug.println("MainLoop: Called ThreadingPageHandler::onExitPage().");
                }

                currentPage = newPage;

                SerialDebug.print("MainLoop: Internal currentPage variable successfully updated to: ");
                SerialDebug.println(static_cast<int>(currentPage));

                if (currentPage == PAGE_SETUP)
                {
                    SetupPageHandler::onEnterPage();
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onEnterPage(menuSystem.getTurningMode());
                    const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;
                    displayComm.updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, true);
                    SerialDebug.println("Entered Turning Page. Called TurningPageHandler::onEnterPage() and sent RH state (true) to HMI dir display (210).");
                }
                else if (currentPage == PAGE_JOG)
                {
                    JogPageHandler::onEnterPage();
                    SerialDebug.println("Entered Jog Page. Called JogPageHandler::onEnterPage().");
                }
                else if (currentPage == PAGE_THREADING)
                {
                    ThreadingPageHandler::onEnterPage();
                    SerialDebug.println("Entered Threading Page. Called ThreadingPageHandler::onEnterPage().");
                }
                else if (currentPage == PAGE_POSITIONING)
                {
                    SerialDebug.println("Entered Positioning Page (Handler TBD)");
                }
            }
            packet_handled_by_page_handler = true;
        }
        else
        {
            switch (currentPage)
            {
            case PAGE_SETUP:
                SetupPageHandler::handlePacket(packet);
                packet_handled_by_page_handler = true;
                break;
            case PAGE_TURNING:
                if (packet->address == 192 && packet->type == kBool && packet->data._bool)
                {
                    SerialDebug.println("MainLoop: Zero Z-Pos button pressed on Turning Page.");
                    if (menuSystem.getTurningMode())
                    {
                        menuSystem.getTurningMode()->setZeroPosition();
                    }
                    packet_handled_by_page_handler = true;
                }
                else
                {
                    TurningPageHandler::handlePacket(packet, menuSystem.getTurningMode(), menuSystem.getDisplayComm(), &motionCtrl);
                    packet_handled_by_page_handler = true;
                }
                break;
            case PAGE_JOG:
                JogPageHandler::handlePacket(packet);
                packet_handled_by_page_handler = true;
                break;
            case PAGE_THREADING:
                if (packet->address == 192 && packet->type == kBool && packet->data._bool)
                {
                    SerialDebug.println("MainLoop: Zero Z-Pos button pressed on Threading Page.");
                    if (menuSystem.getThreadingMode())
                    {
                        menuSystem.getThreadingMode()->setZeroPosition();
                    }
                    packet_handled_by_page_handler = true;
                }
                else
                {
                    ThreadingPageHandler::handlePacket(packet);
                    packet_handled_by_page_handler = true;
                }
                break;
            default:
                SerialDebug.print("MainLoop: Unhandled/Global packet address: ");
                SerialDebug.println(packet->address);
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

// TIM2_IRQHandler is defined by the Arduino STM32 framework (in HardwareTimer.cpp).
// We must not redefine it here.
