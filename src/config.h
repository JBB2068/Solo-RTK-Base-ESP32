#pragma once
// Compile-time defaults. Everything here except pins can be changed from the
// web UI and is persisted in NVS. Override with -D build flags if you want.

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""          // empty = boot straight into setup AP
#endif
#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS "sololink"
#endif
#ifndef DEFAULT_AP_PASS
#define DEFAULT_AP_PASS "solortk1"    // setup AP "SoloRTK-XXXX", >= 8 chars
#endif

#define DEFAULT_LOCAL_UDP_PORT 14550
#define DEFAULT_FALLBACK_IP    IPAddress(10, 1, 1, 10)
#define DEFAULT_FALLBACK_PORT  14560
#define DEFAULT_MAV_SYSID      253     // see README: avoid 255 (SYSID_MYGCS)
#define DEFAULT_MAV_COMPID     190
#define DEFAULT_GPS_BAUD       115200
#define DEFAULT_LOCK_TIMEOUT_S 10

// Hardware (ESP32-C3). GPIO20/21 are the UART0 pins; with USB-CDC on boot the
// console is on native USB, so Serial1 can be routed here.
#define GPS_RX_PIN 20
#define GPS_TX_PIN 21
#define GPS_RX_BUFFER 2048

#define HOSTNAME "solortk"

// Wi-Fi supervision
#define WIFI_RETRY_MS            8000UL    // STA reconnect attempt interval
#define WIFI_RETRY_AP_BUSY_MS    60000UL   // slower while a phone is on the setup AP
#define WIFI_AP_AFTER_MS         30000UL   // bring setup AP up after this long without STA
#define WIFI_AP_LINGER_MS        120000UL  // drop setup AP this long after STA is back
#define WIFI_REBOOT_AFTER_MS     (10UL * 60UL * 1000UL)  // reboot if STA down this long and AP idle

// Task watchdog
#define WDT_TIMEOUT_S 10
#define RTCM_STALE_MS 5000UL   // UI flags the GPS feed as stale after this
