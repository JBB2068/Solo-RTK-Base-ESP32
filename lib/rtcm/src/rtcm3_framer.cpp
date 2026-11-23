#include "rtcm3_framer.h"

#include <string.h>

#include "crc24q.h"

void Rtcm3Framer::feed(uint8_t b) {
  if (len_ == 0 && b != 0xD3) {
    stats_.bytesDiscarded++;
    return;
  }
  buf_[len_++] = b;
  while (len_ > 0 && tryParse()) {
  }
}

void Rtcm3Framer::resync(size_t from) {
  size_t i = from;
  while (i < len_ && buf_[i] != 0xD3) i++;
  stats_.bytesDiscarded += i;
  memmove(buf_, buf_ + i, len_ - i);
  len_ -= i;
}

bool Rtcm3Framer::tryParse() {
  if (len_ < 3) return false;

  const uint16_t payloadLen = (uint16_t(buf_[1] & 0x03) << 8) | buf_[2];
  if ((buf_[1] & 0xFC) != 0 || payloadLen < 2) {
    stats_.headerErrors++;
    resync(1);
    return true;
  }

  const size_t frameLen = size_t(payloadLen) + 6;
  if (len_ < frameLen) return false;

  const uint32_t calc = crc24q(buf_, frameLen - 3);
  const uint32_t recv = (uint32_t(buf_[frameLen - 3]) << 16) |
                        (uint32_t(buf_[frameLen - 2]) << 8) | buf_[frameLen - 1];
  if (calc != recv) {
    stats_.crcErrors++;
    resync(1);  // rescan everything after the bogus preamble
    return true;
  }

  stats_.frames++;
  if (cb_) cb_(buf_, uint16_t(frameLen), ctx_);
  len_ -= frameLen;
  if (len_) memmove(buf_, buf_ + frameLen, len_);
  if (len_ && buf_[0] != 0xD3) resync(0);
  return true;
}
