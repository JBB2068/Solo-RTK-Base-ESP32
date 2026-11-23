#pragma once
#include <Arduino.h>
#include <IPAddress.h>

struct Settings {
  char ssid[33];
  char pass[65];
  char apPass[65];
  uint32_t fallbackIp;     // network order as IPAddress(uint32_t)
  uint16_t fallbackPort;
  uint16_t localPort;
  uint8_t sysid;
  uint8_t compid;
  uint32_t gpsBaud;
  uint16_t lockTimeoutS;
  bool apAlwaysOn;
};

void settingsDefaults(Settings& s);
void settingsLoad(Settings& s);
bool settingsSave(const Settings& s);
void settingsFactoryReset();
// Returns nullptr if valid, else a human-readable error.
const char* settingsValidate(const Settings& s);
