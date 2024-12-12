#ifndef LUMEN_PROTOCOL_H_
#define LUMEN_PROTOCOL_H_

// Version 1.1

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "LumenProtocolConfiguration.h"

#define DATA_NULL 0xFFFF

  typedef union {
    bool _bool;
    char _char;
    uint8_t _u8;
    int8_t _s8;
    uint16_t _u16;
    int16_t _s16;
    uint32_t _u32;
    int32_t _s32;
    float _float;
    double _double;
    char _string[MAX_STRING_SIZE];
  } lumen_data_t;

  typedef enum data_type {
    kBool,
    kChar,
    kU8,
    kS8,
    kU16,
    kS16,
    kU32,
    kS32,
    kFloat,
    kDouble,
    kString,
  } lumen_data_type_t;

  typedef struct {
    uint16_t address;
    lumen_data_type_t type;
    lumen_data_t data;
  } lumen_packet_t;

  uint32_t lumen_write(uint16_t address, uint8_t *data, uint32_t length);
  uint32_t lumen_write_packet(lumen_packet_t *packet);
  uint32_t lumen_available();
  bool lumen_read(lumen_packet_t *packet);
  bool lumen_request(lumen_packet_t *packet);
  lumen_packet_t *lumen_get_first_packet();
#if USE_ACK
  void lumen_ack_trigger(uint32_t time_in_ms);
#endif

#if USE_PROJECT_UPDATE
  bool lumen_project_update_send_data(uint8_t *data, uint32_t length);
  void lumen_project_update_tick(uint32_t time_in_ms);
  void lumen_project_update_finish();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* LUMEN_PROTOCOL_H_ */