#include <unity.h>
#include <algorithm>
#include <string>
#include "air_bluetooth.h"

void setUp() {}
void tearDown() {}

namespace {
AirTelemetry fixture() {
  AirTelemetry sample;
  sample.hasFrame = true;
  sample.lastFrameMs = 31000;
  sample.sessionStartMs = 1000;
  sample.frames = 42;
  sample.reading.pm1 = 11;
  sample.reading.pm25 = 22;
  sample.reading.pm10 = 33;
  sample.reading.temperatureTenths = -52;
  sample.reading.humidityTenths = 654;
  sample.reading.hchoUgM3 = 123;
  return sample;
}

std::string wire(const AirTelemetry &sample, uint32_t now) {
  char line[AirTelemetry::JSON_CAPACITY];
  const size_t size = formatAirTelemetry(line, sizeof(line), sample, now);
  TEST_ASSERT_GREATER_THAN_UINT(0, size);
  return std::string(line, size);
}

void contains(const std::string &text, const char *part) {
  TEST_ASSERT_NOT_EQUAL(std::string::npos, text.find(part));
}

class FakeUart {
 public:
  std::string input, output;
  size_t cursor = 0;
  int writable = 0;
  size_t shortWrite = 512;
  size_t reads = 0;
  int available() const { return static_cast<int>(input.size() - cursor); }
  int read() { ++reads; return available() ? uint8_t(input[cursor++]) : -1; }
  int availableForWrite() const { return writable; }
  size_t write(const uint8_t *bytes, size_t count) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT(static_cast<unsigned>(writable), count);
    const size_t accepted = std::min(count, shortWrite);
    output.append(reinterpret_cast<const char *>(bytes), accepted);
    writable -= accepted;
    return accepted;
  }
};

void drain(AirBluetooth &link, FakeUart &uart, const AirTelemetry &sample,
           uint32_t now) {
  for (unsigned i = 0; i < 100 && link.queuedBytes(); ++i) {
    uart.writable = 16;
    link.poll(uart, sample, now);
  }
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
}

BluetoothCommand command(BluetoothCommandParser &parser, const char *text,
                         uint32_t start = 0) {
  BluetoothCommand result = BluetoothCommand::None;
  for (size_t i = 0; text[i]; ++i) {
    const BluetoothCommand next = parser.feed(text[i], start + i);
    if (next != BluetoothCommand::None) result = next;
  }
  return result;
}

void test_json_units_negative_temperature_and_line_framing() {
  const std::string actual = wire(fixture(), 31000);
  TEST_ASSERT_EQUAL_STRING(
      "{\"type\":\"air\",\"v\":1,\"uptime_ms\":31000,\"status\":\"LIVE\","
      "\"frames\":42,\"age_ms\":0,\"pm1_ug_m3\":11,\"pm25_ug_m3\":22,"
      "\"pm10_ug_m3\":33,\"temp_c\":-5.2,\"rh_pct\":65.4,\"hcho_mg_m3\":0.123,"
      "\"sensor_error\":0,\"bad_frames\":0}\n", actual.c_str());
  AirTelemetry sample = fixture();
  sample.reading.temperatureTenths = -1;
  contains(wire(sample, 31000), "\"temp_c\":-0.1");
}

void test_waiting_stale_and_error_readings_are_null() {
  AirTelemetry sample;
  contains(wire(sample, 1000), "\"status\":\"WAITING\"");
  contains(wire(sample, 1000), "\"age_ms\":null");
  contains(wire(sample, 1000), "\"sensor_error\":null");
  sample = fixture();
  contains(wire(sample, 41000), "\"status\":\"NO_DATA\"");
  contains(wire(sample, 41000), "\"age_ms\":10000");
  sample.reading.error = 2;
  const std::string error = wire(sample, 31000);
  contains(error, "\"status\":\"SENSOR_ERROR\"");
  contains(error, "\"sensor_error\":2");
  const char *fields[] = {"pm1_ug_m3", "pm25_ug_m3", "pm10_ug_m3",
                          "temp_c", "rh_pct", "hcho_mg_m3"};
  for (const char *field : fields) {
    const std::string key = std::string("\"") + field + "\":null";
    contains(error, key.c_str());
    contains(wire(sample, 41000), key.c_str());
    contains(wire(AirTelemetry{}, 1000), key.c_str());
  }
}

void test_status_boundaries_and_millis_rollover() {
  AirTelemetry sample = fixture();
  TEST_ASSERT_TRUE(sample.status(30999) == AirStatus::NoData);
  sample.lastFrameMs = 30999;
  TEST_ASSERT_TRUE(sample.status(30999) == AirStatus::Warmup);
  TEST_ASSERT_TRUE(sample.status(31000) == AirStatus::Live);
  TEST_ASSERT_TRUE(sample.valuesVisible(40998));
  TEST_ASSERT_FALSE(sample.valuesVisible(40999));
  sample.lastFrameMs = 0xFFFFFFF0U;
  sample.sessionStartMs = 0xFFFF0000U;
  TEST_ASSERT_TRUE(sample.status(20) == AirStatus::Live);
  contains(wire(sample, 20), "\"age_ms\":36");
  TEST_ASSERT_TRUE(sample.status(0x2700U) == AirStatus::NoData);
  sample.lastFrameMs = 0;
  sample.sessionStartMs = 0;
  TEST_ASSERT_TRUE(sample.status(0) == AirStatus::Warmup);
}

