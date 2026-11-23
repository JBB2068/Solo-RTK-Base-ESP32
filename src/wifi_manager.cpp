#include "wifi_manager.h"

#include "config.h"

void WifiManager::begin(const Settings& s) {
  s_ = &s;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid_, sizeof(apSsid_), "SoloRTK-%02X%02X", mac[4], mac[5]);

  WiFi.persistent(false);  // creds live in our own NVS namespace
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(false);

  WiFi.onEvent([this](arduino_event_id_t ev, arduino_event_info_t info) {
    switch (ev) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        gotIp_ = gotIp_ + 1;
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        disconnects_ = disconnects_ + 1;
        lastReason_ = info.wifi_sta_disconnected.reason;
        break;
      default:
        break;
    }
  });

  staEnabled_ = s.ssid[0] != 0;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep adds 100+ ms latency to UDP
  if (!staEnabled_ || s.apAlwaysOn) startAp();
  if (staEnabled_) beginSta();
  downSinceMs_ = millis();
}

void WifiManager::beginSta() {
  WiFi.disconnect(false, false);
  WiFi.begin(s_->ssid, s_->pass[0] ? s_->pass : nullptr);
  WiFi.setSleep(false);
  lastBeginMs_ = millis();
  attempts_++;
}

void WifiManager::startAp() {
  if (apActive_) return;
  WiFi.mode(staEnabled_ ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(apSsid_, s_->apPass);
  apActive_ = true;
  Serial.printf("[wifi] setup AP \"%s\" up at %s\n", apSsid_, WiFi.softAPIP().toString().c_str());
}

void WifiManager::stopAp() {
  if (!apActive_) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  apActive_ = false;
  Serial.println("[wifi] setup AP down");
}

void WifiManager::poll(uint32_t now) {
  if (gotIp_ != seenGen_) {
    seenGen_ = gotIp_;
    upSinceMs_ = now;
    Serial.printf("[wifi] connected to %s, IP %s, RSSI %d, ch %d\n", s_->ssid,
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(), (int)WiFi.channel());
  }
  if (!staEnabled_) return;

  const bool up = staConnected();
  const int apClients = apActive_ ? WiFi.softAPgetStationNum() : 0;

  if (up) {
    downSinceMs_ = now;
    if (apActive_ && !s_->apAlwaysOn && apClients == 0 && now - upSinceMs_ > WIFI_AP_LINGER_MS)
      stopAp();
    return;
  }

  // Down. Retry less often while someone is using the setup AP: every STA
  // scan hops channels and makes the AP unusable for a few seconds.
  const uint32_t retry = apClients ? WIFI_RETRY_AP_BUSY_MS : WIFI_RETRY_MS;
  if (now - lastBeginMs_ >= retry) {
    Serial.printf("[wifi] STA down (last reason %u), reconnect attempt %lu\n", lastReason_,
                  (unsigned long)attempts_ + 1);
    beginSta();
  }
  if (!apActive_ && now - downSinceMs_ > WIFI_AP_AFTER_MS) startAp();
  if (now - downSinceMs_ > WIFI_REBOOT_AFTER_MS && apClients == 0) {
    Serial.println("[wifi] STA down too long, rebooting");
    delay(100);
    ESP.restart();
  }
}
