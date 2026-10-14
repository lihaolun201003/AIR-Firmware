#include <unity.h>
#include <algorithm>
#include <string>
#include "air_bluetooth.h"
#include "air_config.h"

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
  char line[AirTelemetry::FRAME_CAPACITY];
  const size_t size = formatAirTelemetry(line, sizeof(line), sample, now);
  TEST_ASSERT_GREATER_THAN_UINT(0, size);
  return std::string(line, size);
}

std::string envWire(const AirTelemetry &sample, uint32_t now) {
  char line[AirTelemetry::FRAME_CAPACITY];
  const size_t size = formatEnvTelemetry(line, sizeof(line), sample, now);
  TEST_ASSERT_GREATER_THAN_UINT(0, size);
  return std::string(line, size);
}

std::string blinkerWire(const AirTelemetry &sample, uint32_t now) {
  char line[AirTelemetry::FRAME_CAPACITY];
  const size_t size = formatBlinkerTelemetry(line, sizeof(line), sample, now);
  TEST_ASSERT_GREATER_THAN_UINT(0, size);
  return std::string(line, size);
}

const char *expectedReply(const char *blinker, const char *env) {
  return AIR_BLUETOOTH_BLINKER ? blinker : env;
}

const char *expectedFixture() {
  return expectedReply(
      "{\"pm1\":{\"val\":11},\"pm25\":{\"val\":22},\"pm10\":{\"val\":33},"
      "\"hcho\":{\"val\":0.123},\"temp\":{\"val\":-5.2,\"uni\":\"\\u2103\"},"
      "\"humi\":{\"val\":65.4,\"uni\":\"%\"}}\n",
      "$ENV,11,22,33,0.123,-5.2,65.4#");
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

void test_env_matches_phone_example_and_exact_framing() {
  AirTelemetry sample = fixture();
  sample.reading.pm1 = 18;
  sample.reading.pm25 = 35;
  sample.reading.pm10 = 48;
  sample.reading.hchoUgM3 = 23;
  sample.reading.temperatureTenths = 256;
  sample.reading.humidityTenths = 612;
  const std::string actual = envWire(sample, 31000);
  TEST_ASSERT_EQUAL_STRING("$ENV,18,35,48,0.023,25.6,61.2#", actual.c_str());
  TEST_ASSERT_EQUAL_UINT(6, std::count(actual.begin(), actual.end(), ','));
  TEST_ASSERT_EQUAL_INT('#', actual.back());
  TEST_ASSERT_TRUE(actual.find('\n') == std::string::npos);
  TEST_ASSERT_TRUE(actual.find('\r') == std::string::npos);
}

void test_env_negative_temperature_and_zero_values() {
  AirTelemetry sample = fixture();
  TEST_ASSERT_EQUAL_STRING("$ENV,11,22,33,0.123,-5.2,65.4#",
                           envWire(sample, 31000).c_str());
  sample.reading.temperatureTenths = -1;
  contains(envWire(sample, 31000), ",0.123,-0.1,65.4#");
  sample.reading = PmsReading{};
  TEST_ASSERT_EQUAL_STRING("$ENV,0,0,0,0.000,0.0,0.0#",
                           envWire(sample, 31000).c_str());
}

void test_blinker_matches_requested_values_units_and_newline() {
  AirTelemetry sample = fixture();
  sample.reading.pm1 = 10;
  sample.reading.pm25 = 20;
  sample.reading.pm10 = 30;
  sample.reading.hchoUgM3 = 15;
  sample.reading.temperatureTenths = 286;
  sample.reading.humidityTenths = 725;
  const std::string actual = blinkerWire(sample, 31000);
  TEST_ASSERT_EQUAL_STRING(
      "{\"pm1\":{\"val\":10},\"pm25\":{\"val\":20},\"pm10\":{\"val\":30},"
      "\"hcho\":{\"val\":0.015},\"temp\":{\"val\":28.6,\"uni\":\"\\u2103\"},"
      "\"humi\":{\"val\":72.5,\"uni\":\"%\"}}\n", actual.c_str());
  TEST_ASSERT_EQUAL_UINT(1, std::count(actual.begin(), actual.end(), '\n'));
  TEST_ASSERT_EQUAL_INT('\n', actual.back());
  TEST_ASSERT_TRUE(actual.find('$') == std::string::npos);
  TEST_ASSERT_TRUE(actual.find('#') == std::string::npos);
  TEST_ASSERT_EQUAL_STRING(
      (AIR_BLUETOOTH_BLINKER ? actual : envWire(sample, 31000)).c_str(),
      wire(sample, 31000).c_str());
}

void test_blinker_negative_temperature_and_real_zero_values() {
  AirTelemetry sample = fixture();
  sample.reading.temperatureTenths = -1;
  contains(blinkerWire(sample, 31000), "\"temp\":{\"val\":-0.1,");
  sample.reading = PmsReading{};
  contains(blinkerWire(sample, 31000), "\"hcho\":{\"val\":0.000}");
  contains(blinkerWire(sample, 31000), "\"temp\":{\"val\":0.0,");
  contains(blinkerWire(sample, 31000), "\"humi\":{\"val\":0.0,");
}

void test_invalid_readings_are_not_published() {
  AirTelemetry sample;
  char line[AirTelemetry::FRAME_CAPACITY] = "old data";
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(line, sizeof(line), sample, 1000));
  TEST_ASSERT_EQUAL_STRING("", line);
  sample = fixture();
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(line, sizeof(line), sample, 41000));
  TEST_ASSERT_EQUAL_STRING("", line);
  sample.reading.error = 2;
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(line, sizeof(line), sample, 31000));
  TEST_ASSERT_EQUAL_STRING("", line);
  sample.reading.error = 0;
  TEST_ASSERT_EQUAL_STRING(expectedFixture(),
                           wire(sample, 31000).c_str());
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
  TEST_ASSERT_EQUAL_STRING(expectedFixture(),
                           wire(sample, 20).c_str());
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
  TEST_ASSERT_LESS_THAN_UINT(AirTelemetry::FRAME_CAPACITY, line.size());
  TEST_ASSERT_EQUAL_STRING("$ENV,65535,65535,65535,65.535,-3276.8,6553.5#",
                           envWire(sample, 0xFFFFFFF5U).c_str());
  const std::string blinker = blinkerWire(sample, 0xFFFFFFF5U);
  TEST_ASSERT_LESS_THAN_UINT(AirTelemetry::FRAME_CAPACITY, blinker.size());
  contains(blinker, "\"temp\":{\"val\":-3276.8,");
  contains(blinker, "\"humi\":{\"val\":6553.5,");
  char tiny[4] = {'x', 'x', 'x', 'x'};
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(tiny, sizeof(tiny), sample, 20));
  TEST_ASSERT_EQUAL_INT(0, tiny[0]);
  TEST_ASSERT_EQUAL_UINT(0, formatAirTelemetry(nullptr, 0, sample, 20));
  TEST_ASSERT_EQUAL_UINT(0, formatBlinkerTelemetry(tiny, sizeof(tiny), sample, 20));
  TEST_ASSERT_EQUAL_INT(0, tiny[0]);
  TEST_ASSERT_EQUAL_UINT(0, formatEnvTelemetry(tiny, sizeof(tiny), sample, 20));
}

void test_blinker_heartbeat_fragments_whitespace_and_other_queries() {
  BluetoothCommandParser parser;
  command(parser, "{\"get\":", 0);
  TEST_ASSERT_TRUE(command(parser, "\"state\"}\r\n", 100) ==
      (AIR_BLUETOOTH_BLINKER ? BluetoothCommand::Heartbeat : BluetoothCommand::Unknown));
  TEST_ASSERT_TRUE(command(parser, " { \"get\" : \"state\" } \n", 200) ==
      (AIR_BLUETOOTH_BLINKER ? BluetoothCommand::Heartbeat : BluetoothCommand::Unknown));
  TEST_ASSERT_TRUE(command(parser, "{\"get\":\" sta te\"}\n", 300) ==
      (AIR_BLUETOOTH_BLINKER ? BluetoothCommand::None : BluetoothCommand::Unknown));
  TEST_ASSERT_TRUE(command(parser, "{\"get\":\"version\"}\n", 400) ==
      (AIR_BLUETOOTH_BLINKER ? BluetoothCommand::None : BluetoothCommand::Unknown));
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
  AirTelemetry sample = fixture();
  sample.lastFrameMs = 1000;
  sample.sessionStartMs = 0;
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
  TEST_ASSERT_EQUAL_STRING(expectedReply("{\"stream\":\"off\"}\n", "$ACK,STREAM,OFF#"),
                           uart.output.c_str());
  uart.output.clear();
  sample.lastFrameMs = 31000;
  link.poll(uart, sample, 31000);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  uart.input += "GET\r\n";
  link.poll(uart, sample, 31000);
  drain(link, uart, sample, 31000);
  TEST_ASSERT_EQUAL_STRING(wire(sample, 31000).c_str(), uart.output.c_str());
}

void test_invalid_stream_is_silent_and_get_reports_reason() {
  AirBluetooth link;
  FakeUart uart;
  AirTelemetry sample;
  link.poll(uart, sample, 1000);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  uart.input = "GET\n";
  link.poll(uart, sample, 1001);
  drain(link, uart, sample, 1001);
  TEST_ASSERT_EQUAL_STRING(expectedReply("{\"error\":\"WAITING\"}\n", "$ERR,WAITING#"),
                           uart.output.c_str());
  sample = fixture();
  sample.reading.error = 2;
  uart.input += "GET\n";
  uart.output.clear();
  link.poll(uart, sample, 31000);
  drain(link, uart, sample, 31000);
  TEST_ASSERT_EQUAL_STRING(expectedReply("{\"error\":\"SENSOR_ERROR\"}\n", "$ERR,SENSOR_ERROR#"),
                           uart.output.c_str());
  uart.output.clear();
  link.poll(uart, sample, 41000);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  uart.input += "GET\n";
  link.poll(uart, sample, 41001);
  drain(link, uart, sample, 41001);
  TEST_ASSERT_EQUAL_STRING(expectedReply("{\"error\":\"NO_DATA\"}\n", "$ERR,NO_DATA#"),
                           uart.output.c_str());
  uart.output.clear();
  sample.reading.error = 0;
  sample.lastFrameMs = 43000;
  link.poll(uart, sample, 43000);
  drain(link, uart, sample, 43000);
  TEST_ASSERT_EQUAL_STRING(wire(sample, 43000).c_str(), uart.output.c_str());
}

void test_ping_help_and_resume_acknowledgments() {
  AirBluetooth link;
  FakeUart uart;
  AirTelemetry sample = fixture();
  sample.lastFrameMs = 10;
  sample.sessionStartMs = 0;
  uart.input = "PING\nHELP\nSTREAM ON\n";
  link.poll(uart, sample, 10);
  drain(link, uart, sample, 10);
  TEST_ASSERT_EQUAL_STRING(
      expectedReply(
          "{\"pong\":true}\n"
          "{\"help\":[\"PING\",\"GET\",\"STREAM ON\",\"STREAM OFF\",\"HELP\"]}\n"
          "{\"stream\":\"on\"}\n",
          "$PONG#$HELP,PING,GET,STREAM ON,STREAM OFF,HELP#$ACK,STREAM,ON#"),
      uart.output.c_str());
  TEST_ASSERT_TRUE(link.streamEnabled());
  link.poll(uart, sample, 1009);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
  link.poll(uart, sample, 1010);
  TEST_ASSERT_GREATER_THAN_UINT(0, link.queuedBytes());
}

void test_blinker_heartbeat_reports_immediately_while_stream_paused() {
  if (!AIR_BLUETOOTH_BLINKER) return;
  AirBluetooth link;
  FakeUart uart;
  AirTelemetry sample = fixture();
  uart.input = "STREAM OFF\n";
  link.poll(uart, sample, 30999);
  drain(link, uart, sample, 30999);
  uart.output.clear();
  uart.input += "{\"get\":\"state\"}\n";
  link.poll(uart, sample, 31000);
  drain(link, uart, sample, 31000);
  const std::string expected = std::string("{\"state\":\"connected\"}\n") + expectedFixture();
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), uart.output.c_str());
  TEST_ASSERT_FALSE(link.streamEnabled());
}

