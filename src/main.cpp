// File: src/main.cpp (Implementing Interrupt-Driven Serial RX)

#include "LumenProtocol.h" // Still at the top, but without extern "C" wrapper
#include <Arduino.h>
#include <cmath> // For abs()
#include "Config/serial_debug.h"
#include "Hardware/SystemClock.h"
#include "Config/SystemConfig.h"
#include <STM32Step.h>
#include "Hardware/EncoderTimer.h"
#include "Motion/MotionControl.h"
#include "Motion/FeedRateManager.h"
// #include "Config/HmiInputOptions.h" // This was commented out, but we need it now.
#include "Config/HmiInputOptions.h"              // For ADDR_TURNING_MM_INCH_...
#include "UI/DisplayComm.h"                      // For DisplayComm class
#include "UI/MenuSystem.h"                       // For MenuSystem class
#include "UI/HmiHandlers/SetupPageHandler.h"     // Include the new Setup Page Handler
#include "UI/HmiHandlers/TurningPageHandler.h"   // Include the new Turning Page Handler
#include "UI/HmiHandlers/JogPageHandler.h"       // Include the new Jog Page Handler
#include "UI/HmiHandlers/ThreadingPageHandler.h" // Include the new Threading Page Handler

// --- End Circular Buffer --- // Placeholder for removed code

// Enum and global variable for tracking the active HMI page
enum ActiveHmiPage
{
    PAGE_UNKNOWN = 0,
    PAGE_TURNING = 1, // Main turning page functions
    PAGE_THREADING = 2,
    PAGE_POSITIONING = 3,
    PAGE_SETUP = 4,
    PAGE_JOG = 5 // Dedicated Jog Page
};
ActiveHmiPage currentPage = PAGE_TURNING;      // Default to Turning page, or set based on HMI initial signal
const uint16_t int_tab_selectionAddress = 136; // HMI address for page change notifications

// const uint16_t rebootAddress = 102;         // No longer used for HMI readiness detection

static const uint8_t ACTUAL_STEP_PIN = STM32Step::PinConfig::StepPin::PIN;
static const uint8_t ACTUAL_DIR_PIN = STM32Step::PinConfig::DirPin::PIN;
static const uint8_t ACTUAL_ENABLE_PIN = STM32Step::PinConfig::EnablePin::PIN;

HardwareSerial SerialDebug(PD9, PD8); // USART3_RX: PD9, USART3_TX: PD8. NOTE: Assumes HardwareSerial constructor handles GPIO init for these pins for USART3.
HardwareSerial SerialDisplay(PA10, PA9);
DisplayComm displayComm; // Global DisplayComm instance
MenuSystem menuSystem;   // Global MenuSystem instance
// static UART_HandleTypeDef *pSerialDisplayHuart = nullptr; // Removed

const uint16_t mmInchSelectorAddress = 124;
const uint16_t directionSelectorAddress = 129;
const uint16_t startSTOPFEEDAddress = 130;
const uint16_t rmpAddress = 131;
const uint16_t actualFEEDRATEAddress = 132;
const uint16_t prevNEXTFEEDRATEVALUEAddress = 133;
const uint16_t actualFEEDRATEDESCRIPTIONAddress = 134;

// All Setup Tab HMI addresses are now in include/Config/Hmi/SetupPageOptions.h
// and handled by SetupPageHandler.cpp

// HMI Addresses for "Main Page" or common elements (can be moved later if desired)
// const uint16_t mmInchSelectorAddress = 124; // Already defined
// const uint16_t directionSelectorAddress = 129; // Already defined
// const uint16_t startSTOPFEEDAddress = 130; // Already defined
// const uint16_t rmpAddress = 131; // Already defined
// const uint16_t actualFEEDRATEAddress = 132; // Already defined
// const uint16_t prevNEXTFEEDRATEVALUEAddress = 133; // Already defined
// const uint16_t actualFEEDRATEDESCRIPTIONAddress = 134; // Already defined

