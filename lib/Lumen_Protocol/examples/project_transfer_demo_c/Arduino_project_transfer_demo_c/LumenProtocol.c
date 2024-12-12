#include "LumenProtocol.h"

// Version 1.1

extern void lumen_write_bytes(uint8_t *data, uint32_t length);
extern uint16_t lumen_get_byte();

typedef union {
  struct
  {
    uint8_t low;
    uint8_t high;
  } byte;
  uint16_t value;
} u16_union_t;

typedef union {
  struct
  {
    uint8_t low;
    uint8_t high;
  } byte;
  int16_t value;
} s16_union_t;

typedef union {
  struct
  {
    uint8_t lowest;
    uint8_t low;
    uint8_t high;
    uint8_t highest;
  } byte;
  uint32_t value;
} u32_union_t;

typedef union {
  struct
  {
    uint8_t lowest;
    uint8_t low;
    uint8_t high;
    uint8_t highest;
  } byte;
  int32_t value;
} s32_union_t;

typedef union {
  struct
  {
    uint8_t lowest;
    uint8_t low;
    uint8_t high;
    uint8_t highest;
  } byte;
  float value;
} float_union_t;

typedef union {
  struct
  {
    uint8_t _7;
    uint8_t _6;
    uint8_t _5;
    uint8_t _4;
    uint8_t _3;
    uint8_t _2;
    uint8_t _1;
    uint8_t _0;
  } byte;
  double value;
} double_union_t;

typedef enum {
  kCommand,
  kAddressHigh,
  kAddressLow,
  kData,
  kPayloadNull
} PayloadIndex_t;

typedef enum {
  kCommandNull,
  kWrite,
  kRead,
  kReadMultipleVariables
} Command_t;

#if USE_PROJECT_UPDATE
typedef enum lumen_project_update_response {
  kNull,
  kOk,
  kFail,
  kWaitingCommand,
  kReceivingProjectImageBlock
} lumen_project_update_response_t;

volatile bool g_is_updating = false;
#endif

static uint8_t quantityOfPacketsAvailable = 0;

static lumen_packet_t packets[QUANTITY_OF_PACKETS];
static bool occupiedSlots[QUANTITY_OF_PACKETS];

#if USE_CRC
static u16_union_t _crc;
#endif


#define kDataLength (MAX_STRING_SIZE + 8) * 2
static uint32_t _dataIndex;
static uint16_t receivedData;
static uint8_t _dataIn[kDataLength];
static uint8_t _dataOut[QUANTITY_OF_DATABUFFER_FOR_RETRY][kDataLength];
#if USE_ACK
static uint32_t _dataOutElapsedTime[QUANTITY_OF_DATABUFFER_FOR_RETRY];
static uint8_t _dataOutRetries[QUANTITY_OF_DATABUFFER_FOR_RETRY] = { 0, 0, 0, 0, 0 };
static uint8_t _dataOutLengths[QUANTITY_OF_DATABUFFER_FOR_RETRY] = { 0, 0, 0, 0, 0 };
static uint8_t _dataOutIndex = 1;
#else
static uint8_t _dataOutIndex = 0;
#endif
static uint32_t _command;
static u16_union_t _address;
static PayloadIndex_t _payloadIndex;

lumen_packet_t *readingPacket;
static bool reading = false;

#if USE_CRC
void calculate_crc(uint16_t data) {
  static uint32_t pos;
  static uint32_t i;
  static uint32_t z = 0;
  static uint32_t u = 0x0001;
  static uint32_t o = 8;
  static uint32_t p = 0xA001;

  _crc.value ^= data;
  ++data;

  for (i = o; z != i; --i) {
    if (z != (u & _crc.value)) {
      _crc.value >>= u;
      _crc.value ^= p;
    } else {
      _crc.value >>= u;
    }
  }
}
#endif

static uint8_t writeTempData;

#if USE_ACK
uint32_t elapsed_time_in_ms = 0;
void lumen_ack_trigger(uint32_t time_in_ms) {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return;
#endif

  for (uint8_t dataOutIndex = 1; dataOutIndex < QUANTITY_OF_DATABUFFER_FOR_RETRY; ++dataOutIndex) {
    if (_dataOutRetries[dataOutIndex] > 0) {
      _dataOutElapsedTime[dataOutIndex] += time_in_ms;
      if (_dataOutElapsedTime[dataOutIndex] >= ELAPSED_TIME_TO_RETRY) {
        lumen_write_bytes(_dataOut[dataOutIndex], _dataOutLengths[dataOutIndex]);
        --_dataOutRetries[dataOutIndex];
        _dataOutElapsedTime[dataOutIndex] = 0;
      }
    }
  }
}
#endif

