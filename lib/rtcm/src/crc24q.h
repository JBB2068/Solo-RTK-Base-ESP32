#pragma once
#include <stddef.h>
#include <stdint.h>

// CRC-24Q (Qualcomm), poly 0x1864CFB, init 0, no reflection, no xorout.
uint32_t crc24q(const uint8_t* data, size_t len, uint32_t crc = 0);