lumen_packet_t mmInchSelectorPacket = {mmInchSelectorAddress, kS32};
// lumen_packet_t directionSelectorPacket = {directionSelectorAddress, kS32}; // Example if needed
lumen_packet_t startSTOPFEEDPacket = {startSTOPFEEDAddress, kBool};
lumen_packet_t rmpPacket = {rmpAddress, kS32};
lumen_packet_t actualFEEDRATEPacket = {actualFEEDRATEAddress, kString};
lumen_packet_t prevNEXTFEEDRATEVALUEPacket = {prevNEXTFEEDRATEVALUEAddress, kS32};
lumen_packet_t actualFEEDRATEDESCRIPTIONPacket = {actualFEEDRATEDESCRIPTIONAddress, kString};

FeedRateManager feedRateManager; // Moved definition up
char feedRateBuffer[40];         // Moved definition up

// Helper function to send initial/main page feed rate display
void sendMainPageFeedRateDisplay()
{
    feedRateManager.getDisplayString(feedRateBuffer, sizeof(feedRateBuffer));
    memcpy(actualFEEDRATEPacket.data._string, feedRateBuffer, MAX_STRING_SIZE - 1);
    actualFEEDRATEPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEPacket);

    const char *initialCategory = feedRateManager.getCurrentCategory(); // Or current category
    strncpy(actualFEEDRATEDESCRIPTIONPacket.data._string, initialCategory, MAX_STRING_SIZE - 1);
    actualFEEDRATEDESCRIPTIONPacket.data._string[MAX_STRING_SIZE - 1] = '\0';
    lumen_write_packet(&actualFEEDRATEDESCRIPTIONPacket);
    SerialDebug.println("MainLoop: Sent main page feed rate display.");
}

EncoderTimer globalEncoderTimerInstance;
MotionControl motionCtrl(MotionControl::MotionPins{ACTUAL_STEP_PIN, ACTUAL_DIR_PIN, ACTUAL_ENABLE_PIN});

volatile bool g_exti_pa5_index_pulse_detected = false; // Flag for EXTI-based index pulse
volatile unsigned long g_last_pa5_interrupt_time = 0;  // For debouncing PA5 EXTI
const unsigned long PA5_DEBOUNCE_DELAY_MS = 5;         // Debounce delay for PA5 EXTI in milliseconds

// HAL Callback for Timer Trigger Events (e.g., ETR for Index Pulse) // Temporarily Commented Out
// void HAL_TIM_TriggerCallback(TIM_HandleTypeDef *htim)
// {
//     if (htim->Instance == TIM2) // Check if it's TIM2
//     {
//         // Check if the Trigger flag is set (this is for ETR, Hall sensor, etc.)
//         // It's important to check the specific flag if multiple trigger sources could use this callback.
//         if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_TRIGGER) != RESET)
//         {
//             __HAL_TIM_CLEAR_IT(htim, TIM_IT_TRIGGER);                  // Clear the interrupt flag
//             globalEncoderTimerInstance.IndexPulse_Callback_Internal(); // Call our handler
//         }
//     }
// }

// FeedRateManager feedRateManager; // Definition moved up
// char feedRateBuffer[40];         // Definition moved up
// char hmiDisplayStringBuffer[40]; // This buffer is now static within SetupPageHandler.cpp

// Option lists and their current indices are now managed within SetupPageHandler.cpp
// uint8_t currentPprIndex = 0;
// uint8_t currentZLeadscrewPitchIndex = 0;
// uint8_t currentZDriverMicrosteppingIndex = 0;

// find_initial_index is now static within SetupPageHandler.cpp
// template <typename T>
// uint8_t find_initial_index(const T *list, uint8_t list_size, T target_value)
// {
//     for (uint8_t i = 0; i < list_size; ++i)
//     {
//         if (list[i] == target_value)
//             return i;
//     }
//     return 0;
// }

extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length)
{
    SerialDisplay.write(data, length);
}

// --- ISR Debug Counters (declared in their respective .cpp files) ---

