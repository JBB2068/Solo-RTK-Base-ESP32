#include "web_ui.h"

#include <WebServer.h>
#include <esp_system.h>

#include "config.h"
#include "web_page.h"

namespace {
WebServer server(80);
Settings* g_s;
WifiManager* g_wifi;
MavlinkLink* g_link;
RtcmPipeline* g_rtcm;
uint32_t g_rebootAt = 0;

// Minimal JSON writer into a String.
void jsonStr(String& o, const char* v) {
  o += '"';
  for (const char* p = v; *p; p++) {
    const char c = *p;
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (uint8_t(c) < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
    else o += c;
  }
  o += '"';
}
void kv(String& o, const char* k) { o += '"'; o += k; o += "\":"; }
void kvStr(String& o, const char* k, const char* v) { kv(o, k); jsonStr(o, v); o += ','; }
void kvNum(String& o, const char* k, long long v) { kv(o, k); o += String((long)v); o += ','; }
void kvU(String& o, const char* k, uint32_t v) { kv(o, k); o += String(v); o += ','; }
void kvBool(String& o, const char* k, bool v) { kv(o, k); o += v ? "true," : "false,"; }
void kvF(String& o, const char* k, double v, int dp) { kv(o, k); o += String(v, dp); o += ','; }
void jclose(String& o, char c) {
  if (o.endsWith(",")) o.remove(o.length() - 1);
  o += c;
}

const char* resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_EXT: return "external";
    default: return "other";
  }
}

void handleStatus() {
  const uint32_t now = millis();
  RtcmPipeline::Snapshot r;
  g_rtcm->snapshot(r);
  const MavlinkLink::Status m = g_link->status();

  String o;
  o.reserve(1500);
  o += '{';
  kvStr(o, "version", FW_VERSION_STR);
  kvU(o, "uptime_s", now / 1000);
  kvU(o, "heap", ESP.getFreeHeap());
  kvU(o, "heap_min", ESP.getMinFreeHeap());
  kvStr(o, "reset_reason", resetReasonStr());

  kv(o, "wifi"); o += '{';
  kvBool(o, "connected", g_wifi->staConnected());
  kvStr(o, "ssid", g_s->ssid);
  kvStr(o, "ip", WiFi.localIP().toString().c_str());
  kvNum(o, "rssi", g_wifi->staConnected() ? WiFi.RSSI() : 0);
  kvNum(o, "channel", WiFi.channel());
  kvU(o, "up_s", g_wifi->staUpForMs(now) / 1000);
  kvU(o, "attempts", g_wifi->reconnectAttempts());
  kvU(o, "disconnects", g_wifi->disconnects());
  kvU(o, "last_reason", g_wifi->lastDisconnectReason());
  kvBool(o, "ap", g_wifi->apActive());
  kvStr(o, "ap_ssid", g_wifi->apSsid());
  kvStr(o, "ap_ip", WiFi.softAPIP().toString().c_str());
  kvNum(o, "ap_clients", g_wifi->apActive() ? WiFi.softAPgetStationNum() : 0);
  jclose(o, '}'); o += ',';

  kv(o, "mav"); o += '{';
  kvBool(o, "locked", m.locked);
  kvStr(o, "target", (m.target.toString() + ":" + String(m.targetPort)).c_str());
  kvNum(o, "last_rx_ms", m.lastRxAgoMs == UINT32_MAX ? -1 : (long long)m.lastRxAgoMs);
  kvU(o, "rx_datagrams", m.rxDatagrams);
  kvU(o, "rx_msgs", m.rxMsgs);
  kvU(o, "rx_crc_err", m.rxParseErrors);
  kvU(o, "tx", m.txPackets);
  kvU(o, "tx_err", m.txErrors);
  kvU(o, "tx_no_wifi", m.txSkippedNoWifi);
  kvU(o, "lock_changes", m.lockChanges);
  kvU(o, "veh_sysid", m.vehicleSysid);
  kvNum(o, "veh_fix", m.vehicleFixType);
  kvU(o, "veh_sats", m.vehicleSats);
  kvU(o, "veh_hdop_cm", m.vehicleHdop);
  jclose(o, '}'); o += ',';

  kv(o, "rtcm"); o += '{';
  kvU(o, "uart_bytes", r.uartBytes);
  kvU(o, "bps", r.bytesPerSec);
  kvU(o, "frames", r.framer.frames);
  kvU(o, "crc_err", r.framer.crcErrors);
  kvU(o, "hdr_err", r.framer.headerErrors);
  kvU(o, "discarded", r.framer.bytesDiscarded);
  kvU(o, "oversize", r.oversize);
  kvU(o, "send_fail", r.sendFailures);
  kvNum(o, "last_frame_ms", r.lastFrameMs ? (long long)(now - r.lastFrameMs) : -1);
  kvBool(o, "stale", !r.lastFrameMs || now - r.lastFrameMs > RTCM_STALE_MS);
  kv(o, "types"); o += '[';
  for (int i = 0; i < r.typeCount; i++) {
    o += '{';
    kvU(o, "t", r.types[i].type);
    kvU(o, "n", r.types[i].count);
    kvU(o, "len", r.types[i].lastLen);
    kvU(o, "ago_ms", now - r.types[i].lastMs);
    jclose(o, '}'); o += ',';
  }
  jclose(o, ']'); o += ',';
  jclose(o, '}'); o += ',';

  kv(o, "base"); o += '{';
  kvBool(o, "valid", r.haveBase);
  if (r.haveBase) {
    kvU(o, "msg", r.base.msgType);
    kvU(o, "station", r.base.stationId);
    kvF(o, "lat", r.base.latDeg, 8);
    kvF(o, "lon", r.base.lonDeg, 8);
    kvF(o, "h", r.base.hM, 3);
    kvU(o, "ago_ms", now - r.baseMs);
  }
  jclose(o, '}');
  o += '}';
  server.send(200, "application/json", o);
}