uint32_t lumen_write(uint16_t address, uint8_t *data, uint32_t length) {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return 0;
#endif

  static uint32_t outDataIndex;
  outDataIndex = 0;

  _dataOut[_dataOutIndex][outDataIndex] = START_FLAG;
  ++outDataIndex;

  _dataOut[_dataOutIndex][outDataIndex] = WRITE_FLAG;
#if USE_CRC
  _crc.value = 0xFFFF;
  calculate_crc(_dataOut[_dataOutIndex][outDataIndex]);
#endif
  ++outDataIndex;

  writeTempData = address & 0xFF;
#if USE_CRC
  calculate_crc(writeTempData);
#endif
  if (writeTempData == START_FLAG || writeTempData == END_FLAG || writeTempData == ESCAPE_FLAG) {
    _dataOut[_dataOutIndex][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[_dataOutIndex][outDataIndex] = writeTempData ^ XOR_FLAG;
  } else {
    _dataOut[_dataOutIndex][outDataIndex] = writeTempData;
  }
  ++outDataIndex;

  writeTempData = address >> 8;
#if USE_CRC
  calculate_crc(writeTempData);
#endif
  if (writeTempData == START_FLAG || writeTempData == END_FLAG || writeTempData == ESCAPE_FLAG) {
    _dataOut[_dataOutIndex][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[_dataOutIndex][outDataIndex] = writeTempData ^ XOR_FLAG;
  } else {
    _dataOut[_dataOutIndex][outDataIndex] = writeTempData;
  }
  ++outDataIndex;

#if USE_CRC
  for (uint16_t i = 0; i < length; i++) {
    calculate_crc(data[i]);
  }
#endif

  for (uint16_t i = 0; i < length; i++) {
    if (data[i] == START_FLAG || data[i] == END_FLAG || data[i] == ESCAPE_FLAG) {
      _dataOut[_dataOutIndex][outDataIndex] = ESCAPE_FLAG;
      ++outDataIndex;
      _dataOut[_dataOutIndex][outDataIndex] = data[i] ^ XOR_FLAG;
      ++outDataIndex;
    } else {
      _dataOut[_dataOutIndex][outDataIndex] = data[i];
      ++outDataIndex;
    }
  }

#if USE_ACK
  _dataOut[_dataOutIndex][outDataIndex] = _dataOutIndex;
#if USE_CRC
  calculate_crc(_dataOut[_dataOutIndex][outDataIndex]);
#endif
  ++outDataIndex;
  _dataOut[_dataOutIndex][outDataIndex] = 0;
#if USE_CRC
  calculate_crc(_dataOut[_dataOutIndex][outDataIndex]);
#endif
  ++outDataIndex;
#endif

#if USE_CRC
  if ((_crc.byte.high == (uint8_t)START_FLAG) || (_crc.byte.high == (uint8_t)END_FLAG)
      || (_crc.byte.high == (uint8_t)ESCAPE_FLAG)) {
    _dataOut[_dataOutIndex][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[_dataOutIndex][outDataIndex] = _crc.byte.high ^ XOR_FLAG;
    ++outDataIndex;
  } else {
    _dataOut[_dataOutIndex][outDataIndex] = _crc.byte.high;
    ++outDataIndex;
  }

  if ((_crc.byte.low == (uint8_t)START_FLAG) || (_crc.byte.low == (uint8_t)END_FLAG)
      || (_crc.byte.low == (uint8_t)ESCAPE_FLAG)) {
    _dataOut[_dataOutIndex][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[_dataOutIndex][outDataIndex] = _crc.byte.low ^ XOR_FLAG;
    ++outDataIndex;
  } else {
    _dataOut[_dataOutIndex][outDataIndex] = _crc.byte.low;
    ++outDataIndex;
  }
#endif

  _dataOut[_dataOutIndex][outDataIndex] = END_FLAG;
  ++outDataIndex;

  lumen_write_bytes(_dataOut[_dataOutIndex], outDataIndex);

#if USE_ACK
  _dataOutLengths[_dataOutIndex] = outDataIndex;
  _dataOutElapsedTime[_dataOutIndex] = 0;
  _dataOutRetries[_dataOutIndex] = QUANTITY_OF_RETRIES;
  for (_dataOutIndex = 1; _dataOutIndex < QUANTITY_OF_DATABUFFER_FOR_RETRY; ++_dataOutIndex) {
    if (_dataOutRetries[_dataOutIndex] == 0) {
      break;
    }
  }
  if (_dataOutIndex >= QUANTITY_OF_DATABUFFER_FOR_RETRY) {
    _dataOutIndex = QUANTITY_OF_DATABUFFER_FOR_RETRY - 1;
  }
#endif

  return outDataIndex;
}

uint32_t lumen_write_packet(lumen_packet_t *packet) {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return 0;
#endif

  switch (packet->type) {
    case kBool:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 1);
      }
      break;
    case kString:
      {
        uint8_t length = 0;
        for (; length < MAX_STRING_SIZE; ++length) {
          if (packet->data._string[length] == '\0') {
            break;
          }
        }

        lumen_write(packet->address, (uint8_t *)packet->data._string, length + 1);
      }
      break;
    case kChar:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 1);
      }
      break;
    case kU8:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 1);
      }
      break;
    case kS8:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 1);
      }
      break;
    case kU16:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 2);
      }
      break;
    case kS16:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 2);
      }
      break;
    case kU32:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 4);
      }
      break;
    case kS32:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 4);
      }
      break;
    case kFloat:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 4);
      }
      break;
    case kDouble:
      {
        lumen_write(packet->address, (uint8_t *)packet->data._string, 8);
      }
      break;
    default:
      {
      }
      break;
  }
  return 1;
}

