#pragma once
#include <stddef.h>
#include <stdint.h>

// Byte-stream RTCM3 framer. Feed it raw UART bytes; it calls `onFrame` with
// every CRC-valid frame (preamble..CRC inclusive).
//
// Unlike a reset-on-error parser, a CRC failure does not throw away the
// buffered bytes: it drops only the false 0xD3 and rescans what it already
// buffered, so a spurious preamble inside payload data cannot swallow the
// real frames that follow it.
class Rtcm3Framer {
 public:
  static constexpr uint16_t kMaxPayload = 1023;
  static constexpr uint16_t kMaxFrame = 3 + kMaxPayload + 3;

  using FrameCb = void (*)(const uint8_t* frame, uint16_t len, void* ctx);

  struct Stats {
    uint32_t frames = 0;
    uint32_t crcErrors = 0;
    uint32_t headerErrors = 0;    // reserved bits set or zero length
    uint32_t bytesDiscarded = 0;  // bytes skipped while hunting for 0xD3
  };

  Rtcm3Framer(FrameCb cb, void* ctx) : cb_(cb), ctx_(ctx) {}

  void feed(uint8_t b);
  void feed(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; i++) feed(data[i]);
  }
  void reset() { len_ = 0; }

  const Stats& stats() const { return stats_; }

  // 12-bit message number from a complete frame.
  static uint16_t messageType(const uint8_t* frame) {
    return (uint16_t(frame[3]) << 4) | (frame[4] >> 4);
  }

 private:
  bool tryParse();          // returns true if it consumed bytes and may make more progress
  void resync(size_t from); // drop buf_[0..), keep from next 0xD3 at/after `from`

  FrameCb cb_;
  void* ctx_;
  uint8_t buf_[kMaxFrame];
  size_t len_ = 0;
  Stats stats_;
};
