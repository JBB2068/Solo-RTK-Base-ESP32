// Solo RTK base: ESP32-C3 reads RTCM3 from a base GPS on Serial1 and injects
// it into a 3DR Solo (ArduPilot) as MAVLink2 GPS_RTCM_DATA over SoloLink UDP.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "mavlink_link.h"
#include "rtcm_pipeline.h"
#include "settings.h"
#include "web_ui.h"
#include "wifi_manager.h"

static Settings settings;
static WifiManager wifi;
static MavlinkLink mavLink;
static RtcmPipeline rtcm;
static uint32_t boundGen = 0;
static uint32_t lastLogMs = 0;

static void watchdogInit() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WDT_TIMEOUT_S * 1000;
  cfg.idle_core_mask = (1 << portNUM_PROCESSORS) - 1;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);  // reconfigures if already running
#endif
  esp_task_wdt_add(nullptr);  // loopTask
}

static void logStatus() {
  RtcmPipeline::Snapshot r;
  rtcm.snapshot(r);
  const MavlinkLink::Status m = mavLink.status();
  Serial.printf("[stat] wifi=%s rtcm frames=%lu crc=%lu %luB/s | mav %s %s:%u tx=%lu err=%lu | fix=%d\n",
                wifi.staConnected() ? "up" : "down", (unsigned long)r.framer.frames,
                (unsigned long)r.framer.crcErrors, (unsigned long)r.bytesPerSec,
                m.locked ? "locked" : "fallback", m.target.toString().c_str(), m.targetPort,
                (unsigned long)m.txPackets, (unsigned long)m.txErrors, m.vehicleFixType);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[boot] Solo RTK base %s, reset reason %d\n", FW_VERSION_STR, (int)esp_reset_reason());

  watchdogInit();
  settingsLoad(settings);

  mavLink.begin(settings.localPort, IPAddress(settings.fallbackIp), settings.fallbackPort,
             settings.sysid, settings.compid, uint32_t(settings.lockTimeoutS) * 1000);
  rtcm.begin(Serial1, settings.gpsBaud, mavLink);  // starts RTCM task (prio 3)
  wifi.begin(settings);                         // non-blocking
  webBegin(settings, wifi, mavLink, rtcm);
}

void loop() {
  const uint32_t now = millis();
  esp_task_wdt_reset();

  wifi.poll(now);
  if (wifi.connectGeneration() != boundGen && wifi.staConnected()) {
    boundGen = wifi.connectGeneration();
    mavLink.rebind();  // fresh socket on every (re)connect
    MDNS.end();
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  }
  mavLink.poll(now);
  webPoll();

  if (now - lastLogMs >= 10000) {
    lastLogMs = now;
    logStatus();
  }
  delay(1);  // let IDLE run so the idle-task watchdog stays fed
}
