#include "settings.h"

#include <Preferences.h>

#include "config.h"

static const char* kNs = "rtkbase";

static void copyStr(char* dst, size_t cap, const String& v) {
  strncpy(dst, v.c_str(), cap - 1);
  dst[cap - 1] = 0;
}

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof(s));
  copyStr(s.ssid, sizeof(s.ssid), DEFAULT_WIFI_SSID);
  copyStr(s.pass, sizeof(s.pass), DEFAULT_WIFI_PASS);
  copyStr(s.apPass, sizeof(s.apPass), DEFAULT_AP_PASS);
  s.fallbackIp = uint32_t(DEFAULT_FALLBACK_IP);
  s.fallbackPort = DEFAULT_FALLBACK_PORT;
  s.localPort = DEFAULT_LOCAL_UDP_PORT;
  s.sysid = DEFAULT_MAV_SYSID;
  s.compid = DEFAULT_MAV_COMPID;
  s.gpsBaud = DEFAULT_GPS_BAUD;
  s.lockTimeoutS = DEFAULT_LOCK_TIMEOUT_S;
  s.apAlwaysOn = false;
}

void settingsLoad(Settings& s) {
  settingsDefaults(s);
  Preferences p;
  if (!p.begin(kNs, true)) return;  // namespace not created yet
  copyStr(s.ssid, sizeof(s.ssid), p.getString("ssid", s.ssid));
  copyStr(s.pass, sizeof(s.pass), p.getString("pass", s.pass));
  copyStr(s.apPass, sizeof(s.apPass), p.getString("appass", s.apPass));
  s.fallbackIp = p.getUInt("fbip", s.fallbackIp);
  s.fallbackPort = p.getUShort("fbport", s.fallbackPort);
  s.localPort = p.getUShort("lport", s.localPort);
  s.sysid = p.getUChar("sysid", s.sysid);
  s.compid = p.getUChar("compid", s.compid);
  s.gpsBaud = p.getUInt("baud", s.gpsBaud);
  s.lockTimeoutS = p.getUShort("lockto", s.lockTimeoutS);
  s.apAlwaysOn = p.getBool("apalways", s.apAlwaysOn);
  p.end();
}

bool settingsSave(const Settings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  p.putString("ssid", s.ssid);
  p.putString("pass", s.pass);
  p.putString("appass", s.apPass);
  p.putUInt("fbip", s.fallbackIp);
  p.putUShort("fbport", s.fallbackPort);
  p.putUShort("lport", s.localPort);
  p.putUChar("sysid", s.sysid);
  p.putUChar("compid", s.compid);
  p.putUInt("baud", s.gpsBaud);
  p.putUShort("lockto", s.lockTimeoutS);
  p.putBool("apalways", s.apAlwaysOn);
  p.end();
  return true;
}

void settingsFactoryReset() {
  Preferences p;
  if (p.begin(kNs, false)) {
    p.clear();
    p.end();
  }
}

const char* settingsValidate(const Settings& s) {
  const size_t pl = strlen(s.pass), al = strlen(s.apPass);
  if (strlen(s.ssid) > 32) return "SSID too long";
  if (pl != 0 && (pl < 8 || pl > 63)) return "Wi-Fi password must be 8-63 chars (or empty for open)";
  if (al < 8 || al > 63) return "Setup AP password must be 8-63 chars";
  if (s.fallbackPort == 0 || s.localPort == 0) return "Ports must be 1-65535";
  if (s.sysid == 0) return "System ID must be 1-255";
  switch (s.gpsBaud) {
    case 9600: case 19200: case 38400: case 57600: case 115200: case 230400: case 460800: case 921600:
      break;
    default:
      return "Unsupported GPS baud";
  }
  if (s.lockTimeoutS < 2 || s.lockTimeoutS > 600) return "Lock timeout must be 2-600 s";
  return nullptr;
}