void ParsePayload() {
  switch (_payloadIndex) {
    case kCommand:
      {
        _dataIn[_dataIndex] = receivedData;
        ++_dataIndex;
        _command = receivedData;
        _payloadIndex = kAddressLow;
      }
      break;
    case kAddressLow:
      {
        _dataIn[_dataIndex] = receivedData;
        ++_dataIndex;
        _address.byte.low = receivedData;
        _payloadIndex = kAddressHigh;
      }
      break;
    case kAddressHigh:
      {
        _dataIn[_dataIndex] = receivedData;
        ++_dataIndex;
        _address.byte.high = receivedData;
        _payloadIndex = kData;
      }
      break;
    case kData:
      {
        if (_dataIndex < kDataLength) {
          _dataIn[_dataIndex] = receivedData;
          ++_dataIndex;
        }
      }
      break;
    default:
      break;
  }
}

void Pack() {
  if (_command == READ_FLAG) {
    if (reading == true) {
      if (_address.value == readingPacket->address) {

        uint8_t dataSize = _dataIndex - kData;

        for (uint8_t i = 0; i < dataSize; ++i) {
          readingPacket->data._string[i] = _dataIn[i + kData];
        }
        reading = false;
        return;
      }
    }

    for (uint8_t packetIndex = 0; packetIndex < QUANTITY_OF_PACKETS; ++packetIndex) {
      if (occupiedSlots[packetIndex] == true) {
        if (packets[packetIndex].address == _address.value) {
          uint8_t dataSize = _dataIndex - kData;

          for (uint8_t i = 0; i < dataSize; ++i) {
            packets[packetIndex].data._string[i] = _dataIn[i + kData];
          }
          break;
        }
      }
    }

    for (uint8_t packetIndex = 0; packetIndex < QUANTITY_OF_PACKETS; ++packetIndex) {
      if (occupiedSlots[packetIndex] == false) {

        packets[packetIndex].address = _address.value;

        uint8_t dataSize = _dataIndex - kData;

        for (uint8_t i = 0; i < dataSize; ++i) {
          packets[packetIndex].data._string[i] = _dataIn[i + kData];
        }
        occupiedSlots[packetIndex] = true;

        ++quantityOfPacketsAvailable;
        break;
      }
    }
  }
#if USE_ACK
  else if (_command == ACK_FLAG) {
    _dataOutRetries[_address.byte.low] = 0;
  }
#endif
}

