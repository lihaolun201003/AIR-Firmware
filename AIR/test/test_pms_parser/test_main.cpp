#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include "pms5003st.h"

// Independent protocol fixture: CF1=101/202/303; atmospheric=11/22/33;
// particles=300/200/100/50/10/2; HCHO=123 ug/m3; T=-5.2 C; RH=65.4%.
static const uint8_t payload[38] = {
    0x42, 0x4D, 0, 36, 0, 101, 0, 202, 1, 47, 0, 11, 0, 22, 0, 33,
    1, 44, 0, 200, 0, 100, 0, 50, 0, 10, 0, 2, 0, 123, 0xFF, 0xCC,
    2, 142, 0, 0, 7, 0};

void setUp() {}
void tearDown() {}

static void seal(uint8_t *frame) {
  uint16_t sum = 0;
  for (size_t i = 0; i < 38; ++i) sum += frame[i];
  frame[38] = sum >> 8;
  frame[39] = sum & 0xFF;
}

static void fixture(uint8_t *frame) {
  memcpy(frame, payload, sizeof(payload));
  seal(frame);
}

static unsigned send(Pms5003stParser &p, PmsReading &r, const uint8_t *data,
                     size_t count, uint32_t start = 10) {
  unsigned accepted = 0;
  for (size_t i = 0; i < count; ++i) accepted += p.feed(data[i], start + i, r);
  return accepted;
}

static void test_decodes_all_fields_and_signed_temperature() {
  Pms5003stParser p;
  PmsReading r;
  uint8_t frame[40]; fixture(frame);
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40));
  TEST_ASSERT_EQUAL_UINT16(101, r.pm1Cf1);
  TEST_ASSERT_EQUAL_UINT16(202, r.pm25Cf1);
  TEST_ASSERT_EQUAL_UINT16(303, r.pm10Cf1);
  TEST_ASSERT_EQUAL_UINT16(11, r.pm1);
  TEST_ASSERT_EQUAL_UINT16(22, r.pm25);
  TEST_ASSERT_EQUAL_UINT16(33, r.pm10);
  const uint16_t counts[] = {300, 200, 100, 50, 10, 2};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(counts, r.particles, 6);
  TEST_ASSERT_EQUAL_UINT16(123, r.hchoUgM3);
  TEST_ASSERT_EQUAL_INT16(-52, r.temperatureTenths);
  TEST_ASSERT_EQUAL_UINT16(654, r.humidityTenths);
  TEST_ASSERT_EQUAL_UINT8(7, r.version);
  TEST_ASSERT_EQUAL_UINT8(0, r.error);
}

static void test_bad_checksum_does_not_publish_values() {
  Pms5003stParser p; PmsReading r; r.pm25 = 999;
  uint8_t frame[40]; fixture(frame); frame[12] ^= 1;
  TEST_ASSERT_EQUAL_UINT(0, send(p, r, frame, 40));
  TEST_ASSERT_EQUAL_UINT16(999, r.pm25);
  TEST_ASSERT_EQUAL_UINT32(1, p.checksumErrors);
  fixture(frame);
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40, 60));
}

static void test_rejects_32_byte_variant_and_recovers_from_noise() {
  Pms5003stParser p; PmsReading r;
  uint8_t frame[40]; fixture(frame);
  const uint8_t noise[] = {0, 0x4D, 0x42, 0x42, 0x4D, 0, 28, 0x42};
  TEST_ASSERT_EQUAL_UINT(0, send(p, r, noise, sizeof(noise)));
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40, 30));
  TEST_ASSERT_GREATER_THAN_UINT32(0, p.lengthErrors);
}

static void test_partial_frame_recovery_preserves_embedded_new_header() {
  Pms5003stParser p; PmsReading r;
  uint8_t frame[40]; fixture(frame);
  TEST_ASSERT_EQUAL_UINT(0, send(p, r, frame, 17));
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40, 30));
  TEST_ASSERT_EQUAL_UINT16(22, r.pm25);
}

static void test_timeout_and_millis_wraparound() {
  Pms5003stParser p; PmsReading r;
  uint8_t frame[40]; fixture(frame);
  TEST_ASSERT_EQUAL_UINT(0, send(p, r, frame, 10));
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40, 1000));
  TEST_ASSERT_EQUAL_UINT32(1, p.timeouts);
  Pms5003stParser wrap;
  TEST_ASSERT_EQUAL_UINT(1, send(wrap, r, frame, 40, 0xFFFFFFF0U));
  TEST_ASSERT_EQUAL_UINT32(0, wrap.timeouts);
}

static void test_back_to_back_frames_and_header_bytes_in_payload() {
  Pms5003stParser p; PmsReading r;
  uint8_t frame[40]; fixture(frame);
  frame[16] = 0x42; frame[17] = 0x4D;
  frame[30] = 1; frame[31] = 0x13;  // +27.5 C.
  frame[37] = 2; seal(frame);       // Preserve the sensor error code.
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40));
  TEST_ASSERT_EQUAL_UINT(1, send(p, r, frame, 40, 50));
  TEST_ASSERT_EQUAL_UINT16(0x424D, r.particles[0]);
  TEST_ASSERT_EQUAL_INT16(275, r.temperatureTenths);
  TEST_ASSERT_EQUAL_UINT8(2, r.error);
  TEST_ASSERT_EQUAL_UINT32(2, p.frames);
}

static int runTests() {
  UNITY_BEGIN();
  RUN_TEST(test_decodes_all_fields_and_signed_temperature);
  RUN_TEST(test_bad_checksum_does_not_publish_values);
  RUN_TEST(test_rejects_32_byte_variant_and_recovers_from_noise);
  RUN_TEST(test_partial_frame_recovery_preserves_embedded_new_header);
  RUN_TEST(test_timeout_and_millis_wraparound);
  RUN_TEST(test_back_to_back_frames_and_header_bytes_in_payload);
  return UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000);
  runTests();
}
void loop() {}
#else
int main() { return runTests(); }
#endif
