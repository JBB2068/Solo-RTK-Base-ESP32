#include "rtcm_decode.h"

#include <math.h>

uint64_t rtcmGetBits(const uint8_t* p, unsigned pos, unsigned bits) {
  uint64_t v = 0;
  for (unsigned i = pos; i < pos + bits; i++) v = (v << 1) | ((p[i >> 3] >> (7 - (i & 7))) & 1u);
  return v;
}

int64_t rtcmGetSignedBits(const uint8_t* p, unsigned pos, unsigned bits) {
  uint64_t v = rtcmGetBits(p, pos, bits);
  if (bits < 64 && (v & (1ULL << (bits - 1)))) v |= ~((1ULL << bits) - 1);
  return int64_t(v);
}

void ecefToGeodetic(double x, double y, double z, double& latDeg, double& lonDeg, double& hM) {
  const double a = 6378137.0, f = 1.0 / 298.257223563;
  const double e2 = f * (2 - f);
  const double p = sqrt(x * x + y * y);
  double lat = atan2(z, p * (1 - e2)), n = a, h = 0;
  for (int i = 0; i < 6; i++) {
    const double s = sin(lat);
    n = a / sqrt(1 - e2 * s * s);
    h = p / cos(lat) - n;
    lat = atan2(z, p * (1 - e2 * n / (n + h)));
  }
  latDeg = lat * 180.0 / M_PI;
  lonDeg = atan2(y, x) * 180.0 / M_PI;
  hM = h;
}

bool rtcmDecodeBaseStation(const uint8_t* frame, uint16_t len, RtcmBaseStation& out) {
  if (len < 6 + 19) return false;
  const uint8_t* p = frame + 3;
  const uint16_t type = uint16_t(rtcmGetBits(p, 0, 12));
  if (type != 1005 && type != 1006) return false;
  if (type == 1006 && len < 6 + 21) return false;
  out.msgType = type;
  out.stationId = uint16_t(rtcmGetBits(p, 12, 12));
  out.x = rtcmGetSignedBits(p, 34, 38) * 1e-4;
  out.y = rtcmGetSignedBits(p, 74, 38) * 1e-4;
  out.z = rtcmGetSignedBits(p, 114, 38) * 1e-4;
  out.antennaHeightM = (type == 1006) ? rtcmGetBits(p, 152, 16) * 1e-4 : 0;
  ecefToGeodetic(out.x, out.y, out.z, out.latDeg, out.lonDeg, out.hM);
  return true;
}
