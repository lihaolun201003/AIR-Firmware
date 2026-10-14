#pragma once

#include <stddef.h>
#include <stdint.h>
#include "pms5003st.h"

enum class AirStatus { Waiting, Warmup, Live, NoData, SensorError };

struct AirTelemetry {
  static constexpr uint32_t STALE_MS = 10000;
  static constexpr uint32_t WARMUP_MS = 30000;
  static constexpr size_t JSON_CAPACITY = 512;

  PmsReading reading;
  bool hasFrame = false;
  uint32_t lastFrameMs = 0;
  uint32_t sessionStartMs = 0;
  uint32_t frames = 0;
  uint32_t badFrames = 0;

  AirStatus status(uint32_t now) const;
  bool valuesVisible(uint32_t now) const;
};

const char *airStatusName(AirStatus status);
// Includes a terminating LF for the wire, and a NUL for local use.
// Returns zero on insufficient capacity; never publishes a truncated JSON line.
size_t formatAirTelemetry(char *out, size_t capacity,
                          const AirTelemetry &sample, uint32_t now);

enum class BluetoothCommand {
  None, Ping, Get, StreamOn, StreamOff, Help, Unknown, TooLong, Invalid
};

class BluetoothCommandParser {
 public:
  static constexpr size_t MAX_LENGTH = 32;
  static constexpr uint32_t PARTIAL_TIMEOUT_MS = 1000;
  BluetoothCommand feed(uint8_t byte, uint32_t now);

 private:
  char line_[MAX_LENGTH + 1] = {};
  size_t used_ = 0;
  uint32_t lastByteMs_ = 0;
  bool tooLong_ = false;
  bool invalid_ = false;
};

class AirBluetooth {
 public:
  static constexpr uint32_t REPORT_INTERVAL_MS = 1000;
  static constexpr size_t TX_CAPACITY = 1024;

  // A transport needs available(), read(), availableForWrite(), write(data, n).
  // Work per call is bounded; no delay(), flush(), heap, or blocking writes.
  template <typename Transport>
  void poll(Transport &uart, const AirTelemetry &sample, uint32_t now) {
    drain(uart);
    for (size_t count = 0; count < 64 && uart.available() > 0; ++count) {
      const int byte = uart.read();
      if (byte < 0) break;
      handle(commands_.feed(static_cast<uint8_t>(byte), now), sample, now);
    }
    if (streamEnabled_ &&
        static_cast<uint32_t>(now - lastReportMs_) >= REPORT_INTERVAL_MS) {
      lastReportMs_ = now;
      report(sample, now);
    }
    drain(uart);
  }

  bool streamEnabled() const { return streamEnabled_; }
  size_t queuedBytes() const { return used_; }
  uint32_t droppedPackets() const { return droppedPackets_; }

 private:
  template <typename Transport>
  void drain(Transport &uart) {
    size_t budget = 128;
    while (used_ && budget) {
      const int available = uart.availableForWrite();
      if (available <= 0) break;
      size_t count = TX_CAPACITY - head_;
      if (count > used_) count = used_;
      if (count > budget) count = budget;
      if (count > static_cast<size_t>(available)) count = available;
      size_t written = uart.write(
          reinterpret_cast<const uint8_t *>(tx_ + head_), count);
      if (!written) break;
      if (written > count) written = count;
      head_ = (head_ + written) % TX_CAPACITY;
      used_ -= written;
      budget -= written;
    }
  }

  bool enqueue(const char *message, size_t length);
  void reply(const char *message);
  void report(const AirTelemetry &sample, uint32_t now);
  void handle(BluetoothCommand command, const AirTelemetry &sample,
              uint32_t now);

  BluetoothCommandParser commands_;
  char tx_[TX_CAPACITY] = {};
  size_t head_ = 0;
  size_t used_ = 0;
  uint32_t lastReportMs_ = 0;
  uint32_t droppedPackets_ = 0;
  bool streamEnabled_ = true;
};
