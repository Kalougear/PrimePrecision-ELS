#pragma once

#include <Arduino.h>

// Screen ID definitions
struct ScreenIDs {
    static constexpr uint8_t MAIN_SCREEN = 0;
    static constexpr uint8_t TURNING_SCREEN = 1;
    static constexpr uint8_t THREADING_SCREEN = 2;
    static constexpr uint8_t SETUP_SCREEN = 3;
};

// Text field IDs
struct TextIDs {
    // Main screen text IDs
    static constexpr uint8_t RPM_VALUE = 0;
    static constexpr uint8_t STATUS_TEXT = 1;
    
    // Turning screen text IDs
    static constexpr uint8_t TURNING_FEEDRATE = 10;
    static constexpr uint8_t TURNING_POSITION = 11;
    static constexpr uint8_t TURNING_DISTANCE = 12;
    
    // Threading screen text IDs
    static constexpr uint8_t THREAD_PITCH = 20;
    static constexpr uint8_t THREAD_STARTS = 21;
    static constexpr uint8_t THREAD_POSITION = 22;
    static constexpr uint8_t THREAD_TYPE = 23;
    
    // Setup screen text IDs
    static constexpr uint8_t LEADSCREW_PITCH = 30;
    static constexpr uint8_t MICROSTEPS = 31;
    static constexpr uint8_t BACKLASH = 32;
};

// Button IDs
struct ButtonIDs {
    // Main screen buttons
    static constexpr uint8_t TURNING_BTN = 0;
    static constexpr uint8_t THREADING_BTN = 1;
    static constexpr uint8_t SETUP_BTN = 2;
    
    // Turning screen buttons
    static constexpr uint8_t TURNING_START_BTN = 10;
    static constexpr uint8_t TURNING_STOP_BTN = 11;
    static constexpr uint8_t TURNING_FEEDRATE_UP = 12;
    static constexpr uint8_t TURNING_FEEDRATE_DOWN = 13;
    static constexpr uint8_t TURNING_AUTOMODE_BTN = 14;
    static constexpr uint8_t TURNING_SET_END_BTN = 15;
    
    // Threading screen buttons
    static constexpr uint8_t THREADING_START_BTN = 20;
    static constexpr uint8_t THREADING_STOP_BTN = 21;
    static constexpr uint8_t THREADING_PITCH_UP = 22;
    static constexpr uint8_t THREADING_PITCH_DOWN = 23;
    static constexpr uint8_t THREADING_MULTI_BTN = 24;
    static constexpr uint8_t THREADING_UNITS_BTN = 25;
};

// Button event handler function type
using ButtonHandler = void (*)(uint8_t button_id);

class DisplayComm {
public:
    DisplayComm();
    ~DisplayComm();
    
    // Initialization
    bool begin(HardwareSerial* serial);
    void end();
    
    // Screen control
    void showScreen(uint8_t screen_id);
    uint8_t getCurrentScreen() const { return _currentScreen; }
    
    // Text display
    void updateText(uint8_t text_id, const char* text);
    
    // Numeric value updates - use templates to avoid overload ambiguity
    template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    void updateText(uint8_t text_id, T value) {
        char valueStr[16];
        snprintf(valueStr, sizeof(valueStr), "%ld", static_cast<int32_t>(value));
        updateText(text_id, valueStr);
    }
    
    void updateText(uint8_t text_id, float value, uint8_t decimals = 2);
    
    // Button handling
    void setButtonHandler(ButtonHandler handler);
    void processIncoming();
    
    // Status indicator
    void showStatus(const char* text, bool error = false);
    
private:
    HardwareSerial* _serial;
    uint8_t _currentScreen;
    ButtonHandler _buttonHandler;
    
    // Private methods
    void sendCommand(const char* cmd);
    void handleRxData(uint8_t data);
};