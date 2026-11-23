#pragma once
#include <stdint.h>

// Minimal decoders for the station messages, used for the web UI.
struct RtcmBaseStation {
  uint16_t msgType = 0;    // 1005 or 1006
  uint16_t stationId = 0;
  double x = 0, y = 0, z = 0;          // ECEF ARP, metres
  double latDeg = 0, lonDeg = 0, hM = 0; // WGS84 geodetic
  double antennaHeightM = 0;           // 1006 only
};

// `frame` is a full CRC-checked frame (0xD3..CRC). Returns false if it is not 1005/1006.
bool rtcmDecodeBaseStation(const uint8_t* frame, uint16_t len, RtcmBaseStation& out);

void ecefToGeodetic(double x, double y, double z, double& latDeg, double& lonDeg, double& hM);

// Bit helpers (MSB first, as RTCM3). `pos` and `bits` in bits.
uint64_t rtcmGetBits(const uint8_t* p, unsigned pos, unsigned bits);
int64_t rtcmGetSignedBits(const uint8_t* p, unsigned pos, unsigned bits);