uint32_t lumen_available() {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return 0;
#endif

  static bool _started;
  static bool _escaped;
#if USE_CRC
  static bool _crcStarted;
  static uint16_t _crcIndex;
  static uint16_t _crcIndexDelayed;
  static uint16_t _crcData[3];
#endif

  receivedData = lumen_get_byte();

  while (receivedData != 0xFFFF) {
    if (receivedData == START_FLAG) {

#if USE_CRC
      _crc.value = 0xFFFF;
      _crcIndex = 0;
      _crcStarted = false;
      _crcIndexDelayed = 0;
#endif
      _started = true;
      _escaped = false;
      _dataIndex = 0;
      _payloadIndex = kCommand;

    } else if (receivedData == END_FLAG) {

#if USE_CRC
      _crc.value = 0xFFFF;
      uint16_t _dataIndexOffseted = _dataIndex - 2;

      for (uint16_t pos = 0; pos < _dataIndexOffseted; ++pos) {
        calculate_crc(_dataIn[pos]);
      }

      if ((_dataIn[_dataIndex - 2] == (_crc.byte.high)) && (_dataIn[_dataIndex - 1] == (_crc.byte.low))) {
        Pack();
      }
      _crcStarted = false;
#else
      Pack();

#endif
      _started = false;
      _payloadIndex = kPayloadNull;
    } else if (_started) {
      if (_escaped) {
        receivedData ^= XOR_FLAG;
        _escaped = false;
        ParsePayload();
      } else if (receivedData == ESCAPE_FLAG) {
        _escaped = true;
      } else {
        ParsePayload();
      }
    }
    receivedData = lumen_get_byte();
  }
  return quantityOfPacketsAvailable;
}

lumen_packet_t *lumen_get_first_packet() {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return NULL;
#endif

  for (uint8_t i = 0; i < QUANTITY_OF_PACKETS; ++i) {
    if (occupiedSlots[i]) {
      occupiedSlots[i] = false;
      --quantityOfPacketsAvailable;
      return &packets[i];
    }
  }
  return NULL;
}