void test_maximum_values_fit_and_small_buffer_is_rejected() {
  AirTelemetry sample = fixture();
  sample.reading.pm1 = sample.reading.pm25 = sample.reading.pm10 = 65535;
  sample.reading.temperatureTenths = -32768;
  sample.reading.humidityTenths = sample.reading.hchoUgM3 = 65535;
  sample.frames = sample.badFrames = 0xFFFFFFFFU;
  sample.lastFrameMs = 0xFFFFFFF0U;
  sample.sessionStartMs = 0xFFFF0000U;
  const std::string line = wire(sample, 0xFFFFFFF5U);
  TEST_ASSERT_LESS_THAN_UINT(AirTelemetry::JSON_CAPACITY, line.size());
  contains(line, "\"temp_c\":-3276.8");
  contains(line, "\"rh_pct\":6553.5");
  contains(line, "\"hcho_mg_m3\":65.535");
  char tiny[4] = {'x', 'x', 'x', 'x'};
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(tiny, sizeof(tiny), sample, 20));
  TEST_ASSERT_EQUAL_INT(0, tiny[0]);
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(nullptr, 0, sample, 20));
}

void test_commands_accept_case_whitespace_and_crlf() {
  BluetoothCommandParser parser;
  TEST_ASSERT_TRUE(command(parser, "  ping\r\n") == BluetoothCommand::Ping);
  TEST_ASSERT_TRUE(command(parser, "get\n") == BluetoothCommand::Get);
  TEST_ASSERT_TRUE(command(parser, "stream\ton  \r") == BluetoothCommand::StreamOn);
  TEST_ASSERT_TRUE(command(parser, "STREAM OFF\n") == BluetoothCommand::StreamOff);
  TEST_ASSERT_TRUE(command(parser, "HELP\n") == BluetoothCommand::Help);
  TEST_ASSERT_TRUE(command(parser, " \r\n") == BluetoothCommand::None);
  TEST_ASSERT_TRUE(command(parser, "AT+BAUD8\n") == BluetoothCommand::Unknown);
}

void test_command_fragments_timeout_and_rollover() {
  BluetoothCommandParser parser;
  TEST_ASSERT_TRUE(command(parser, "GE", 0) == BluetoothCommand::None);
  TEST_ASSERT_TRUE(command(parser, "T\n", 100) == BluetoothCommand::Get);
  command(parser, "P", 200);
  TEST_ASSERT_TRUE(command(parser, "GET\n", 1300) == BluetoothCommand::Get);
  command(parser, "GE", 0xFFFFFFFEU);
  TEST_ASSERT_TRUE(command(parser, "T\n", 0) == BluetoothCommand::Get);
}

void test_long_and_binary_commands_discard_through_newline_then_recover() {
  BluetoothCommandParser parser;
  const std::string longLine = std::string(80, 'X') + "GET\n";
  TEST_ASSERT_TRUE(command(parser, longLine.c_str()) == BluetoothCommand::TooLong);
  TEST_ASSERT_TRUE(command(parser, "PING\n", 200) == BluetoothCommand::Ping);
  parser.feed('G', 300);
  parser.feed(0, 301);
  TEST_ASSERT_TRUE(command(parser, "ET\n", 302) == BluetoothCommand::Invalid);
  TEST_ASSERT_TRUE(command(parser, "GET\n", 310) == BluetoothCommand::Get);
}

void test_stream_interval_and_get_while_paused() {
  AirBluetooth link;
  FakeUart uart;
  const AirTelemetry sample = fixture();
  link.poll(uart, sample, 999);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  link.poll(uart, sample, 1000);
  drain(link, uart, sample, 1000);
  TEST_ASSERT_EQUAL_STRING(wire(sample, 1000).c_str(), uart.output.c_str());
  uart.output.clear();
  uart.input = "STREAM OFF\n";
  link.poll(uart, sample, 1001);
  drain(link, uart, sample, 1001);
  TEST_ASSERT_FALSE(link.streamEnabled());
  TEST_ASSERT_EQUAL_STRING(
      "{\"type\":\"ack\",\"command\":\"STREAM OFF\",\"stream\":false}\n",
      uart.output.c_str());
  uart.output.clear();
  link.poll(uart, sample, 31000);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  uart.input += "GET\r\n";
  link.poll(uart, sample, 31000);
  drain(link, uart, sample, 31000);
  TEST_ASSERT_EQUAL_STRING(wire(sample, 31000).c_str(), uart.output.c_str());
}

