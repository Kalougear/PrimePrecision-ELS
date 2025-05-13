#include "UI/DisplayComm.h"
#include "Config/serial_debug.h"

DisplayComm::DisplayComm()
    : _serial(nullptr),
      _currentScreen(ScreenIDs::MAIN_SCREEN),
      _buttonHandler(nullptr)
{
}

DisplayComm::~DisplayComm()
{
    end();
}

bool DisplayComm::begin(HardwareSerial *serial)
{
    if (!serial)
    {
        SerialDebug.println("Invalid serial port for display");
        return false;
    }

    _serial = serial;
    _serial->begin(115200);
    delay(100);

    SerialDebug.println("Display communication initialized");

    // Show main screen on startup
    showScreen(ScreenIDs::MAIN_SCREEN);

    return true;
}

void DisplayComm::end()
{
    if (_serial)
    {
        _serial = nullptr;
    }
}

void DisplayComm::showScreen(uint8_t screen_id)
{
    if (!_serial)
        return;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "page %d", screen_id);
    sendCommand(cmd);

    _currentScreen = screen_id;

    SerialDebug.print("Switched to screen: ");
    SerialDebug.println(screen_id);
}

void DisplayComm::updateText(uint8_t text_id, const char *text)
{
    if (!_serial)
        return;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"", text_id, text);
    sendCommand(cmd);
}

void DisplayComm::updateText(uint8_t text_id, float value, uint8_t decimals)
{
    if (!_serial)
        return;

    char valueStr[16];
    char cmd[32];

    // Format float with specified decimal places
    switch (decimals)
    {
    case 0:
        snprintf(valueStr, sizeof(valueStr), "%d", (int)value);
        break;
    case 1:
        snprintf(valueStr, sizeof(valueStr), "%.1f", value);
        break;
    case 2:
        snprintf(valueStr, sizeof(valueStr), "%.2f", value);
        break;
    case 3:
        snprintf(valueStr, sizeof(valueStr), "%.3f", value);
        break;
    default:
        snprintf(valueStr, sizeof(valueStr), "%.2f", value);
        break;
    }

    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"", text_id, valueStr);
    sendCommand(cmd);
}

void DisplayComm::setButtonHandler(ButtonHandler handler)
{
    _buttonHandler = handler;
}

void DisplayComm::processIncoming()
{
    if (!_serial)
        return;

    // Process all available data
    while (_serial->available() > 0)
    {
        uint8_t data = _serial->read();
        handleRxData(data);
    }
}

void DisplayComm::showStatus(const char *text, bool error)
{
    updateText(TextIDs::STATUS_TEXT, text);

    // If this is an error, also log to debug
    if (error)
    {
        SerialDebug.print("ERROR: ");
        SerialDebug.println(text);
    }
}

void DisplayComm::sendCommand(const char *cmd)
{
    if (!_serial)
        return;

    // Send command string
    _serial->print(cmd);

    // Send command terminator (three bytes)
    _serial->write(0xFF);
    _serial->write(0xFF);
    _serial->write(0xFF);

    // Debug output
    SerialDebug.print("Display CMD: ");
    SerialDebug.println(cmd);
}

void DisplayComm::handleRxData(uint8_t data)
{
    // Simple protocol: button presses send a single byte with button ID
    if (_buttonHandler)
    {
        _buttonHandler(data);
    }

    // Debug output
    SerialDebug.print("Display button pressed: ");
    SerialDebug.println(data);
}
