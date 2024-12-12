#include "LumenProtocol.h"

extern "C" void lumen_write_bytes(uint8_t *data, uint32_t length) {
  Serial.write(data, length);
}

extern "C" uint16_t lumen_get_byte() {
  if (Serial.available()) {
    return Serial.read();
  }
  return DATA_NULL;
}

const uint16_t appendValueAddress = 1000;
const uint16_t selectedPointAddress = 2000;
const uint16_t selectedPointValueAddress = 2001;
const uint16_t autoAppendValueAddress = 3000;
const uint16_t autoIndividualManipulationAddress = 3001;
const uint16_t maxSelectedPointAddress = 4000;
const uint16_t delayAddress = 5000;

lumen_packet_t appendValuePacket = { appendValueAddress, kS32 };
lumen_packet_t selectedPointPacket = { selectedPointAddress, kS32 };
lumen_packet_t selectedPointValuePacket = { selectedPointValueAddress, kS32 };
lumen_packet_t autoAppendValuePacket = { autoAppendValueAddress, kBool };
lumen_packet_t autoIndividualManipulationPacket = { autoIndividualManipulationAddress, kBool };
lumen_packet_t maxSelectedPointPacket = { maxSelectedPointAddress, kS32 };
lumen_packet_t selectedDelayPacket = { delayAddress, kS32 };

lumen_packet_t *currentPackage;

bool isAutoAppendValue = false;
bool isAutoIndividualManipulation = false;

float rad = 0;

uint16_t selectedPoint = 0;
int8_t selectedPointValue[800];
uint32_t maxSelectedPoint = 20;
uint16_t selectedDelay = 0;

lumen_packet_t text = { 124, kString };

void setup() {
  delay(1000);
  Serial.begin(115200);

  delay(2000);

  if (lumen_read(&autoAppendValuePacket))
  {
    isAutoAppendValue = autoAppendValuePacket.data._bool;
    Serial.print("\r\n autoAppendValue -- ");
    Serial.print(isAutoAppendValue);
  }
  if (lumen_read(&autoIndividualManipulationPacket))
  {
    isAutoIndividualManipulation = autoIndividualManipulationPacket.data._bool;
    Serial.print("\r\n autoIndividualManipulation -- ");
    Serial.print(isAutoIndividualManipulation);
  }
  if (lumen_read(&maxSelectedPointPacket))
  {
    maxSelectedPoint = maxSelectedPointPacket.data._s32;
    Serial.print("\r\n maxSelectedPoint -- ");
    Serial.print(maxSelectedPoint);
  }
  if (lumen_read(&selectedDelayPacket))
  {
    selectedDelay = selectedDelayPacket.data._s32;
    Serial.print("\r\n selectedDelay -- ");
    Serial.print(selectedDelay);
  }
}

void loop() {

  while (lumen_available() > 0) {
    currentPackage = lumen_get_first_packet();

    if (currentPackage != NULL) {
      Serial.print("\r\n\r\n Address: ");
      Serial.print(currentPackage->address);
      Serial.print("\r\n Data: ");
      Serial.print(currentPackage->data._u8);

      switch (currentPackage->address) {
        case autoAppendValueAddress:
          isAutoAppendValue = currentPackage->data._bool;
          break;
        case autoIndividualManipulationAddress:
          isAutoIndividualManipulation = currentPackage->data._bool;
          break;
        case maxSelectedPointAddress:
          maxSelectedPoint = currentPackage->data._s32;
          break;
        case delayAddress:
          selectedDelay = currentPackage->data._s32;
          break;
      }
    }
  }

  if (isAutoAppendValue == true) {
    appendValuePacket.data._s32 = (sin(rad += 0.2) + 1) * 50;
    lumen_write_packet(&appendValuePacket);
  }

  if (isAutoIndividualManipulation == true) {
    for (uint16_t i = 0; i < maxSelectedPoint; ++i) {
      selectedPoint = i;
      selectedPointValue[i] += random(-5, 6);

      if (selectedPointValue[i] < 0)
      {
        selectedPointValue[i] = 0;
      }
      
      if (selectedPointValue[i] > 100)
      {
        selectedPointValue[i] = 100;
      }
      
      selectedPointPacket.data._u32 = selectedPoint;
      selectedPointValuePacket.data._u32 = selectedPointValue[i];

      lumen_write_packet(&selectedPointPacket);
      lumen_write_packet(&selectedPointValuePacket);
    }
    delay(100);
  }

  for(uint16_t i = 0; i < selectedDelay; ++i)
  {
    delay(1);
  }
}
