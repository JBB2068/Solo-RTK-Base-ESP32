#pragma once
#include <stddef.h>
#include <stdint.h>

// Splits one RTCM3 frame into GPS_RTCM_DATA payloads, matching
// ArduPilot AP_GPS::handle_gps_rtcm_fragment():
//   flags bit0     = fragmented
//   flags bits1-2  = fragment id (0..3)
//   flags bits3-7  = 5-bit sequence id
//   <=180 B        -> one unfragmented message
//   181..720 B     -> ceil(n/180) fragments; a 0-length closer is added for
//                     exactly 360 / 540 B (ArduPilot only knows the count
//                     from a short fragment, or from 4 full ones at 720 B)
//   >720 B         -> rejected
class RtcmFragmenter {
 public:
  static constexpr uint8_t kChunk = 180;
  static constexpr uint16_t kMaxFrame = 4 * kChunk;

  // `data` is NOT padded; the sender must copy into a 180-byte zeroed buffer.
  using EmitCb = bool (*)(uint8_t flags, const uint8_t* data, uint8_t len, void* ctx);

  // Returns messages emitted, or -1 if the frame is too large.
  int fragment(const uint8_t* frame, uint16_t len, EmitCb emit, void* ctx);

  uint8_t sequence() const { return seq_; }

 private:
  uint8_t seq_ = 0;
};
