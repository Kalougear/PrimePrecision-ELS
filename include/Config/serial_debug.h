#pragma once

#include <HardwareSerial.h>

// Define the SerialDebug globally accessible
extern HardwareSerial SerialDebug;

// Debug macro helpers
#define DEBUG_ENABLE 1

#if DEBUG_ENABLE
#define DEBUG_PRINT(x) SerialDebug.print(x)
#define DEBUG_PRINTLN(x) SerialDebug.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif