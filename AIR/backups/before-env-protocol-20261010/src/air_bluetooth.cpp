#include "air_bluetooth.h"
#include <stdio.h>
#include <string.h>

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
  if (!out || !capacity) return 0;
  char pm1[8] = "null", pm25[8] = "null", pm10[8] = "null";
  char temperature[16] = "null", humidity[16] = "null", hcho[16] = "null";
  char age[16] = "null", error[8] = "null";
  if (sample.hasFrame) {
    snprintf(age, sizeof(age), "%lu",
             static_cast<unsigned long>(now - sample.lastFrameMs));
    snprintf(error, sizeof(error), "%u", sample.reading.error);
  }
  if (sample.valuesVisible(now)) {
    const PmsReading &r = sample.reading;
    snprintf(pm1, sizeof(pm1), "%u", r.pm1);
    snprintf(pm25, sizeof(pm25), "%u", r.pm25);
    snprintf(pm10, sizeof(pm10), "%u", r.pm10);
    const int32_t signedTemp = r.temperatureTenths;
    const uint32_t magnitude = signedTemp < 0 ? -signedTemp : signedTemp;
    snprintf(temperature, sizeof(temperature), "%s%lu.%lu",
             signedTemp < 0 ? "-" : "",
             static_cast<unsigned long>(magnitude / 10),
             static_cast<unsigned long>(magnitude % 10));
    snprintf(humidity, sizeof(humidity), "%u.%u",
             r.humidityTenths / 10, r.humidityTenths % 10);
    snprintf(hcho, sizeof(hcho), "%u.%03u", r.hchoUgM3 / 1000,
             r.hchoUgM3 % 1000);
  }
  const int length = snprintf(out, capacity,
      "{\"type\":\"air\",\"v\":1,\"uptime_ms\":%lu,\"status\":\"%s\","
      "\"frames\":%lu,\"age_ms\":%s,\"pm1_ug_m3\":%s,\"pm25_ug_m3\":%s,"
      "\"pm10_ug_m3\":%s,\"temp_c\":%s,\"rh_pct\":%s,\"hcho_mg_m3\":%s,"
      "\"sensor_error\":%s,\"bad_frames\":%lu}\n",
      static_cast<unsigned long>(now), airStatusName(sample.status(now)),
      static_cast<unsigned long>(sample.frames), age, pm1, pm25, pm10,
      temperature, humidity, hcho, error,
      static_cast<unsigned long>(sample.badFrames));
  if (length < 0 || static_cast<size_t>(length) >= capacity) {
    out[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(length);
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
  char line[AirTelemetry::JSON_CAPACITY];
  const size_t length = formatAirTelemetry(line, sizeof(line), sample, now);
  if (length) enqueue(line, length);
}

void AirBluetooth::handle(BluetoothCommand command, const AirTelemetry &sample,
                          uint32_t now) {
  switch (command) {
    case BluetoothCommand::None: break;
    case BluetoothCommand::Ping:
      reply("{\"type\":\"pong\",\"v\":1}\n");
      break;
    case BluetoothCommand::Get:
      report(sample, now);
      lastReportMs_ = now;
      break;
    case BluetoothCommand::StreamOn:
      streamEnabled_ = true;
      lastReportMs_ = now;
      reply("{\"type\":\"ack\",\"command\":\"STREAM ON\",\"stream\":true}\n");
      break;
    case BluetoothCommand::StreamOff:
      streamEnabled_ = false;
      reply("{\"type\":\"ack\",\"command\":\"STREAM OFF\",\"stream\":false}\n");
      break;
    case BluetoothCommand::Help:
      reply("{\"type\":\"help\",\"commands\":[\"PING\",\"GET\",\"STREAM ON\",\"STREAM OFF\",\"HELP\"]}\n");
      break;
    case BluetoothCommand::Unknown:
      reply("{\"type\":\"error\",\"reason\":\"unknown_command\"}\n");
      break;
    case BluetoothCommand::TooLong:
      reply("{\"type\":\"error\",\"reason\":\"command_too_long\"}\n");
      break;
    case BluetoothCommand::Invalid:
      reply("{\"type\":\"error\",\"reason\":\"invalid_command\"}\n");
      break;
  }
}
