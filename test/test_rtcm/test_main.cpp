#include <string.h>
#include <unity.h>

#include <vector>

#include "crc24q.h"
#include "rtcm3_framer.h"
#include "rtcm_decode.h"
#include <math.h>
#include "rtcm_fragmenter.h"

void setUp() {}
void tearDown() {}

// Build a valid RTCM3 frame with message type `type` and `payloadLen` bytes.
static std::vector<uint8_t> makeFrame(uint16_t type, uint16_t payloadLen, uint8_t fill = 0x5A) {
  std::vector<uint8_t> f(payloadLen + 6, fill);
  f[0] = 0xD3;
  f[1] = (payloadLen >> 8) & 0x03;
  f[2] = payloadLen & 0xFF;
  f[3] = type >> 4;
  f[4] = uint8_t((type & 0x0F) << 4) | (f[4] & 0x0F);
  uint32_t c = crc24q(f.data(), payloadLen + 3);
  f[payloadLen + 3] = c >> 16;
  f[payloadLen + 4] = c >> 8;
  f[payloadLen + 5] = c;
  return f;
}

struct Sink {
  std::vector<std::vector<uint8_t>> frames;
  static void cb(const uint8_t* d, uint16_t n, void* ctx) {
    static_cast<Sink*>(ctx)->frames.emplace_back(d, d + n);
  }
};

void test_crc24q_check_value() {
  const char* s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCDE703, crc24q((const uint8_t*)s, 9));
}

void test_message_type_extract() {
  auto f = makeFrame(1005, 19);
  TEST_ASSERT_EQUAL_UINT16(1005, Rtcm3Framer::messageType(f.data()));
}

void test_framer_back_to_back_with_garbage() {
  Sink s;
  Rtcm3Framer fr(Sink::cb, &s);
  auto a = makeFrame(1005, 19), b = makeFrame(1074, 300), c = makeFrame(1230, 4);
  std::vector<uint8_t> stream = {0x00, 0xFF, 0x12};
  for (auto* f : {&a, &b, &c}) stream.insert(stream.end(), f->begin(), f->end());
  fr.feed(stream.data(), stream.size());
  TEST_ASSERT_EQUAL(3, s.frames.size());
  TEST_ASSERT_EQUAL_UINT16(1074, Rtcm3Framer::messageType(s.frames[1].data()));
  TEST_ASSERT_EQUAL(3, fr.stats().bytesDiscarded);
}

void test_framer_recovers_after_false_preamble() {
  // A stray 0xD3 followed by a plausible 1000-byte length must not eat the
  // real frames behind it.
  Sink s;
  Rtcm3Framer fr(Sink::cb, &s);
  std::vector<uint8_t> stream = {0xD3, 0x03, 0xE8};  // claims 1000-byte payload
  for (int i = 0; i < 5; i++) {
    auto f = makeFrame(1084, 200 + i);
    stream.insert(stream.end(), f.begin(), f.end());
  }
  // pad so the bogus frame "completes" and fails CRC
  stream.resize(stream.size() + 1100, 0x00);
  fr.feed(stream.data(), stream.size());
  TEST_ASSERT_EQUAL(5, s.frames.size());
  TEST_ASSERT_GREATER_OR_EQUAL(1, fr.stats().crcErrors);
}

void test_framer_corrupted_frame_dropped_next_kept() {
  Sink s;
  Rtcm3Framer fr(Sink::cb, &s);
  auto bad = makeFrame(1094, 150), good = makeFrame(1124, 150);
  bad[50] ^= 0x01;
  fr.feed(bad.data(), bad.size());
  fr.feed(good.data(), good.size());
  TEST_ASSERT_EQUAL(1, s.frames.size());
  TEST_ASSERT_EQUAL(1, fr.stats().crcErrors);
}

struct Emits {
  struct E { uint8_t flags, len; };
  std::vector<E> v;
  static bool cb(uint8_t flags, const uint8_t*, uint8_t len, void* ctx) {
    static_cast<Emits*>(ctx)->v.push_back({flags, len});
    return true;
  }
};

