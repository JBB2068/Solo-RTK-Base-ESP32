#include "rtcm_pipeline.h"

#include <esp_task_wdt.h>

#include "config.h"
#include "mavlink_link.h"

void RtcmPipeline::begin(HardwareSerial& uart, uint32_t baud, MavlinkLink& link) {
  uart_ = &uart;
  link_ = &link;
  uart.setRxBufferSize(GPS_RX_BUFFER);  // must precede begin()
  uart.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  xTaskCreate(&RtcmPipeline::taskEntry, "rtcm", 6144, this, 3, nullptr);
}

void RtcmPipeline::taskEntry(void* arg) { static_cast<RtcmPipeline*>(arg)->run(); }

void RtcmPipeline::run() {
  esp_task_wdt_add(nullptr);
  uint8_t buf[256];
  for (;;) {
    esp_task_wdt_reset();
    int avail = uart_->available();
    if (avail <= 0) {
      vTaskDelay(pdMS_TO_TICKS(5));  // ~58 B at 115200, far below the 2 KB ring
      continue;
    }
    size_t n = uart_->read(buf, avail > int(sizeof(buf)) ? sizeof(buf) : size_t(avail));
    framer_.feed(buf, n);

    const uint32_t now = millis();
    portENTER_CRITICAL(&mux_);
    s_.uartBytes += n;
    s_.framer = framer_.stats();
    rateWindowBytes_ += n;
    if (now - rateWindowStart_ >= 2000) {
      s_.bytesPerSec = rateWindowBytes_ * 1000 / (now - rateWindowStart_);
      rateWindowStart_ = now;
      rateWindowBytes_ = 0;
    }
    portEXIT_CRITICAL(&mux_);
  }
}

void RtcmPipeline::onFrame(const uint8_t* frame, uint16_t len, void* ctx) {
  static_cast<RtcmPipeline*>(ctx)->handleFrame(frame, len);
}

bool RtcmPipeline::emit(uint8_t flags, const uint8_t* data, uint8_t len, void* ctx) {
  auto* self = static_cast<RtcmPipeline*>(ctx);
  if (!self->link_->sendRtcm(flags, data, len)) self->emitFailed_ = true;
  return true;
}

void RtcmPipeline::handleFrame(const uint8_t* frame, uint16_t len) {
  const uint32_t now = millis();
  const uint16_t type = Rtcm3Framer::messageType(frame);

  emitFailed_ = false;
  const int sent = fragmenter_.fragment(frame, len, &RtcmPipeline::emit, this);

  RtcmBaseStation base;
  const bool isBase = rtcmDecodeBaseStation(frame, len, base);

  portENTER_CRITICAL(&mux_);
  s_.lastFrameMs = now;
  if (sent < 0) s_.oversize++;
  if (emitFailed_) s_.sendFailures++;
  int slot = -1;
  for (int i = 0; i < s_.typeCount; i++)
    if (s_.types[i].type == type) slot = i;
  if (slot < 0 && s_.typeCount < kTypeSlots) {
    slot = s_.typeCount++;
    s_.types[slot] = {type, 0, 0, 0};
  }
  if (slot >= 0) {
    s_.types[slot].count++;
    s_.types[slot].lastMs = now;
    s_.types[slot].lastLen = len;
  }
  if (isBase) {
    s_.haveBase = true;
    s_.base = base;
    s_.baseMs = now;
  }
  portEXIT_CRITICAL(&mux_);

  if (sent < 0) Serial.printf("[rtcm] %u: %u B frame > 720 B, dropped (use MSM4)\n", type, len);
}

void RtcmPipeline::snapshot(Snapshot& out) {
  portENTER_CRITICAL(&mux_);
  out = s_;
  portEXIT_CRITICAL(&mux_);
}
