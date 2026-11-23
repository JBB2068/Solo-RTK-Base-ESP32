#pragma once
#include "mavlink_link.h"
#include "rtcm_pipeline.h"
#include "settings.h"
#include "wifi_manager.h"

// Port-80 config + status UI. Single page, polls /api/status every second.
//   GET  /              page
//   GET  /api/status    live JSON
//   GET  /api/config    current settings (passwords omitted)
//   POST /api/config    form-encoded; validates, saves to NVS, reboots
//   POST /api/reboot
//   POST /api/factory   clear NVS, reboot
void webBegin(Settings& settings, WifiManager& wifi, MavlinkLink& link, RtcmPipeline& rtcm);
void webPoll();