void handleGetConfig() {
  String o = "{";
  kvStr(o, "ssid", g_s->ssid);
  kvBool(o, "has_pass", g_s->pass[0] != 0);
  kvStr(o, "fallback_ip", IPAddress(g_s->fallbackIp).toString().c_str());
  kvU(o, "fallback_port", g_s->fallbackPort);
  kvU(o, "local_port", g_s->localPort);
  kvU(o, "sysid", g_s->sysid);
  kvU(o, "compid", g_s->compid);
  kvU(o, "gps_baud", g_s->gpsBaud);
  kvU(o, "lock_timeout_s", g_s->lockTimeoutS);
  kvBool(o, "ap_always", g_s->apAlwaysOn);
  jclose(o, '}');
  server.send(200, "application/json", o);
}

void sendErr(const char* msg) {
  String o = "{";
  kvStr(o, "error", msg);
  jclose(o, '}');
  server.send(400, "application/json", o);
}

void handlePostConfig() {
  Settings n = *g_s;
  auto str = [](const char* name, char* dst, size_t cap, bool keepIfEmpty) {
    if (!server.hasArg(name)) return;
    String v = server.arg(name);
    v.trim();
    if (keepIfEmpty && v.isEmpty()) return;
    strncpy(dst, v.c_str(), cap - 1);
    dst[cap - 1] = 0;
  };
  auto num = [](const char* name, long lo, long hi, long& out) -> bool {
    if (!server.hasArg(name)) return true;
    const String v = server.arg(name);
    char* end;
    const long x = strtol(v.c_str(), &end, 10);
    if (v.isEmpty() || *end || x < lo || x > hi) return false;
    out = x;
    return true;
  };

  str("ssid", n.ssid, sizeof(n.ssid), false);
  str("pass", n.pass, sizeof(n.pass), true);     // blank = keep current
  str("ap_pass", n.apPass, sizeof(n.apPass), true);
  if (server.hasArg("clear_pass")) n.pass[0] = 0;
  if (server.hasArg("fallback_ip")) {
    IPAddress ip;
    if (!ip.fromString(server.arg("fallback_ip"))) return sendErr("Bad fallback IP");
    n.fallbackIp = uint32_t(ip);
  }
  long v;
  v = n.fallbackPort; if (!num("fallback_port", 1, 65535, v)) return sendErr("Bad fallback port"); n.fallbackPort = v;
  v = n.localPort;    if (!num("local_port", 1, 65535, v)) return sendErr("Bad local port");       n.localPort = v;
  v = n.sysid;        if (!num("sysid", 1, 255, v)) return sendErr("Bad system ID");              n.sysid = v;
  v = n.compid;       if (!num("compid", 0, 255, v)) return sendErr("Bad component ID");          n.compid = v;
  v = n.gpsBaud;      if (!num("gps_baud", 1, 2000000, v)) return sendErr("Bad baud");            n.gpsBaud = v;
  v = n.lockTimeoutS; if (!num("lock_timeout_s", 1, 65535, v)) return sendErr("Bad lock timeout"); n.lockTimeoutS = v;
  if (server.hasArg("ap_always")) n.apAlwaysOn = server.arg("ap_always") == "1";

  if (const char* err = settingsValidate(n)) return sendErr(err);
  if (!settingsSave(n)) return sendErr("NVS write failed");
  *g_s = n;
  server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
  g_rebootAt = millis() + 1000;
}
}  // namespace

void webBegin(Settings& settings, WifiManager& wifi, MavlinkLink& link, RtcmPipeline& rtcm) {
  g_s = &settings;
  g_wifi = &wifi;
  g_link = &link;
  g_rtcm = &rtcm;

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/reboot", HTTP_POST, [] {
    server.send(200, "application/json", "{\"ok\":true}");
    g_rebootAt = millis() + 500;
  });
  server.on("/api/factory", HTTP_POST, [] {
    settingsFactoryReset();
    server.send(200, "application/json", "{\"ok\":true}");
    g_rebootAt = millis() + 500;
  });
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
}

void webPoll() {
  server.handleClient();
  if (g_rebootAt && int32_t(millis() - g_rebootAt) >= 0) {
    Serial.println("[web] rebooting");
    delay(50);
    ESP.restart();
  }
}
