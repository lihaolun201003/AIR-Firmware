#include "air_bluetooth.h"
#include "air_config.h"
#include <stdio.h>
#include <string.h>

namespace {
const char *protocolReply(const char *blinker, const char *env) {
  return AIR_BLUETOOTH_BLINKER ? blinker : env;
}

// Match Blinker's small status query without introducing a JSON dependency.
// The line parser has already converted ASCII to uppercase.
bool isBlinkerHeartbeat(const char *line) {
  const char *expected = "{\"GET\":\"STATE\"}";
  bool quoted = false;
  while (*line) {
    const char c = *line++;
    if (c == ' ' && !quoted) continue;
    if (!*expected || c != *expected++) return false;
    if (c == '"') quoted = !quoted;
  }
  return !*expected;
}

size_t formatTelemetry(char *out, size_t capacity,
                       const AirTelemetry &sample, uint32_t now, bool blinker) {
  if (!out || !capacity) return 0;
  out[0] = '\0';
  if (!sample.valuesVisible(now)) return 0;
  const PmsReading &r = sample.reading;
  char temperature[16];
  const int32_t signedTemp = r.temperatureTenths;
  const uint32_t magnitude = signedTemp < 0 ? -signedTemp : signedTemp;
  snprintf(temperature, sizeof(temperature), "%s%lu.%lu",
           signedTemp < 0 ? "-" : "",
           static_cast<unsigned long>(magnitude / 10),
           static_cast<unsigned long>(magnitude % 10));
  const char *format = blinker
      ? "{\"pm1\":{\"val\":%u},\"pm25\":{\"val\":%u},\"pm10\":{\"val\":%u},"
        "\"hcho\":{\"val\":%u.%03u},\"temp\":{\"val\":%s,\"uni\":\"\\u2103\"},"
        "\"humi\":{\"val\":%u.%u,\"uni\":\"%%\"}}\n"
      : "$ENV,%u,%u,%u,%u.%03u,%s,%u.%u#";
  const int length = snprintf(out, capacity, format,
      r.pm1, r.pm25, r.pm10, r.hchoUgM3 / 1000, r.hchoUgM3 % 1000,
      temperature, r.humidityTenths / 10, r.humidityTenths % 10);
  if (length < 0 || static_cast<size_t>(length) >= capacity) {
    out[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(length);
}
}  // namespace

AirStatus AirTelemetry::status(uint32_t now) const {
  if (!hasFrame) return AirStatus::Waiting;
  if (static_cast<uint32_t>(now - lastFrameMs) >= STALE_MS)
    return AirStatus::NoData;
  if (reading.error) return AirStatus::SensorError;
  if (static_cast<uint32_t>(now - sessionStartMs) < WARMUP_MS)
    return AirStatus::Warmup;
  return AirStatus::Live;
}

bool AirTelemetry::valuesVisible(uint32_t now) const {
  const AirStatus state = status(now);
  return state == AirStatus::Warmup || state == AirStatus::Live;
}

const char *airStatusName(AirStatus status) {
  switch (status) {
    case AirStatus::Waiting: return "WAITING";
    case AirStatus::Warmup: return "WARMUP";
    case AirStatus::Live: return "LIVE";
    case AirStatus::NoData: return "NO_DATA";
    case AirStatus::SensorError: return "SENSOR_ERROR";
  }
  return "WAITING";
}

size_t formatAirTelemetry(char *out, size_t capacity,
                          const AirTelemetry &sample, uint32_t now) {
  return formatTelemetry(out, capacity, sample, now, AIR_BLUETOOTH_BLINKER);
}

size_t formatBlinkerTelemetry(char *out, size_t capacity,
                              const AirTelemetry &sample, uint32_t now) {
  return formatTelemetry(out, capacity, sample, now, true);
}

size_t formatEnvTelemetry(char *out, size_t capacity,
                          const AirTelemetry &sample, uint32_t now) {
  return formatTelemetry(out, capacity, sample, now, false);
}

BluetoothCommand BluetoothCommandParser::feed(uint8_t byte, uint32_t now) {
  if ((used_ || tooLong_ || invalid_) &&
      static_cast<uint32_t>(now - lastByteMs_) > PARTIAL_TIMEOUT_MS) {
    used_ = 0;
    tooLong_ = invalid_ = false;
  }
  lastByteMs_ = now;
  if (byte == '\r' || byte == '\n') {
    BluetoothCommand result = BluetoothCommand::None;
    if (tooLong_) result = BluetoothCommand::TooLong;
    else if (invalid_) result = BluetoothCommand::Invalid;
    else {
      while (used_ && line_[used_ - 1] == ' ') --used_;
      line_[used_] = '\0';
      const char *command = line_;
      while (*command == ' ') ++command;
      if (!*command) result = BluetoothCommand::None;
      else if (!strcmp(command, "PING")) result = BluetoothCommand::Ping;
      else if (!strcmp(command, "GET")) result = BluetoothCommand::Get;
      else if (!strcmp(command, "STREAM ON")) result = BluetoothCommand::StreamOn;
      else if (!strcmp(command, "STREAM OFF")) result = BluetoothCommand::StreamOff;
      else if (!strcmp(command, "HELP")) result = BluetoothCommand::Help;
      else if (AIR_BLUETOOTH_BLINKER && isBlinkerHeartbeat(command))
        result = BluetoothCommand::Heartbeat;
      else if (AIR_BLUETOOTH_BLINKER && *command == '{')
        result = BluetoothCommand::None;  // Unused Blinker widgets/queries.
      else result = BluetoothCommand::Unknown;
    }
    used_ = 0;
    tooLong_ = invalid_ = false;
    return result;
  }
  if (tooLong_ || invalid_) return BluetoothCommand::None;
  if (byte == '\t') byte = ' ';
  if (byte < 0x20 || byte > 0x7E) {
    invalid_ = true;
    return BluetoothCommand::None;
  }
  if (used_ == MAX_LENGTH) {
    tooLong_ = true;
    return BluetoothCommand::None;
  }
  if (byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
  line_[used_++] = static_cast<char>(byte);
  return BluetoothCommand::None;
}

bool AirBluetooth::enqueue(const char *message, size_t length) {
  if (length > TX_CAPACITY - used_) {
    ++droppedPackets_;
    return false;
  }
  size_t tail = (head_ + used_) % TX_CAPACITY;
  for (size_t i = 0; i < length; ++i) {
    tx_[tail] = message[i];
    tail = (tail + 1) % TX_CAPACITY;
  }
  used_ += length;
  return true;
}

void AirBluetooth::reply(const char *message) {
  enqueue(message, strlen(message));
}

void AirBluetooth::report(const AirTelemetry &sample, uint32_t now) {
  char line[AirTelemetry::FRAME_CAPACITY];
  const size_t length = formatAirTelemetry(line, sizeof(line), sample, now);
  if (length) enqueue(line, length);
}

void AirBluetooth::handle(BluetoothCommand command, const AirTelemetry &sample,
                          uint32_t now) {
  switch (command) {
    case BluetoothCommand::None: break;
    case BluetoothCommand::Ping:
      reply(protocolReply("{\"pong\":true}\n", "$PONG#"));
      break;
    case BluetoothCommand::Get:
      if (sample.valuesVisible(now)) {
        report(sample, now);
      } else {
        char error[40];
        snprintf(error, sizeof(error),
                 protocolReply("{\"error\":\"%s\"}\n", "$ERR,%s#"),
                 airStatusName(sample.status(now)));
        reply(error);
      }
      lastReportMs_ = now;
      break;
    case BluetoothCommand::Heartbeat:
      reply("{\"state\":\"connected\"}\n");
      report(sample, now);
      lastReportMs_ = now;
      break;
    case BluetoothCommand::StreamOn:
      streamEnabled_ = true;
      lastReportMs_ = now;
      reply(protocolReply("{\"stream\":\"on\"}\n", "$ACK,STREAM,ON#"));
      break;
    case BluetoothCommand::StreamOff:
      streamEnabled_ = false;
      reply(protocolReply("{\"stream\":\"off\"}\n", "$ACK,STREAM,OFF#"));
      break;
    case BluetoothCommand::Help:
      reply(protocolReply(
          "{\"help\":[\"PING\",\"GET\",\"STREAM ON\",\"STREAM OFF\",\"HELP\"]}\n",
          "$HELP,PING,GET,STREAM ON,STREAM OFF,HELP#"));
      break;
    case BluetoothCommand::Unknown:
      reply(protocolReply("{\"error\":\"UNKNOWN_COMMAND\"}\n", "$ERR,UNKNOWN_COMMAND#"));
      break;
    case BluetoothCommand::TooLong:
      reply(protocolReply("{\"error\":\"COMMAND_TOO_LONG\"}\n", "$ERR,COMMAND_TOO_LONG#"));
      break;
    case BluetoothCommand::Invalid:
      reply(protocolReply("{\"error\":\"INVALID_COMMAND\"}\n", "$ERR,INVALID_COMMAND#"));
      break;
  }
}