// User ISR for PA5 Index Pulse with Debouncing
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
{ // Needed for linker to find these
    extern volatile uint32_t g_isr_call_count;
    extern volatile uint32_t g_stepper_isr_entry_count;
}
uint32_t last_g_isr_call_count = 0;
uint32_t last_g_stepper_isr_entry_count = 0;
uint32_t last_isr_print_time = 0;

// --- HMI Serial Input Buffer ---
const uint16_t HMI_SERIAL_INPUT_BUFFER_SIZE = 256;
static uint8_t hmi_serial_input_buffer[HMI_SERIAL_INPUT_BUFFER_SIZE];
static uint16_t hmi_buffer_write_idx = 0; // Points to the next empty spot
static uint16_t hmi_buffer_read_idx = 0;  // Points to the next byte to be read by Lumen

// lumen_get_byte now reads from our software buffer
extern "C" uint16_t lumen_get_byte()
{
    if (hmi_buffer_read_idx < hmi_buffer_write_idx)
    {
        return hmi_serial_input_buffer[hmi_buffer_read_idx++];
    }
    return DATA_NULL; // No more bytes in our software buffer for Lumen
}

// ISR for USART1 (SerialDisplay) // Removed
// void USART1_IRQHandler(void) // Removed
// { // Removed
// } // Removed