void test_ping_help_and_resume_acknowledgments() {
  AirBluetooth link;
  FakeUart uart;
  uart.input = "PING\nHELP\nSTREAM ON\n";
  link.poll(uart, fixture(), 10);
  drain(link, uart, fixture(), 10);
  TEST_ASSERT_EQUAL_STRING(
      "{\"type\":\"pong\",\"v\":1}\n"
      "{\"type\":\"help\",\"commands\":[\"PING\",\"GET\",\"STREAM ON\",\"STREAM OFF\",\"HELP\"]}\n"
      "{\"type\":\"ack\",\"command\":\"STREAM ON\",\"stream\":true}\n",
      uart.output.c_str());
  TEST_ASSERT_TRUE(link.streamEnabled());
  link.poll(uart, fixture(), 1009);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  link.poll(uart, fixture(), 1010);
  TEST_ASSERT_GREATER_THAN_UINT(0, link.queuedBytes());
}

void test_queue_pressure_short_writes_and_ring_wrap_preserve_whole_packets() {
  AirBluetooth link;
  FakeUart uart;
  uart.shortWrite = 3;
  const AirTelemetry sample = fixture();
  std::string expected;
  for (uint32_t now = 1000; now <= 10000; now += 1000) {
    const uint32_t dropped = link.droppedPackets();
    link.poll(uart, sample, now);  // UART accepts no data.
    if (dropped == link.droppedPackets()) expected += wire(sample, now);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(AirBluetooth::TX_CAPACITY, link.queuedBytes());
  }
  TEST_ASSERT_GREATER_THAN_UINT32(0, link.droppedPackets());
  TEST_ASSERT_EQUAL_UINT(0, uart.output.size());
  drain(link, uart, sample, 10000);
  for (uint32_t now = 11000; now <= 20000; now += 1000) {
    uart.writable = 0;
    link.poll(uart, sample, now);
    expected += wire(sample, now);
    drain(link, uart, sample, now);
  }
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), uart.output.c_str());
}

void test_receive_flood_has_bounded_work() {
  AirBluetooth link;
  FakeUart uart;
  for (unsigned i = 0; i < 100; ++i) uart.input += "PING\n";
  link.poll(uart, fixture(), 0);
  TEST_ASSERT_EQUAL_UINT(64, uart.reads);
  for (unsigned i = 0; i < 8; ++i) link.poll(uart, fixture(), 0);
  TEST_ASSERT_EQUAL_UINT(0, uart.available());
  TEST_ASSERT_GREATER_THAN_UINT32(0, link.droppedPackets());
}

void test_pms_frame_to_bluetooth_uses_atmospheric_fields_and_scales() {
  uint8_t bytes[40] = {
      0x42, 0x4D, 0, 36, 0, 101, 0, 202, 1, 47, 0, 11, 0, 22, 0, 33,
      1, 44, 0, 200, 0, 100, 0, 50, 0, 10, 0, 2, 0, 123, 0xFF, 0xCC,
      2, 142, 0, 0, 7, 0, 0, 0};
  uint16_t checksum = 0;
  for (size_t i = 0; i < 38; ++i) checksum += bytes[i];
  bytes[38] = checksum >> 8;
  bytes[39] = checksum & 0xFF;
  Pms5003stParser parser;
  AirTelemetry sample;
  for (size_t i = 0; i < sizeof(bytes); ++i)
    if (parser.feed(bytes[i], 1000 + i, sample.reading)) sample.hasFrame = true;
  sample.frames = parser.frames;
  sample.lastFrameMs = 1039;
  sample.sessionStartMs = 1000;
  AirBluetooth link;
  FakeUart uart;
  uart.input = "GET\n";
  link.poll(uart, sample, 1040);
  drain(link, uart, sample, 1040);
  contains(uart.output, "\"status\":\"WARMUP\"");
  contains(uart.output, "\"pm25_ug_m3\":22");
  contains(uart.output, "\"temp_c\":-5.2");
  contains(uart.output, "\"hcho_mg_m3\":0.123");
  contains(uart.output, "\"age_ms\":1");
}
}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_json_units_negative_temperature_and_line_framing);
  RUN_TEST(test_waiting_stale_and_error_readings_are_null);
  RUN_TEST(test_status_boundaries_and_millis_rollover);
  RUN_TEST(test_maximum_values_fit_and_small_buffer_is_rejected);
  RUN_TEST(test_commands_accept_case_whitespace_and_crlf);
  RUN_TEST(test_command_fragments_timeout_and_rollover);
  RUN_TEST(test_long_and_binary_commands_discard_through_newline_then_recover);
  RUN_TEST(test_stream_interval_and_get_while_paused);
  RUN_TEST(test_ping_help_and_resume_acknowledgments);
  RUN_TEST(test_queue_pressure_short_writes_and_ring_wrap_preserve_whole_packets);
  RUN_TEST(test_receive_flood_has_bounded_work);
  RUN_TEST(test_pms_frame_to_bluetooth_uses_atmospheric_fields_and_scales);
  return UNITY_END();
}
