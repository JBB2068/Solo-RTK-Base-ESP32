#include "mavlink_link.h"

#include <WiFi.h>
#include <ardupilotmega/mavlink.h>  // superset of common; avoids BAD_CRC on APM-only msgs

namespace {
constexpr mavlink_channel_t kRxChan = MAVLINK_COMM_0;
constexpr mavlink_channel_t kTxChan = MAVLINK_COMM_1;

struct Lock {
  explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m_); }
  SemaphoreHandle_t m_;
};
}  // namespace

void MavlinkLink::begin(uint16_t localPort, IPAddress fallbackIp, uint16_t fallbackPort,
                        uint8_t sysid, uint8_t compid, uint32_t lockTimeoutMs) {
  mtx_ = xSemaphoreCreateMutex();
  localPort_ = localPort;
  fallbackIp_ = fallbackIp;
  fallbackPort_ = fallbackPort;
  sysid_ = sysid;
  compid_ = compid;
  lockTimeoutMs_ = lockTimeoutMs;
  target_ = fallbackIp_;
  targetPort_ = fallbackPort_;
  st_.vehicleFixType = -1;
}

void MavlinkLink::rebind() {
  Lock l(mtx_);
  udp_.stop();
  bound_ = udp_.begin(localPort_) == 1;
  Serial.printf("[mav] UDP %u %s\n", localPort_, bound_ ? "bound" : "BIND FAILED");
}

bool MavlinkLink::sendRtcm(uint8_t flags, const uint8_t* data, uint8_t len) {
  // pack() memcpy's all 180 bytes of `data`, so never pass the frame pointer
  // directly: pad into a zeroed local buffer (also lets MAVLink2 trim zeros).
  uint8_t padded[MAVLINK_MSG_GPS_RTCM_DATA_FIELD_DATA_LEN] = {0};
  if (len) memcpy(padded, data, len);

  mavlink_message_t msg;
  uint8_t tx[MAVLINK_MAX_PACKET_LEN];
  Lock l(mtx_);
  if (!bound_ || WiFi.status() != WL_CONNECTED) {
    st_.txSkippedNoWifi++;
    return false;
  }
  mavlink_msg_gps_rtcm_data_pack_chan(sysid_, compid_, kTxChan, &msg, flags, len, padded);
  const uint16_t n = mavlink_msg_to_send_buffer(tx, &msg);
  bool ok = udp_.beginPacket(target_, targetPort_) == 1;
  ok = ok && udp_.write(tx, n) == n;
  ok = ok && udp_.endPacket() == 1;
  if (ok) st_.txPackets++;
  else st_.txErrors++;
  return ok;
}

void MavlinkLink::poll(uint32_t now) {
  uint8_t buf[512];
  for (int budget = 16; budget > 0; budget--) {  // bounded so a flood can't starve loop()
    IPAddress src;
    uint16_t srcPort = 0;
    int remaining;
    {
      Lock l(mtx_);
      if (!bound_) break;
      remaining = udp_.parsePacket();
      if (remaining <= 0) break;
      src = udp_.remoteIP();
      srcPort = udp_.remotePort();
      st_.rxDatagrams++;
    }
    while (remaining > 0) {
      int n;
      {
        Lock l(mtx_);
        n = udp_.read(buf, sizeof(buf));
      }
      if (n <= 0) break;
      remaining -= n;
      for (int i = 0; i < n; i++) {
        mavlink_message_t msg;
        mavlink_status_t status;
        const uint8_t r = mavlink_parse_char(kRxChan, buf[i], &msg, &status);
        if (r == MAVLINK_FRAMING_OK) handleMessage(&msg, src, srcPort, now);
        else if (r == MAVLINK_FRAMING_BAD_CRC) {
          Lock l(mtx_);
          st_.rxParseErrors++;
        }
      }
    }
  }

  Lock l(mtx_);
  if (locked_ && now - lastRxMs_ > lockTimeoutMs_) {
    Serial.printf("[mav] no telemetry from %s:%u for %lus, reverting to fallback %s:%u\n",
                  target_.toString().c_str(), targetPort_, (unsigned long)(lockTimeoutMs_ / 1000),
                  fallbackIp_.toString().c_str(), fallbackPort_);
    locked_ = false;
    target_ = fallbackIp_;
    targetPort_ = fallbackPort_;
    st_.lockChanges++;
  }
}

void MavlinkLink::handleMessage(const void* m, IPAddress src, uint16_t srcPort, uint32_t now) {
  const mavlink_message_t& msg = *static_cast<const mavlink_message_t*>(m);
  Lock l(mtx_);
  st_.rxMsgs++;
  const bool fromTarget = locked_ && src == target_ && srcPort == targetPort_;

  if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT && msg.compid == MAV_COMP_ID_AUTOPILOT1) {
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&msg, &hb);
    const bool isVehicle = hb.autopilot != MAV_AUTOPILOT_INVALID && hb.type != MAV_TYPE_GCS;
    const bool stale = !locked_ || now - lastRxMs_ > lockTimeoutMs_ / 2;
    if (isVehicle && !fromTarget && stale) {
      target_ = src;
      targetPort_ = srcPort;
      locked_ = true;
      lastRxMs_ = now;
      st_.lockChanges++;
      st_.vehicleSysid = msg.sysid;
      Serial.printf("[mav] locked to vehicle sysid %u via %s:%u\n", msg.sysid,
                    src.toString().c_str(), srcPort);
    }
  }

  if (locked_ && src == target_ && srcPort == targetPort_) {
    lastRxMs_ = now;
    everRx_ = true;
    if (msg.msgid == MAVLINK_MSG_ID_GPS_RAW_INT && msg.sysid == st_.vehicleSysid) {
      mavlink_gps_raw_int_t g;
      mavlink_msg_gps_raw_int_decode(&msg, &g);
      st_.vehicleFixType = int8_t(g.fix_type);
      st_.vehicleSats = g.satellites_visible;
      st_.vehicleHdop = g.eph;
    }
  }
}

MavlinkLink::Status MavlinkLink::status() {
  Lock l(mtx_);
  Status s = st_;
  s.locked = locked_;
  s.target = target_;
  s.targetPort = targetPort_;
  s.lastRxAgoMs = everRx_ ? millis() - lastRxMs_ : UINT32_MAX;
  return s;
}