bool lumen_request(lumen_packet_t *packet) {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return false;
#endif

  static uint32_t outDataIndex;
  readingPacket = packet;
  outDataIndex = 0;

  _dataOut[0][outDataIndex] = START_FLAG;
  ++outDataIndex;

  _dataOut[0][outDataIndex] = READ_FLAG;
#if USE_CRC
  _crc.value = 0xFFFF;
  calculate_crc(_dataOut[0][outDataIndex]);
#endif
  ++outDataIndex;

  writeTempData = readingPacket->address & 0xFF;
#if USE_CRC
  calculate_crc(writeTempData);
#endif
  if (writeTempData == START_FLAG || writeTempData == END_FLAG || writeTempData == ESCAPE_FLAG) {
    _dataOut[0][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[0][outDataIndex] = writeTempData ^ XOR_FLAG;
  } else {
    _dataOut[0][outDataIndex] = writeTempData;
  }
  ++outDataIndex;

  writeTempData = readingPacket->address >> 8;
#if USE_CRC
  calculate_crc(writeTempData);
#endif
  if (writeTempData == START_FLAG || writeTempData == END_FLAG || writeTempData == ESCAPE_FLAG) {
    _dataOut[0][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[0][outDataIndex] = writeTempData ^ XOR_FLAG;
  } else {
    _dataOut[0][outDataIndex] = writeTempData;
  }
  ++outDataIndex;

  _dataOut[0][outDataIndex] = 1;
#if USE_CRC
  calculate_crc(_dataOut[0][outDataIndex]);
#endif
  ++outDataIndex;

#if USE_CRC

  if ((_crc.byte.high == (uint8_t)START_FLAG) || (_crc.byte.high == (uint8_t)END_FLAG)
      || (_crc.byte.high == (uint8_t)ESCAPE_FLAG)) {
    _dataOut[0][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[0][outDataIndex] = _crc.byte.high ^ XOR_FLAG;
    ++outDataIndex;
  } else {
    _dataOut[0][outDataIndex] = _crc.byte.high;
    ++outDataIndex;
  }

  if ((_crc.byte.low == (uint8_t)START_FLAG) || (_crc.byte.low == (uint8_t)END_FLAG)
      || (_crc.byte.low == (uint8_t)ESCAPE_FLAG)) {
    _dataOut[0][outDataIndex] = ESCAPE_FLAG;
    ++outDataIndex;
    _dataOut[0][outDataIndex] = _crc.byte.low ^ XOR_FLAG;
    ++outDataIndex;
  } else {
    _dataOut[0][outDataIndex] = _crc.byte.low;
    ++outDataIndex;
  }

#endif

  _dataOut[0][outDataIndex] = END_FLAG;
  ++outDataIndex;

  lumen_write_bytes(_dataOut[0], outDataIndex);

  return true;
}

bool lumen_read(lumen_packet_t *packet) {

#if USE_PROJECT_UPDATE
  if (g_is_updating)
    return false;
#endif

  reading = true;

  if (!lumen_request(packet)) {
    return false;
  }

  uint32_t elapsedTickTimeOut = 0;

  while (reading == true) {
    lumen_available();

    ++elapsedTickTimeOut;

    if (elapsedTickTimeOut >= TICK_TIME_OUT) {
      return false;
    }
  }
  return true;
}

#if USE_PROJECT_UPDATE

#define MESSAGE(x) lumen_write_bytes((uint8_t *)x, (uint32_t)sizeof(x) - 1)

#define kUpdateProject "UPDATE PROJECT A"
#define kCommandFinished "FINISHED A"
#define kCommandNewDataBlock "NEW BLOCK A"
#define kOKMessage "RECEIVED OK A"
#define kNotOKMessage "RECEIVED NOT OK A"

#define kStartInterval 5
#define kSendBlockInterval 1000

#define kProjectUpdateBlockLength 1024
#define kProjectUpdateCrcLength 2
#define kProjectUpdateCrcIndexHighByte kProjectUpdateBlockLength
#define kProjectUpdateCrcIndexLowByte kProjectUpdateBlockLength + 1

typedef enum send_step {
  kSendNewBlockCmd,
  kWaitingForOkMessageOfNewBlockCmd,
  kWaitingForOkMessageOfBlock,
} lumen_project_update_send_step_t;

typedef struct {
  const char *word;
  uint32_t length;
  uint32_t index;
} lumen_project_update_word_packet_t;

lumen_project_update_word_packet_t okMessageWordComparator = { .word = kOKMessage, .length = sizeof(kOKMessage) - 1, .index = 0 };
lumen_project_update_word_packet_t notOkMessageWordComparator = { .word = kNotOKMessage, .length = sizeof(kNotOKMessage) - 1, .index = 0 };

uint32_t elapsedTimeInMs = 0;
bool isStarted = false;

static bool lumen_project_update_word_checker(lumen_project_update_word_packet_t *word_packet, char character) {
  if (word_packet->word[word_packet->index] == character) {
    ++word_packet->index;
    if (word_packet->length == word_packet->index) {
      word_packet->index = 0;
      return true;
    }
  } else {
    word_packet->index = 0;
    if (word_packet->word[word_packet->index] == character) {
      ++word_packet->index;
    }
    return false;
  }
  return false;
}

static u16_union_t lumen_project_update_calculate_crc(uint8_t *data, uint32_t length) {

  static u16_union_t crc;
  static uint32_t pos;
  static uint32_t i;
  static uint32_t z = 0;
  static uint32_t u = 0x0001;
  static uint32_t o = 8;
  static uint32_t p = 0xA001;

  crc.value = 0xFFFF;

  for (pos = z; pos < length; pos++) {
    crc.value ^= (uint16_t)*data;
    ++data;

    for (i = o; z != i; --i) {
      if (z != (u & crc.value)) {
        crc.value >>= u;
        crc.value ^= p;
      } else {
        crc.value >>= u;
      }
    }
  }
  return crc;
}

static bool lumen_project_update_start() {

  if (isStarted) {
    return true;
  }

  static uint32_t startInterval = kStartInterval;
  receivedData = lumen_get_byte();

  while (receivedData != 0xFFFF) {
    if (lumen_project_update_word_checker(&okMessageWordComparator, (char)receivedData)) {
      isStarted = true;
    }
    receivedData = lumen_get_byte();
  }

  if (elapsedTimeInMs >= startInterval) {
    MESSAGE(kUpdateProject);
    startInterval = elapsedTimeInMs + kStartInterval;
  }

  return false;
}

bool lumen_project_update_send_data(uint8_t *data, uint32_t length) {
  g_is_updating = true;

  if (lumen_project_update_start()) {
    static uint32_t dataIndex = 0;
    static uint32_t blockBufferLength = 0;
    static uint8_t blockBuffer[kProjectUpdateBlockLength + kProjectUpdateCrcLength];
    static u16_union_t crc;
    static uint32_t sendBlockInterval = 0;
    static bool sending = false;
    static uint32_t sendingLength = 0;
    static uint32_t sendingLengthOfLastBlock = 0;
    static bool finishedLastSend = true;
    static bool res = false;

    res = false;

    if (finishedLastSend) {
      finishedLastSend = false;
      sendingLength = length;
      dataIndex = 0;
    }

    static lumen_project_update_send_step_t sendStep;
    if (!sending) {
      if (blockBufferLength < kProjectUpdateBlockLength) {
        if ((sendingLength + blockBufferLength) < kProjectUpdateBlockLength) {
          memcpy(&blockBuffer[blockBufferLength], &data[dataIndex], sendingLength);
          blockBufferLength += sendingLength;
          sendingLength = 0;
        } else {
          sendingLengthOfLastBlock = kProjectUpdateBlockLength - blockBufferLength;
          memcpy(&blockBuffer[blockBufferLength], &data[dataIndex], sendingLengthOfLastBlock);
          dataIndex += sendingLengthOfLastBlock;
          blockBufferLength = kProjectUpdateBlockLength;
        }
      }

      if (blockBufferLength >= kProjectUpdateBlockLength) {
        crc.value = lumen_project_update_calculate_crc(blockBuffer, kProjectUpdateBlockLength).value;

        blockBuffer[kProjectUpdateCrcIndexHighByte] = crc.byte.high;
        blockBuffer[kProjectUpdateCrcIndexLowByte] = crc.byte.low;

        sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;

        sendStep = kSendNewBlockCmd;
        sending = true;
        blockBufferLength = 0;
      }
    }
    if (sending) {
      switch (sendStep) {
        case kSendNewBlockCmd:
          {
            MESSAGE(kCommandNewDataBlock);
            sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;
            sendStep = kWaitingForOkMessageOfNewBlockCmd;
          }
          break;
        case kWaitingForOkMessageOfNewBlockCmd:
          {
            receivedData = lumen_get_byte();
            while (receivedData != 0xFFFF) {
              if (lumen_project_update_word_checker(&okMessageWordComparator, (char)receivedData)) {
                lumen_write_bytes(blockBuffer, kProjectUpdateBlockLength + kProjectUpdateCrcLength);
                sendStep = kWaitingForOkMessageOfBlock;
                sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;
                break;
              }
              if (lumen_project_update_word_checker(&notOkMessageWordComparator, (char)receivedData)) {
                sendStep = kSendNewBlockCmd;
                sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;
                break;
              }
              receivedData = lumen_get_byte();
            }
          }
          break;
        case kWaitingForOkMessageOfBlock:
          {
            receivedData = lumen_get_byte();
            while (receivedData != 0xFFFF) {
              if (lumen_project_update_word_checker(&okMessageWordComparator, (char)receivedData)) {
                sending = false;
                sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;
                sendingLength -= sendingLengthOfLastBlock;
                sendingLengthOfLastBlock = 0;
                break;
              }
              if (lumen_project_update_word_checker(&notOkMessageWordComparator, (char)receivedData)) {
                sendStep = kSendNewBlockCmd;
                sendBlockInterval = kSendBlockInterval + elapsedTimeInMs;
                break;
              }
              receivedData = lumen_get_byte();
            }
          }
          break;
        default:
          break;
      }

      if (elapsedTimeInMs >= sendBlockInterval) {
        sendStep = kSendNewBlockCmd;
        sendBlockInterval = elapsedTimeInMs + kSendBlockInterval;
      }
    }

    if ((sendingLength == 0) && (!sending)) {
      res = true;
      finishedLastSend = true;
    }

    return res;
  }
  return false;
}

void lumen_project_update_tick(uint32_t time_in_ms) {
  elapsedTimeInMs += time_in_ms;
}

void lumen_project_update_finish() {
  g_is_updating = false;
  MESSAGE(kCommandFinished);
  isStarted = false;
}

#endif
