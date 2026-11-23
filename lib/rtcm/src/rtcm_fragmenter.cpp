#include "rtcm_fragmenter.h"

int RtcmFragmenter::fragment(const uint8_t* frame, uint16_t len, EmitCb emit, void* ctx) {
  if (len > kMaxFrame) return -1;

  if (len <= kChunk) {
    emit(0, frame, uint8_t(len), ctx);
    return 1;
  }

  const uint8_t seqBits = uint8_t((seq_ & 0x1F) << 3);
  int count = 0;
  uint8_t fragId = 0;
  for (uint16_t off = 0; off < len; off += kChunk, fragId++) {
    const uint8_t n = (len - off > kChunk) ? kChunk : uint8_t(len - off);
    emit(uint8_t(1 | (fragId << 1) | seqBits), frame + off, n, ctx);
    count++;
  }
  if (len % kChunk == 0 && fragId < 4) {
    emit(uint8_t(1 | (fragId << 1) | seqBits), nullptr, 0, ctx);
    count++;
  }
  seq_ = (seq_ + 1) & 0x1F;
  return count;
}