void setup()
{
    SerialDebug.begin(115200);
    HAL_Delay(3000); // Wait 5 seconds for HMI to initialize
    // buffer_init(&rxBuffer); // Removed

    SerialDisplay.begin(115200, SERIAL_8N1);
    // pSerialDisplayHuart = SerialDisplay.getHandle(); // Removed

    // --- Enable USART1 Receive Interrupt --- // Removed
    // if (pSerialDisplayHuart != nullptr && pSerialDisplayHuart->Instance == USART1) // Removed
    // { // Removed
    // } // Removed
    // else // Removed
    // { // Removed
    // } // Removed
    // --- End Interrupt Enable --- // Removed

    if (!SystemConfig::ConfigManager::initialize())
    {
        SerialDebug.println("CRITICAL ERROR: System Configuration initialization failed!");
        while (1)
            ; // Halt
    }

    // Initialize current indices for cyclable lists (now done in SetupPageHandler::init())
    // currentPprIndex = find_initial_index(HmiInputOptions::pprList, HmiInputOptions::pprListSize, SystemConfig::RuntimeConfig::Encoder::ppr);
    // currentZLeadscrewPitchIndex = find_initial_index(HmiInputOptions::zLeadscrewPitchList, HmiInputOptions::zLeadscrewPitchListSize, SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch);
    // currentZDriverMicrosteppingIndex = find_initial_index(HmiInputOptions::zDriverMicrosteppingList, HmiInputOptions::zDriverMicrosteppingListSize, SystemConfig::RuntimeConfig::Z_Axis::driver_pulses_per_rev);

    if (!SystemClock::GetInstance().initialize())
    {
        SerialDebug.println("CRITICAL ERROR: SystemClock initialization failed!");
        while (1)
            ; // Halt
    }

    if (!globalEncoderTimerInstance.begin())
    {
        SerialDebug.println("CRITICAL ERROR: globalEncoderTimerInstance.begin() failed!");
        while (1)
            ; // Halt
    }

    // Configure PA5 for EXTI Index Pulse using Arduino API
    pinMode(PA5, INPUT); // INPUT or INPUT_PULLUP. Using INPUT as external pull-up is expected.
                         // PA5 is 5V tolerant. External pull-up to 5V is fine.
    // Ensure PA5 is mapped to an interrupt number by the framework
    if (digitalPinToInterrupt(PA5) != NOT_AN_INTERRUPT)
    {
        attachInterrupt(digitalPinToInterrupt(PA5), pa5_index_pulse_isr, FALLING);
        SerialDebug.println("DEBUG: Attached EXTI for PA5 Index Pulse.");
    }
    else
    {
        SerialDebug.println("CRITICAL ERROR: PA5 cannot be used as an interrupt pin!");
        while (1)
            ; // Halt
    }

    if (!displayComm.begin(&SerialDisplay)) // Initialize DisplayComm
    {
        SerialDebug.println("CRITICAL ERROR: DisplayComm begin failed!");
        while (1)
            ; // Halt
    }

    if (!motionCtrl.begin())
    {
        SerialDebug.println("CRITICAL ERROR: MotionControl begin failed!");
        while (1)
            ; // Halt
    }

    MotionControl::Config cfg;
    cfg.thread_pitch = 0.5f;
    cfg.leadscrew_pitch = SystemConfig::RuntimeConfig::Z_Axis::lead_screw_pitch;
    cfg.steps_per_rev = SystemConfig::Limits::Stepper::STEPS_PER_REV;
    cfg.microsteps = SystemConfig::RuntimeConfig::Stepper::microsteps;
    cfg.reverse_direction = false;
    cfg.sync_frequency = SystemConfig::RuntimeConfig::Motion::sync_frequency;
    motionCtrl.setConfig(cfg);
    motionCtrl.setMode(MotionControl::Mode::TURNING);

    if (!menuSystem.begin(&displayComm, &motionCtrl)) // Initialize MenuSystem
    {
        SerialDebug.println("CRITICAL ERROR: MenuSystem begin failed!");
        while (1)
            ; // Halt
    }
    // Set the static packet handler for MenuSystem if it's used for Lumen packet routing
    // displayComm.setPacketHandler(MenuSystem::staticPacketHandler); // Or however MenuSystem gets packets

    // Initialize Page Handlers AFTER MenuSystem has initialized its modes
    SetupPageHandler::init();
    TurningPageHandler::init(menuSystem.getTurningMode());                                // Pass TurningMode instance
    JogPageHandler::init(&motionCtrl);                                                    // Initialize JogPageHandler
    ThreadingPageHandler::init(&displayComm, menuSystem.getThreadingMode(), &motionCtrl); // Initialize ThreadingPageHandler, added &motionCtrl
    // Future: PositioningPageHandler::init();

    // Explicitly set the initial screen AFTER DisplayComm is initialized
    // and currentPage has its desired default value (PAGE_TURNING).
    displayComm.showScreen(currentPage);
    SerialDebug.print("Initial HMI screen explicitly set to page ID: ");
    SerialDebug.println(static_cast<int>(currentPage));

    rmpPacket.data._s32 = 0;
    lumen_write_packet(&rmpPacket);

    feedRateManager.setMetric(SystemConfig::RuntimeConfig::System::measurement_unit_is_metric);
    sendMainPageFeedRateDisplay(); // Call helper function

    // Initial HMI state for the default page is now handled by the page handler's onEnterPage()
    // For example, if Setup is the default page:
    // if (currentPage == PAGE_SETUP) // Initial page data send will happen after delay
    // {
    //     SetupPageHandler::onEnterPage();
    // }
    // else if (currentPage == PAGE_TURNING) { TurningPageHandler::onEnterPage(); } // etc.

    motionCtrl.startMotion();
    SerialDebug.println("STM32 Setup complete. Waiting 5s for HMI to stabilize before sending initial data...");

    // Now send initial data based on the default/current page
    SerialDebug.print("Resending data for initial page: ");
    SerialDebug.println(currentPage);
    sendMainPageFeedRateDisplay(); // Always send main page common elements

    // Proactively send Setup Page data during MCU startup,
    // ensuring HMI fields are populated if it defaults to or quickly navigates to Setup Page.
    SerialDebug.println("Proactively sending initial data for Setup Page...");
    SetupPageHandler::onEnterPage();

    // Proactively send Turning Page data during MCU startup
    SerialDebug.println("Proactively sending initial data for Turning Page...");
    TurningPageHandler::onEnterPage(menuSystem.getTurningMode()); // Pass TurningMode instance

    // Proactively send Jog Page data during MCU startup
    SerialDebug.println("Proactively sending initial data for Jog Page...");
    JogPageHandler::onEnterPage();

    // The following conditional block for initial page data sending is now largely handled
    // by the proactive onEnterPage calls above for Setup and Turning pages.
    // If other pages are added, their onEnterPage might be called here based on `currentPage`.
    if (currentPage == PAGE_SETUP)
    {
        SerialDebug.println("Initial page is Setup. Specific Setup Page data sent proactively.");
    }
    else if (currentPage == PAGE_TURNING) // This will also cover PAGE_JOG if it's the default
    {
        SerialDebug.println("Initial page is Turning/Jog. Specific Page data sent proactively.");
    }
    else if (currentPage == PAGE_JOG)
    {
        SerialDebug.println("Initial page is Jog. Specific Jog Page data sent proactively.");
    } // Add other pages if necessary

    SerialDebug.println("Initial HMI data sent. Entering main loop.");
}

