/**
  lib_crc.h
  */

#ifndef LIB_CRC_H_
#define LIB_CRC_H_

#include <stdint.h>

extern const uint16_t crc_tabccitt[256];
#define UPDATE_CRC_CCITT(crc, c)    (crc << 8) ^ crc_tabccitt[(uint8_t)(crc >> 8) ^ (0xFF & (uint16_t)c)];

#endif // LIB_CRC_H_
