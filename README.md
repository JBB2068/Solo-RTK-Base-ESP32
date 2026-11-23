# solo-rtk-base

ESP32-C3 RTK base for a 3DR Solo (ArduPilot / Open Solo). Reads RTCM3 from a
base GPS on `Serial1`, validates it, and injects it as MAVLink2
`GPS_RTCM_DATA` over SoloLink UDP. Replaces a Pi running SolexBase.

FC params: `GPS_INJECT_TO = 127`, `GPS_AUTO_CONFIG = 1`.

## Build / flash

```
pio run -e esp32c3 -t upload
pio device monitor
pio test -e native        # host-side tests for framer / CRC / fragmenter / 1005 decode
```

## First boot

With no SSID stored, it starts a setup AP `SoloRTK-XXXX` (password `solortk1`).
Join it and open `http://192.168.4.1`, enter the SoloLink SSID/password,
Save. After that the UI is at the ESP's SoloLink IP (shown on the serial log
and in the UI) or `http://solortk.local` where mDNS works.

The setup AP comes back on its own if SoloLink is unreachable for 30 s, and
goes away 2 min after SoloLink reconnects (unless "Always on" is set).

## Layout

| Path | What |
|---|---|
| `lib/rtcm/` | Pure C++, no Arduino: `crc24q`, `Rtcm3Framer`, `RtcmFragmenter`, 1005/1006 decode. Unit-tested on host. |
| `src/rtcm_pipeline.*` | FreeRTOS task (prio 3): drains `Serial1`, frames, fragments, sends. Per-message-type stats. |
| `src/mavlink_link.*` | UDP socket on 14550, endpoint auto-lock, `GPS_RTCM_DATA` send, vehicle `GPS_RAW_INT` readback. |
| `src/wifi_manager.*` | STA supervisor, reconnects, setup AP, reboot-if-dead. |
| `src/web_ui.*`, `src/web_page.h` | Port-80 UI and JSON API. |
| `src/settings.*` | NVS-backed config (`Preferences`, namespace `rtkbase`). |
| `src/config.h` | Pins, defaults, timing constants. |

## Behaviour

- **RTCM framing**: `0xD3` + 6 reserved zero bits + 10-bit length, CRC-24Q.
  On a CRC or header failure only the false preamble is dropped and the
  buffered bytes are rescanned.
- **Fragmentation** (matches `AP_GPS::handle_gps_rtcm_fragment`): <=180 B
  unfragmented; 181-720 B in 180 B fragments with a 5-bit sequence; a 0-length
  closer for exactly 360/540 B; 720 B needs none (4 full fragments);
  >720 B dropped and counted.
- **Endpoint lock**: locks onto the source IP:port of an autopilot
  `HEARTBEAT` (compid 1, not a GCS). Any valid MAVLink from that endpoint
  keeps it alive. After `lock_timeout_s` (10 s) of silence it reverts to the
  fallback `10.1.1.10:14560`.
- **Watchdog**: task WDT 10 s with panic/reset, subscribed by `loop()` and the
  RTCM task, plus the idle task. The reset reason is shown in the UI.
- **Wi-Fi**: core auto-reconnect off; retries every 8 s (60 s while a phone is
  on the setup AP, since STA scans make the AP unusable); UDP socket rebound on
  every GOT_IP; reboot after 10 min without SoloLink if the AP is idle.

## Web UI / API

`GET /api/status` gives live JSON: RTCM rate, frames, CRC errors, per-type
counts and ages, base position from 1005/1006, link target, tx counts, the
vehicle's GPS fix type (RTK float/fixed from `GPS_RAW_INT`), Wi-Fi RSSI and
reconnect counters, heap and reset reason. `GET/POST /api/config`,
`POST /api/reboot`, `POST /api/factory`. No auth: it is only reachable on
SoloLink and the setup AP.

## Not done yet

- GPS auto-config on boot (UBX for a u-blox ZED-F9P, or Unicore commands for a
  UM980). Waiting to identify the module.
- Survey-in progress (needs UBX NAV-SVIN or the Unicore equivalent on the UART).
  For now the UI shows the base position once 1005/1006 appears, which on a
  u-blox only happens after survey-in completes.