void test_blinker_heartbeat_waiting_does_not_send_fake_zero_values() {
  if (!AIR_BLUETOOTH_BLINKER) return;
  AirBluetooth link;
  FakeUart uart;
  AirTelemetry sample;
  uart.input = "{\"get\":\"state\"}\n";
  link.poll(uart, sample, 1000);
  drain(link, uart, sample, 1000);
  TEST_ASSERT_EQUAL_STRING("{\"state\":\"connected\"}\n", uart.output.c_str());
  uart.output.clear();
  uart.input += "{\"get\":\"version\"}\n";
  link.poll(uart, sample, 1001);
  TEST_ASSERT_EQUAL_UINT(0, link.queuedBytes());
}

void test_queue_pressure_short_writes_and_ring_wrap_preserve_whole_packets() {
  AirBluetooth link;
  FakeUart uart;
  uart.shortWrite = 3;
  AirTelemetry sample = fixture();
  std::string expected;
  for (uint32_t now = 1000; now <= 40000; now += 1000) {
    sample.lastFrameMs = now;
    sample.sessionStartMs = 0;
    sample.reading.pm25 = now / 1000;
    const uint32_t dropped = link.droppedPackets();
    link.poll(uart, sample, now);  // UART accepts no data.
    if (dropped == link.droppedPackets()) expected += wire(sample, now);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(AirBluetooth::TX_CAPACITY, link.queuedBytes());
  }
  TEST_ASSERT_GREATER_THAN_UINT32(0, link.droppedPackets());
  TEST_ASSERT_EQUAL_UINT(0, uart.output.size());
  drain(link, uart, sample, 40000);
  for (uint32_t now = 41000; now <= 60000; now += 1000) {
    sample.lastFrameMs = now;
    sample.reading.pm25 = now / 1000;
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
  for (unsigned i = 0; i < 300; ++i) uart.input += "PING\n";
  link.poll(uart, fixture(), 0);
  TEST_ASSERT_EQUAL_UINT(64, uart.reads);
  for (unsigned i = 0; i < 24; ++i) link.poll(uart, fixture(), 0);
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
  TEST_ASSERT_TRUE(sample.status(1040) == AirStatus::Warmup);
  TEST_ASSERT_EQUAL_STRING(expectedFixture(), uart.output.c_str());
}
}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_env_matches_phone_example_and_exact_framing);
  RUN_TEST(test_env_negative_temperature_and_zero_values);
  RUN_TEST(test_blinker_matches_requested_values_units_and_newline);
  RUN_TEST(test_blinker_negative_temperature_and_real_zero_values);
  RUN_TEST(test_invalid_readings_are_not_published);
  RUN_TEST(test_status_boundaries_and_millis_rollover);
  RUN_TEST(test_maximum_values_fit_and_small_buffer_is_rejected);
  RUN_TEST(test_commands_accept_case_whitespace_and_crlf);
  RUN_TEST(test_blinker_heartbeat_fragments_whitespace_and_other_queries);
  RUN_TEST(test_command_fragments_timeout_and_rollover);
  RUN_TEST(test_long_and_binary_commands_discard_through_newline_then_recover);
  RUN_TEST(test_stream_interval_and_get_while_paused);
  RUN_TEST(test_invalid_stream_is_silent_and_get_reports_reason);
  RUN_TEST(test_ping_help_and_resume_acknowledgments);
  RUN_TEST(test_blinker_heartbeat_reports_immediately_while_stream_paused);
  RUN_TEST(test_blinker_heartbeat_waiting_does_not_send_fake_zero_values);
  RUN_TEST(test_queue_pressure_short_writes_and_ring_wrap_preserve_whole_packets);
  RUN_TEST(test_receive_flood_has_bounded_work);
  RUN_TEST(test_pms_frame_to_bluetooth_uses_atmospheric_fields_and_scales);
  return UNITY_END();
}
