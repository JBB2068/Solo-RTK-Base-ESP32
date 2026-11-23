#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <freertos/semphr.h>

// UDP MAVLink endpoint on SoloLink.
//  - Binds localPort (14550) and parses inbound telemetry.
//  - Locks onto the source IP:port of the first valid autopilot HEARTBEAT
//    (not just any UDP datagram), refreshed by any valid MAVLink from that
//    endpoint; reverts to the fallback after lockTimeout of silence.
//  - Sends GPS_RTCM_DATA from the same socket. sendRtcm() is called from the
//    RTCM task, poll() from loop(); a mutex serialises socket access.
class MavlinkLink {
 public:
  struct Status {
    bool locked;
    IPAddress target;
    uint16_t targetPort;
    uint32_t lastRxAgoMs;      // UINT32_MAX if never
    uint32_t rxDatagrams, rxMsgs, rxParseErrors;
    uint32_t txPackets, txErrors, txSkippedNoWifi;
    uint32_t lockChanges;
    uint8_t vehicleSysid;
    int8_t vehicleFixType;     // GPS_RAW_INT.fix_type, -1 unknown (6 = RTK fixed, 5 = float)
    uint8_t vehicleSats;
    uint16_t vehicleHdop;      // cm
  };

  void begin(uint16_t localPort, IPAddress fallbackIp, uint16_t fallbackPort, uint8_t sysid,
             uint8_t compid, uint32_t lockTimeoutMs);
  void rebind();             // call after every (re)connect
  void poll(uint32_t now);   // loop task only
  bool sendRtcm(uint8_t flags, const uint8_t* data, uint8_t len);  // any task
  Status status();

 private:
  void handleMessage(const void* msg, IPAddress src, uint16_t srcPort, uint32_t now);

  WiFiUDP udp_;
  SemaphoreHandle_t mtx_ = nullptr;
  uint16_t localPort_ = 14550;
  IPAddress fallbackIp_;
  uint16_t fallbackPort_ = 14560;
  uint8_t sysid_ = 253, compid_ = 190;
  uint32_t lockTimeoutMs_ = 10000;

  // guarded by mtx_
  IPAddress target_;
  uint16_t targetPort_ = 0;
  bool locked_ = false;
  bool bound_ = false;
  uint32_t lastRxMs_ = 0;
  bool everRx_ = false;
  Status st_{};
};
