#include "UI/DisplayComm.h"
#include "Config/serial_debug.h"

// Removed static g_lumen_serial_port and the extern "C" implementations
// of lumen_get_byte() and lumen_write_bytes().
// These will now be provided by main.cpp for this test.

DisplayComm::DisplayComm()
    : _serial(nullptr),
      _currentScreen(ScreenIDs::MAIN_SCREEN),
      _packetHandler(nullptr) // Changed from _buttonHandler
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
    // g_lumen_serial_port = _serial; // Removed: main.cpp will handle byte provision
    _serial->begin(115200);
    delay(100);

    SerialDebug.println("Display communication initialized (Lumen funcs from main.cpp)");

    // Show main screen on startup - This will be handled by main.cpp after setting currentPage
    // showScreen(ScreenIDs::MAIN_SCREEN);

    return true;
}

void DisplayComm::end()
{
    if (_serial)
    {
        _serial = nullptr;
        // g_lumen_serial_port = nullptr; // Removed
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

// Implementation for bool overload
void DisplayComm::updateText(uint8_t id, bool value)
{
    if (!_serial) // Or check if Lumen is initialized if that's a separate step
        return;

    lumen_packet_t packet_to_send;
    packet_to_send.address = id;
    packet_to_send.type = kBool; // Lumen type for boolean
    packet_to_send.data._bool = value;

    lumen_write_packet(&packet_to_send); // Assumes lumen_write_packet is globally available or via a Lumen instance

    // Optional: Debug logging
    SerialDebug.print("DisplayComm: Sent bool to HMI Addr=");
    SerialDebug.print(id);
    SerialDebug.print(", Value=");
    SerialDebug.println(value ? "true" : "false");
}

// Changed from setButtonHandler to setPacketHandler
void DisplayComm::setPacketHandler(PacketHandler handler)
{
    _packetHandler = handler; // Changed from _buttonHandler
}

void DisplayComm::processIncoming()
{
    if (!_serial) // Or check g_lumen_serial_port
        return;

    // lumen_available() drives the Lumen Protocol parser by calling
    // our lumen_get_byte() internally to consume serial data.
    // It returns the number of fully parsed packets ready.
    lumen_available();

    lumen_packet_t *packet;
    // Retrieve all fully parsed packets
    while ((packet = lumen_get_first_packet()) != nullptr)
    {
        if (_packetHandler)
        {
            _packetHandler(packet);
        }
// Optional: Add detailed debug logging for received Lumen packets
#if DEBUG_LEVEL > 1 // Example conditional debug
        SerialDebug.print("Lumen Pkt RX: Addr=0x");
        SerialDebug.print(packet->address, HEX);
        SerialDebug.print(", TypeVal="); // Type is not reliably set by LumenProtocol.c on RX
        SerialDebug.print(packet->type);
        SerialDebug.print(", Data (HEX):");

        // Print first few bytes of data payload as HEX
        // MAX_STRING_SIZE is 40, data union starts with _string.
        // LumenProtocol.c's Pack() copies data into packet->data._string.
        // Let's print up to 8 bytes or MAX_STRING_SIZE, whichever is smaller.
        // Note: The actual data length isn't directly available here from packet struct alone
        // without knowing how LumenProtocol.c determined it (it used _dataIndex - kData).
        // For S32, it's 4 bytes. For strings, it's variable.
        // We'll just peek at the beginning of the data buffer.
        int bytes_to_print = 0;
        // Heuristic: if address is 133 (prev/next), it's likely 4 bytes for S32.
        // If address is 132 (feedrate string), it could be longer.
        // For generic logging, let's show a fixed number, e.g., 8 bytes.
        // The actual data length is not stored in lumen_packet_t by LumenProtocol.c's Pack().
        // It only copies data into data._string.

        // A simple approach: print up to 8 bytes from the data union's char array.
        // This shows the raw start of the payload.
        for (int i = 0; i < 8 && i < MAX_STRING_SIZE; ++i)
        {
            SerialDebug.print(" ");
            if (packet->data._string[i] < 0x10)
                SerialDebug.print("0");
            SerialDebug.print((unsigned char)packet->data._string[i], HEX);
        }
        SerialDebug.println();
#endif
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

    // TODO: Implement this using lumen_write_packet() or lumen_write().
    // This requires knowing the Lumen Protocol addresses for HMI screen/text objects.
    // The old method of sending Nextion-like commands will likely not work
    // if the HMI is strictly Lumen Protocol based.
    // For now, just log that it was called.
    if (!_serial)
        return;

    SerialDebug.print("Display CMD (Lumen - '");
    SerialDebug.print(cmd);
    SerialDebug.println("' - needs full Lumen implementation)");

    // Old code (Nextion-style):
    // _serial->print(cmd);
    // _serial->write(0xFF);
    // _serial->write(0xFF);
    // _serial->write(0xFF);
    // SerialDebug.print("Original Display CMD: ");
    // SerialDebug.println(cmd);
}

// Removed handleRxData(uint8_t data) as it's replaced by Lumen Protocol parsing
// void DisplayComm::handleRxData(uint8_t data)
// {
//    ...
// }
