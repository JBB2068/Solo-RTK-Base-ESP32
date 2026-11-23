#include "crc24q.h"

namespace {
struct Table {
  uint32_t t[256];
  Table() {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i << 16;
      for (int j = 0; j < 8; j++) {
        c <<= 1;
        if (c & 0x1000000) c ^= 0x1864CFB;
      }
      t[i] = c & 0xFFFFFF;
    }
  }
};
const Table kTable;  // built once at static init, 1 KB RAM
}  // namespace

uint32_t crc24q(const uint8_t* data, size_t len, uint32_t crc) {
  for (size_t i = 0; i < len; i++) {
    crc = ((crc << 8) & 0xFFFFFF) ^ kTable.t[((crc >> 16) ^ data[i]) & 0xFF];
  }
  return crc & 0xFFFFFF;
}
