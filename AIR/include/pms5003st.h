#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Plantower PMS5003ST V2.6, Appendix I: 40 bytes, big endian,
// 0x42 0x4D, length 36, sum of bytes 0..37 in bytes 38..39.
struct PmsReading {
  uint16_t pm1Cf1 = 0, pm25Cf1 = 0, pm10Cf1 = 0;
  uint16_t pm1 = 0, pm25 = 0, pm10 = 0;  // Atmospheric, ug/m3.
  uint16_t particles[6] = {};            // Counts per 0.1 L.
  uint16_t hchoUgM3 = 0;                 // Divide by 1000 for mg/m3.
  int16_t temperatureTenths = 0;         // Signed, 0.1 degree C.
  uint16_t humidityTenths = 0;            // 0.1 percent RH.
  uint8_t version = 0, error = 0;
};

class Pms5003stParser {
 public:
  static constexpr size_t FRAME_SIZE = 40;

  bool feed(uint8_t byte, uint32_t nowMs, PmsReading &result) {
    if (used_ && static_cast<uint32_t>(nowMs - lastByteMs_) > 250) {
      used_ = 0;
      ++timeouts;
    }
    lastByteMs_ = nowMs;
    bytes_[used_++] = byte;

    while (used_) {
      if (bytes_[0] != 0x42) {
        discardByte();
        continue;
      }
      if (used_ < 2) return false;
      if (bytes_[1] != 0x4D) {
        discardByte();
        continue;
      }
      if (used_ < 4) return false;
      if (word(2) != 36) {
        ++lengthErrors;
        discardByte();
        continue;
      }
      if (used_ < FRAME_SIZE) return false;

      uint16_t checksum = 0;
      for (size_t i = 0; i < FRAME_SIZE - 2; ++i) checksum += bytes_[i];
      if (checksum != word(38)) {
        ++checksumErrors;
        // Keep the tail: a new valid header may already be inside it.
        discardByte();
        continue;
      }

      result.pm1Cf1 = word(4);
      result.pm25Cf1 = word(6);
      result.pm10Cf1 = word(8);
      result.pm1 = word(10);
      result.pm25 = word(12);
      result.pm10 = word(14);
      for (size_t i = 0; i < 6; ++i) result.particles[i] = word(16 + i * 2);
      result.hchoUgM3 = word(28);
      const uint16_t temperature = word(30);
      result.temperatureTenths = static_cast<int16_t>(
          temperature & 0x8000 ? static_cast<int32_t>(temperature) - 65536
                               : temperature);
      result.humidityTenths = word(32);
      result.version = bytes_[36];
      result.error = bytes_[37];
      used_ = 0;
      ++frames;
      return true;
    }
    return false;
  }

  // For diagnostics only: remains valid until the next feed() call.
  const uint8_t *frameBytes() const { return bytes_; }

  uint32_t frames = 0;
  uint32_t checksumErrors = 0;
  uint32_t lengthErrors = 0;
  uint32_t timeouts = 0;

 private:
  uint16_t word(size_t offset) const {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes_[offset]) << 8) |
                                 bytes_[offset + 1]);
  }

  void discardByte() {
    --used_;
    memmove(bytes_, bytes_ + 1, used_);
  }

  uint8_t bytes_[FRAME_SIZE] = {};
  size_t used_ = 0;
  uint32_t lastByteMs_ = 0;
};