static void checkFrag(uint16_t len, std::vector<uint8_t> expectLens, bool fragmented) {
  RtcmFragmenter fg;
  Emits e;
  std::vector<uint8_t> buf(len, 0);
  int n = fg.fragment(buf.data(), len, Emits::cb, &e);
  TEST_ASSERT_EQUAL(int(expectLens.size()), n);
  for (size_t i = 0; i < expectLens.size(); i++) {
    TEST_ASSERT_EQUAL_UINT8(expectLens[i], e.v[i].len);
    TEST_ASSERT_EQUAL(fragmented, bool(e.v[i].flags & 1));
    if (fragmented) TEST_ASSERT_EQUAL_UINT8(i, (e.v[i].flags >> 1) & 3);
  }
}

void test_fragment_sizes() {
  checkFrag(25, {25}, false);
  checkFrag(180, {180}, false);
  checkFrag(181, {180, 1}, true);
  checkFrag(360, {180, 180, 0}, true);
  checkFrag(540, {180, 180, 180, 0}, true);
  checkFrag(719, {180, 180, 180, 179}, true);
  checkFrag(720, {180, 180, 180, 180}, true);
}

void test_fragment_too_large_and_sequence() {
  RtcmFragmenter fg;
  Emits e;
  std::vector<uint8_t> buf(800, 0);
  TEST_ASSERT_EQUAL(-1, fg.fragment(buf.data(), 721, Emits::cb, &e));
  TEST_ASSERT_EQUAL(0, fg.sequence());
  for (int i = 0; i < 33; i++) fg.fragment(buf.data(), 300, Emits::cb, &e);
  TEST_ASSERT_EQUAL(1, fg.sequence());  // wraps mod 32
  TEST_ASSERT_EQUAL_UINT8(31, e.v[62].flags >> 3);
}

static void putBits(uint8_t* p, unsigned pos, unsigned bits, uint64_t v) {
  for (unsigned i = 0; i < bits; i++) {
    unsigned b = pos + i;
    if ((v >> (bits - 1 - i)) & 1) p[b >> 3] |= uint8_t(0x80 >> (b & 7));
    else p[b >> 3] &= uint8_t(~(0x80 >> (b & 7)));
  }
}

void test_decode_1005_roundtrip() {
  // Geodetic -> ECEF, encode into a 1005, decode back.
  const double a = 6378137.0, f = 1.0 / 298.257223563, e2 = f * (2 - f);
  const double lat = 37.8716 * M_PI / 180, lon = -122.2727 * M_PI / 180, h = 123.456;
  const double n = a / sqrt(1 - e2 * sin(lat) * sin(lat));
  const double x = (n + h) * cos(lat) * cos(lon), y = (n + h) * cos(lat) * sin(lon),
               z = (n * (1 - e2) + h) * sin(lat);
  std::vector<uint8_t> payload(19, 0);
  putBits(payload.data(), 0, 12, 1005);
  putBits(payload.data(), 12, 12, 42);
  putBits(payload.data(), 34, 38, uint64_t(llround(x * 1e4)) & ((1ULL << 38) - 1));
  putBits(payload.data(), 74, 38, uint64_t(llround(y * 1e4)) & ((1ULL << 38) - 1));
  putBits(payload.data(), 114, 38, uint64_t(llround(z * 1e4)) & ((1ULL << 38) - 1));
  auto fr = makeFrame(1005, 19);
  memcpy(fr.data() + 3, payload.data(), 19);
  RtcmBaseStation b;
  TEST_ASSERT_EQUAL(true, rtcmDecodeBaseStation(fr.data(), fr.size(), b));
  TEST_ASSERT_EQUAL(42, b.stationId);
  TEST_ASSERT_TRUE(fabs(b.latDeg - 37.8716) < 1e-7);
  TEST_ASSERT_TRUE(fabs(b.lonDeg + 122.2727) < 1e-7);
  TEST_ASSERT_TRUE(fabs(b.hM - 123.456) < 1e-3);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc24q_check_value);
  RUN_TEST(test_message_type_extract);
  RUN_TEST(test_framer_back_to_back_with_garbage);
  RUN_TEST(test_framer_recovers_after_false_preamble);
  RUN_TEST(test_framer_corrupted_frame_dropped_next_kept);
  RUN_TEST(test_fragment_sizes);
  RUN_TEST(test_fragment_too_large_and_sequence);
  RUN_TEST(test_decode_1005_roundtrip);
  return UNITY_END();
}