void loop()
{
    static uint32_t lastRpmHmiUpdateTime = 0;
    static uint32_t lastDroUpdateTime = 0;
    const uint32_t RPM_HMI_UPDATE_INTERVAL = 250; // ms - Adjusted as per user feedback
    const uint32_t DRO_UPDATE_INTERVAL = 200;     // ms, adjust as needed for responsiveness vs. load

    uint32_t currentTime = millis();

    // Check for Encoder Index Pulse via EXTI PA5
    if (g_exti_pa5_index_pulse_detected)
    {
        SerialDebug.println("MainLoop: Index Pulse Detected (EXTI PA5)!");
        g_exti_pa5_index_pulse_detected = false; // Reset flag

        // Optional: Software reset of TIM2 counter or log count
        // globalEncoderTimerInstance.reset();
        // int32_t countAtPulse = globalEncoderTimerInstance.getCount();
        // SerialDebug.print("Count at EXTI pulse: "); SerialDebug.println(countAtPulse);
    }

    // Check for Encoder Index Pulse // Temporarily Commented Out
    // if (globalEncoderTimerInstance.hasIndexPulseOccurred())
    // {
    //     SerialDebug.println("MainLoop: Index Pulse Detected (TIM2_ETR on PA5)!");
    // }

    // Print ISR counters periodically for debugging jog
    if (motionCtrl.isJogActive() && (currentTime - last_isr_print_time > 500)) // Print every 500ms if jog is active
    {
        uint32_t current_timer_isr_calls = STM32Step::g_isr_call_count;              // Qualify with namespace
        uint32_t current_stepper_isr_entries = STM32Step::g_stepper_isr_entry_count; // Qualify with namespace

        SerialDebug.print("ISR Counts since last print: TimerCB=");
        SerialDebug.print(current_timer_isr_calls - last_g_isr_call_count);
        SerialDebug.print(", StepperISR=");
        SerialDebug.println(current_stepper_isr_entries - last_g_stepper_isr_entry_count);

        last_g_isr_call_count = current_timer_isr_calls;
        last_g_stepper_isr_entry_count = current_stepper_isr_entries;
        last_isr_print_time = currentTime;
    }

    // Update RPM (conditionally, not if Turning Page is active, as it will handle its own RPM)
    if (currentPage != PAGE_TURNING && (currentTime - lastRpmHmiUpdateTime >= RPM_HMI_UPDATE_INTERVAL))
    {
        MotionControl::Status mcStatus = motionCtrl.getStatus();
        // Ensure RPM is always positive for display
        int32_t currentRpmBeforeAbs = mcStatus.spindle_rpm; // Store before abs for debug
        rmpPacket.data._s32 = abs(currentRpmBeforeAbs);
        lumen_write_packet(&rmpPacket);
        lastRpmHmiUpdateTime = currentTime;
    }

    // Update Turning Page DRO if active (and not on Jog page, assuming DRO is not on Jog page)
    if (currentPage == PAGE_TURNING) // Not PAGE_JOG
    {
        // Call the new TurningPageHandler::update() which now includes DRO update and auto-stop check
        TurningPageHandler::update(menuSystem.getTurningMode(), menuSystem.getDisplayComm(), &motionCtrl); // Pass motionCtrl again
        // The DRO_UPDATE_INTERVAL logic is now effectively handled within TurningPageHandler::update if it calls updateDRO conditionally,
        // or updateDRO can be called unconditionally if its internal logic is efficient.
        // For simplicity, let's assume TurningPageHandler::update handles its own timing or is called frequently enough.
        // If specific timing for DRO is still needed outside of the general handler update, it can be re-added.
        // For now, the single call to TurningPageHandler::update() covers its periodic tasks.
    }
    else if (currentPage == PAGE_THREADING)
    {
        ThreadingPageHandler::update(); // Call update for ThreadingPage
    }

    // 1. Read all available data from SerialDisplay into our software buffer
    while (SerialDisplay.available() > 0 && hmi_buffer_write_idx < HMI_SERIAL_INPUT_BUFFER_SIZE)
    {
        uint8_t byte_received = SerialDisplay.read();
        // SerialDebug.print("Byte from HMI: 0x");
        // SerialDebug.println(byte_received, HEX); // DEBUG LINE - uncomment to see raw bytes
        hmi_serial_input_buffer[hmi_buffer_write_idx++] = byte_received;
    }

    // 2. Drive the Lumen parser by calling lumen_available().
    //    This will consume bytes from hmi_serial_input_buffer via our lumen_get_byte()
    //    and populate Lumen's internal packet queue if full packets are formed.
    if (hmi_buffer_write_idx > 0) // Only call if there's data in our buffer
    {
        lumen_available(); // This call drives the parsing process.
    }

    // 3. Now check for and process any fully parsed packets from Lumen's internal queue
    lumen_packet_t *packet = nullptr;
    while ((packet = lumen_get_first_packet()) != NULL)
    {
        bool packet_handled_by_page_handler = false;

        // Removed HMI Reboot signal handler (address 102) as it's not an incoming signal for readiness.
        // Startup delay is now in setup().

        // Page change packet
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
                // Call onExitPage for the *old* page if applicable
                if (currentPage == PAGE_JOG)
                {
                    JogPageHandler::onExitPage(); // This will now handle saving the jog speed index if changed
                    HAL_Delay(10);                // Diagnostic delay after potential EEPROM write from JogPage exit
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onExitPage(menuSystem.getTurningMode());
                    SerialDebug.println("MainLoop: Called TurningPageHandler::onExitPage().");
                }
                else if (currentPage == PAGE_THREADING)
                {
                    ThreadingPageHandler::onExitPage(); // Assumes ThreadingPageHandler::onExitPage() takes no args or gets its own instance
                    SerialDebug.println("MainLoop: Called ThreadingPageHandler::onExitPage().");
                }
                // Add other onExitPage calls here for other pages (e.g. SetupPageHandler::onExitPage())
                else
                {
                    // Potentially add delays for other page exits if they also save to EEPROM and cause issues
                }

                currentPage = newPage;

                SerialDebug.print("MainLoop: Internal currentPage variable successfully updated to: ");
                SerialDebug.println(static_cast<int>(currentPage));

                // Call onEnterPage for the new page to send its initial state
                if (currentPage == PAGE_SETUP)
                {
                    SetupPageHandler::onEnterPage();
                }
                else if (currentPage == PAGE_TURNING)
                {
                    TurningPageHandler::onEnterPage(menuSystem.getTurningMode());
                    // Set HMI direction button display (address 210) to "true" (RH/Towards Chuck) state
                    const uint16_t HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS = 210;          // User specified address
                    displayComm.updateText(HMI_DIRECTION_BUTTON_DISPLAY_ADDRESS, true); // Send bool true
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
            packet_handled_by_page_handler = true; // Page change is a global concern handled here
        }
        else
        {
            // Route packet to the handler for the current page
            switch (currentPage)
            {
            case PAGE_SETUP:
                SetupPageHandler::handlePacket(packet);
                packet_handled_by_page_handler = true;
                break;
            case PAGE_TURNING:
                // Handle main page turning controls if not handled by TurningPageHandler
                if (packet->address == 192 && packet->type == kBool && packet->data._bool) // bool_zero_z_posAddress
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
                    // Pass DisplayComm and MotionControl instances to TurningPageHandler::handlePacket
                    // TurningPageHandler::handlePacket(packet, menuSystem.getTurningMode(), menuSystem.getDisplayComm(), &motionCtrl); // DISABLED FOR HMI RX TEST
                    // Pass DisplayComm and MotionControl instances to TurningPageHandler::handlePacket
                    TurningPageHandler::handlePacket(packet, menuSystem.getTurningMode(), menuSystem.getDisplayComm(), &motionCtrl);
                    packet_handled_by_page_handler = true;
                }
                break;
            case PAGE_JOG:
                JogPageHandler::handlePacket(packet);
                packet_handled_by_page_handler = true;
                break;
            case PAGE_THREADING:
                if (packet->address == 192 && packet->type == kBool && packet->data._bool) // bool_zero_z_posAddress
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
            // ... other page cases ...
            default:
                // Packets not handled by a specific page handler might be global or unassigned
                SerialDebug.print("MainLoop: Unhandled/Global packet address: ");
                SerialDebug.println(packet->address);
                break;
            }
        }

        // Common updates that might happen after any relevant HMI interaction (e.g., feed rate display)
        // This logic is now removed from here for PAGE_TURNING as TurningPageHandler handles its own displays.
        // If other pages need a similar global update mechanism, this might need to be re-evaluated
        // or made more specific, e.g., by checking `if (packet_handled_by_page_handler && currentPage != PAGE_TURNING)`
        // or by having each page handler be fully responsible for all its display updates.
        // For now, removing the general update block to avoid duplicate sends for Turning Page.
        // If `sendMainPageFeedRateDisplay()` is still needed for other pages, it should be called conditionally.

        // IMPORTANT: If lumen_get_first_packet() returns NULL, it means either no more data
        // in our hmi_serial_input_buffer (hmi_buffer_read_idx == hmi_buffer_write_idx)
        // or the remaining data does not form a complete packet.
    }

    // 3. Manage the software buffer after processing
    if (hmi_buffer_read_idx >= hmi_buffer_write_idx)
    {
        // All buffered data has been processed (or attempted) by Lumen
        hmi_buffer_read_idx = 0;
        hmi_buffer_write_idx = 0;
    }
    else if (hmi_buffer_read_idx > 0) // Some data processed, but some remains (likely partial packet)
    {
        // Shift unprocessed data to the beginning of the buffer
        uint16_t remaining_bytes = hmi_buffer_write_idx - hmi_buffer_read_idx;
        memmove(hmi_serial_input_buffer, &hmi_serial_input_buffer[hmi_buffer_read_idx], remaining_bytes);
        hmi_buffer_write_idx = remaining_bytes;
        hmi_buffer_read_idx = 0;
        // SerialDebug.println("Shifted remaining HMI bytes.");
    }
    // If hmi_buffer_read_idx is 0 but hmi_buffer_write_idx > 0, it means Lumen couldn't parse
    // anything from the current buffer content. This could happen with a partial packet
    // that's waiting for more data, or continuous garbage. The data remains for the next cycle.
    // If the buffer becomes full (hmi_buffer_write_idx == HMI_SERIAL_INPUT_BUFFER_SIZE) and
    // Lumen still can't parse (hmi_buffer_read_idx remains 0), then new incoming serial data will be lost
    // until Lumen can clear some space. This indicates a persistent parsing issue or data corruption.
}

// TIM2_IRQHandler is defined by the Arduino STM32 framework (in HardwareTimer.cpp).
// We must not redefine it here.
// The framework's TIM2_IRQHandler should call HAL_TIM_IRQHandler for the TIM2 handle.
// HAL_TIM_IRQHandler will then call HAL_TIM_IC_CaptureCallback.
// The framework's HAL_TIM_IC_CaptureCallback should then call our user-defined C-style callback
// if one is registered for TIM2_CH3.
