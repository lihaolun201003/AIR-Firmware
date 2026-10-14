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
  const int length = snprintf(out, capacity,
      "$ENV,%u,%u,%u,%u.%03u,%s,%u.%u#",
      r.pm1, r.pm25, r.pm10, r.hchoUgM3 / 1000, r.hchoUgM3 % 1000,
      temperature, r.humidityTenths / 10, r.humidityTenths % 10);
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
  char line[AirTelemetry::FRAME_CAPACITY];
  const size_t length = formatAirTelemetry(line, sizeof(line), sample, now);
  if (length) enqueue(line, length);
}

void AirBluetooth::handle(BluetoothCommand command, const AirTelemetry &sample,
                          uint32_t now) {
  switch (command) {
    case BluetoothCommand::None: break;
    case BluetoothCommand::Ping:
      reply("$PONG#");
      break;
    case BluetoothCommand::Get:
      if (sample.valuesVisible(now)) {
        report(sample, now);
      } else {
        char error[40];
        snprintf(error, sizeof(error), "$ERR,%s#",
                 airStatusName(sample.status(now)));
        reply(error);
      }
      lastReportMs_ = now;
      break;
    case BluetoothCommand::StreamOn:
      streamEnabled_ = true;
      lastReportMs_ = now;
      reply("$ACK,STREAM,ON#");
      break;
    case BluetoothCommand::StreamOff:
      streamEnabled_ = false;
      reply("$ACK,STREAM,OFF#");
      break;
    case BluetoothCommand::Help:
      reply("$HELP,PING,GET,STREAM ON,STREAM OFF,HELP#");
      break;
    case BluetoothCommand::Unknown:
      reply("$ERR,UNKNOWN_COMMAND#");
      break;
    case BluetoothCommand::TooLong:
      reply("$ERR,COMMAND_TOO_LONG#");
      break;
    case BluetoothCommand::Invalid:
      reply("$ERR,INVALID_COMMAND#");
      break;
  }
}
