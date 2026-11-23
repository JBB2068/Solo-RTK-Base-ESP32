#pragma once
#include <Arduino.h>
#include <WiFi.h>

#include "settings.h"

// STA supervisor for SoloLink plus a setup soft-AP.
//  - Owns reconnects (core auto-reconnect is disabled so retry timing is ours).
//  - Brings up "SoloRTK-XXXX" (AP+STA) if no SSID is configured, STA has been
//    down for WIFI_AP_AFTER_MS, or apAlwaysOn is set; drops it again once STA
//    has been up for WIFI_AP_LINGER_MS and nobody is connected to it.
//  - Reboots if STA has been down for WIFI_REBOOT_AFTER_MS and the AP is idle.
class WifiManager {
 public:
  void begin(const Settings& s);
  void poll(uint32_t now);

  bool staConnected() const { return WiFi.status() == WL_CONNECTED; }
  // Increments on every GOT_IP; compare to detect a (re)connect.
  uint32_t connectGeneration() const { return gotIp_; }

  bool apActive() const { return apActive_; }
  const char* apSsid() const { return apSsid_; }
  uint32_t reconnectAttempts() const { return attempts_; }
  uint32_t disconnects() const { return disconnects_; }
  uint8_t lastDisconnectReason() const { return lastReason_; }
  uint32_t staUpForMs(uint32_t now) const { return staConnected() ? now - upSinceMs_ : 0; }

 private:
  void startAp();
  void stopAp();
  void beginSta();

  const Settings* s_ = nullptr;
  char apSsid_[24] = {0};
  bool apActive_ = false;
  bool staEnabled_ = false;
  uint32_t lastBeginMs_ = 0;
  uint32_t downSinceMs_ = 0;
  uint32_t upSinceMs_ = 0;
  uint32_t attempts_ = 0;
  uint32_t seenGen_ = 0;
  volatile uint32_t gotIp_ = 0;
  volatile uint32_t disconnects_ = 0;
  volatile uint8_t lastReason_ = 0;
};
