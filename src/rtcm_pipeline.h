#pragma once
#include <Arduino.h>

#include "rtcm3_framer.h"
#include "rtcm_decode.h"
#include "rtcm_fragmenter.h"

class MavlinkLink;

// UART -> RTCM3 framer -> GPS_RTCM_DATA fragmenter -> MavlinkLink.
// Runs in its own FreeRTOS task (higher priority than loop) so a slow HTTP
// client or Wi-Fi reconnect in loop() can never stall UART draining.
class RtcmPipeline {
 public:
  static constexpr int kTypeSlots = 12;

  struct TypeStat {
    uint16_t type;
    uint32_t count;
    uint32_t lastMs;
    uint16_t lastLen;
  };

  struct Snapshot {
    Rtcm3Framer::Stats framer;
    uint32_t uartBytes;
    uint32_t oversize;       // > 720 B, cannot go over MAVLink
    uint32_t sendFailures;   // frames where any fragment failed to send
    uint32_t lastFrameMs;
    uint32_t bytesPerSec;
    TypeStat types[kTypeSlots];
    int typeCount;
    bool haveBase;
    RtcmBaseStation base;
    uint32_t baseMs;
  };

  void begin(HardwareSerial& uart, uint32_t baud, MavlinkLink& link);
  void snapshot(Snapshot& out);  // safe to call from any task

 private:
  static void taskEntry(void* arg);
  void run();
  static void onFrame(const uint8_t* frame, uint16_t len, void* ctx);
  static bool emit(uint8_t flags, const uint8_t* data, uint8_t len, void* ctx);
  void handleFrame(const uint8_t* frame, uint16_t len);

  HardwareSerial* uart_ = nullptr;
  MavlinkLink* link_ = nullptr;
  Rtcm3Framer framer_{&RtcmPipeline::onFrame, this};
  RtcmFragmenter fragmenter_;
  bool emitFailed_ = false;

  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot s_{};
  uint32_t rateWindowStart_ = 0, rateWindowBytes_ = 0;
};
